//==================================================================================
// BSD 2-Clause License
//
// Copyright (c) 2014-2022, NJIT, Duality Technologies Inc. and other contributors
//
// All rights reserved.
//
// Author TPOC: contact@openfhe.org
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//==================================================================================

/*
  Control for encryption operations
 */

#include "cryptocontext.h"

#include "key/privatekey.h"
#include "key/publickey.h"
#include "math/chebyshev.h"
#include "schemerns/rns-scheme.h"
#include "scheme/ckksrns/ckksrns-cryptoparameters.h"

namespace lbcrypto {

template <typename Element>
void CryptoContextImpl<Element>::SetKSTechniqueInScheme() {
    // check if the scheme is an RNS scheme
    auto schemeRNSPtr = dynamic_cast<SchemeRNS*>(&(*scheme));
    if (schemeRNSPtr != nullptr) {
        // check if the parameter object is RNS-based
        auto elPtr = dynamic_cast<const CryptoParametersRNS*>(&(*params));
        if (elPtr != nullptr) {
            schemeRNSPtr->SetKeySwitchingTechnique(elPtr->GetKeySwitchTechnique());
            return;
        }
        OPENFHE_THROW(type_error, "Cannot set KeySwitchingTechnique as the parameter object is not RNS-based");
    }
}

/////////////////////////////////////////
// SHE MULTIPLICATION
/////////////////////////////////////////

template <typename Element>
void CryptoContextImpl<Element>::EvalMultKeyGen(const PrivateKey<Element> key) {
    if (key == nullptr || Mismatched(key->GetCryptoContext()))
        OPENFHE_THROW(config_error, "Key passed to EvalMultKeyGen were not generated with this crypto context");

    EvalKey<Element> k = GetScheme()->EvalMultKeyGen(key);

    GetAllEvalMultKeys()[k->GetKeyTag()] = {k};
}

template <typename Element>
void CryptoContextImpl<Element>::EvalMultKeysGen(const PrivateKey<Element> key) {
    if (key == nullptr || Mismatched(key->GetCryptoContext()))
        OPENFHE_THROW(config_error, "Key passed to EvalMultsKeyGen were not generated with this crypto context");

    const std::vector<EvalKey<Element>>& evalKeys = GetScheme()->EvalMultKeysGen(key);

    GetAllEvalMultKeys()[evalKeys[0]->GetKeyTag()] = evalKeys;
}

template <typename Element>
const std::vector<EvalKey<Element>>& CryptoContextImpl<Element>::GetEvalMultKeyVector(const std::string& keyID) {
    auto ekv = GetAllEvalMultKeys().find(keyID);
    if (ekv == GetAllEvalMultKeys().end())
        OPENFHE_THROW(not_available_error,
                      "You need to use EvalMultKeyGen so that you have an "
                      "EvalMultKey available for this ID");
    return ekv->second;
}

template <typename Element>
std::map<std::string, std::vector<EvalKey<Element>>>& CryptoContextImpl<Element>::GetAllEvalMultKeys() {
    return evalMultKeyMap();
}

template <typename Element>
void CryptoContextImpl<Element>::ClearEvalMultKeys() {
    GetAllEvalMultKeys().clear();
}

/**
 * ClearEvalMultKeys - flush EvalMultKey cache for a given id
 * @param id
 */
template <typename Element>
void CryptoContextImpl<Element>::ClearEvalMultKeys(const std::string& id) {
    auto kd = GetAllEvalMultKeys().find(id);
    if (kd != GetAllEvalMultKeys().end())
        GetAllEvalMultKeys().erase(kd);
}

/**
 * ClearEvalMultKeys - flush EvalMultKey cache for a given context
 * @param cc
 */
template <typename Element>
void CryptoContextImpl<Element>::ClearEvalMultKeys(const CryptoContext<Element> cc) {
    for (auto it = GetAllEvalMultKeys().begin(); it != GetAllEvalMultKeys().end();) {
        if (it->second[0]->GetCryptoContext() == cc) {
            it = GetAllEvalMultKeys().erase(it);
        }
        else {
            ++it;
        }
    }
}

template <typename Element>
void CryptoContextImpl<Element>::InsertEvalMultKey(const std::vector<EvalKey<Element>>& vectorToInsert) {
    GetAllEvalMultKeys()[vectorToInsert[0]->GetKeyTag()] = vectorToInsert;
}

/////////////////////////////////////////
// ADVANCED SHE
/////////////////////////////////////////

template <typename Element>
void CryptoContextImpl<Element>::EvalSumKeyGen(const PrivateKey<Element> privateKey,
                                               const PublicKey<Element> publicKey) {
    if (privateKey == nullptr || Mismatched(privateKey->GetCryptoContext())) {
        OPENFHE_THROW(config_error,
                      "Private key passed to EvalSumKeyGen were not generated "
                      "with this crypto context");
    }

    if (publicKey != nullptr && privateKey->GetKeyTag() != publicKey->GetKeyTag()) {
        OPENFHE_THROW(config_error, "Public key passed to EvalSumKeyGen does not match private key");
    }

    auto evalKeys = GetScheme()->EvalSumKeyGen(privateKey, publicKey);

    GetAllEvalSumKeys()[privateKey->GetKeyTag()] = evalKeys;
}

template <typename Element>
std::shared_ptr<std::map<usint, EvalKey<Element>>> CryptoContextImpl<Element>::EvalSumRowsKeyGen(
    const PrivateKey<Element> privateKey, const PublicKey<Element> publicKey, usint rowSize, usint subringDim) {
    if (privateKey == nullptr || Mismatched(privateKey->GetCryptoContext())) {
        OPENFHE_THROW(config_error,
                      "Private key passed to EvalSumKeyGen were not generated "
                      "with this crypto context");
    }

    if (publicKey != nullptr && privateKey->GetKeyTag() != publicKey->GetKeyTag()) {
        OPENFHE_THROW(config_error, "Public key passed to EvalSumKeyGen does not match private key");
    }

    auto evalKeys = GetScheme()->EvalSumRowsKeyGen(privateKey, publicKey, rowSize, subringDim);

    return evalKeys;
}

template <typename Element>
std::shared_ptr<std::map<usint, EvalKey<Element>>> CryptoContextImpl<Element>::EvalSumColsKeyGen(
    const PrivateKey<Element> privateKey, const PublicKey<Element> publicKey) {
    if (privateKey == nullptr || Mismatched(privateKey->GetCryptoContext())) {
        OPENFHE_THROW(config_error,
                      "Private key passed to EvalSumKeyGen were not generated "
                      "with this crypto context");
    }

    if (publicKey != nullptr && privateKey->GetKeyTag() != publicKey->GetKeyTag()) {
        OPENFHE_THROW(config_error, "Public key passed to EvalSumKeyGen does not match private key");
    }

    auto evalKeys = GetScheme()->EvalSumColsKeyGen(privateKey, publicKey);

    return evalKeys;
}

template <typename Element>
const std::map<usint, EvalKey<Element>>& CryptoContextImpl<Element>::GetEvalSumKeyMap(const std::string& keyID) {
    auto ekv = GetAllEvalSumKeys().find(keyID);
    if (ekv == GetAllEvalSumKeys().end())
        OPENFHE_THROW(not_available_error,
                      "You need to use EvalSumKeyGen so that you have EvalSumKeys "
                      "available for this ID");
    return *ekv->second;
}

template <typename Element>
std::map<std::string, std::shared_ptr<std::map<usint, EvalKey<Element>>>>&
CryptoContextImpl<Element>::GetAllEvalSumKeys() {
    return evalSumKeyMap();
}

template <typename Element>
void CryptoContextImpl<Element>::ClearEvalSumKeys() {
    GetAllEvalSumKeys().clear();
}

/**
 * ClearEvalMultKeys - flush EvalMultKey cache for a given id
 * @param id
 */
template <typename Element>
void CryptoContextImpl<Element>::ClearEvalSumKeys(const std::string& id) {
    auto kd = GetAllEvalSumKeys().find(id);
    if (kd != GetAllEvalSumKeys().end())
        GetAllEvalSumKeys().erase(kd);
}

/**
 * ClearEvalMultKeys - flush EvalMultKey cache for a given context
 * @param cc
 */
template <typename Element>
void CryptoContextImpl<Element>::ClearEvalSumKeys(const CryptoContext<Element> cc) {
    for (auto it = GetAllEvalSumKeys().begin(); it != GetAllEvalSumKeys().end();) {
        if (it->second->begin()->second->GetCryptoContext() == cc) {
            it = GetAllEvalSumKeys().erase(it);
        }
        else {
            ++it;
        }
    }
}

template <typename Element>
void CryptoContextImpl<Element>::InsertEvalSumKey(
    const std::shared_ptr<std::map<usint, EvalKey<Element>>> mapToInsert) {
    // find the tag
    if (!mapToInsert->empty()) {
        auto onekey                                      = mapToInsert->begin();
        GetAllEvalSumKeys()[onekey->second->GetKeyTag()] = mapToInsert;
    }
}

/////////////////////////////////////////
// SHE AUTOMORPHISM
/////////////////////////////////////////

template <typename Element>
void CryptoContextImpl<Element>::EvalAtIndexKeyGen(const PrivateKey<Element> privateKey,
                                                   const std::vector<int32_t>& indexList,
                                                   const PublicKey<Element> publicKey) {
    if (privateKey == nullptr || Mismatched(privateKey->GetCryptoContext())) {
        OPENFHE_THROW(config_error,
                      "Private key passed to EvalAtIndexKeyGen were not generated "
                      "with this crypto context");
    }

    if (publicKey != nullptr && privateKey->GetKeyTag() != publicKey->GetKeyTag()) {
        OPENFHE_THROW(config_error, "Public key passed to EvalAtIndexKeyGen does not match private key");
    }

    auto evalKeys = GetScheme()->EvalAtIndexKeyGen(publicKey, privateKey, indexList);

    auto ekv = GetAllEvalAutomorphismKeys().find(privateKey->GetKeyTag());
    if (ekv == GetAllEvalAutomorphismKeys().end()) {
        GetAllEvalAutomorphismKeys()[privateKey->GetKeyTag()] = evalKeys;
    }
    else {
        auto& currRotMap = GetEvalAutomorphismKeyMap(privateKey->GetKeyTag());
        auto iterRowKeys = evalKeys->begin();
        while (iterRowKeys != evalKeys->end()) {
            auto idx = iterRowKeys->first;
            // Search current rotation key map and add key
            // only if it doesn't exist
            if (currRotMap.find(idx) == currRotMap.end()) {
                currRotMap.insert(*iterRowKeys);
            }
            iterRowKeys++;
        }
    }

    //  evalAutomorphismKeyMap()[privateKey->GetKeyTag()] = evalKeys;
}

template <typename Element>
std::map<usint, EvalKey<Element>>& CryptoContextImpl<Element>::GetEvalAutomorphismKeyMap(const std::string& keyID) {
    auto ekv = evalAutomorphismKeyMap().find(keyID);
    if (ekv == evalAutomorphismKeyMap().end())
        OPENFHE_THROW(not_available_error,
                      "You need to use EvalAutomorphismKeyGen so that you have "
                      "EvalAutomorphismKeys available for this ID");
    return *ekv->second;
}

template <typename Element>
std::map<std::string, std::shared_ptr<std::map<usint, EvalKey<Element>>>>&
CryptoContextImpl<Element>::GetAllEvalAutomorphismKeys() {
    return evalAutomorphismKeyMap();
}

template <typename Element>
void CryptoContextImpl<Element>::ClearEvalAutomorphismKeys() {
    evalAutomorphismKeyMap().clear();
}

/**
 * ClearEvalAutomorphismKeys - flush EvalAutomorphismKey cache for a given id
 * @param id
 */
template <typename Element>
void CryptoContextImpl<Element>::ClearEvalAutomorphismKeys(const std::string& id) {
    auto kd = evalAutomorphismKeyMap().find(id);
    if (kd != evalAutomorphismKeyMap().end())
        evalAutomorphismKeyMap().erase(kd);
}

/**
 * ClearEvalAutomorphismKeys - flush EvalAutomorphismKey cache for a given
 * context
 * @param cc
 */
template <typename Element>
void CryptoContextImpl<Element>::ClearEvalAutomorphismKeys(const CryptoContext<Element> cc) {
    for (auto it = evalAutomorphismKeyMap().begin(); it != evalAutomorphismKeyMap().end();) {
        if (it->second->begin()->second->GetCryptoContext() == cc) {
            it = evalAutomorphismKeyMap().erase(it);
        }
        else {
            ++it;
        }
    }
}

template <typename Element>
void CryptoContextImpl<Element>::InsertEvalAutomorphismKey(
    const std::shared_ptr<std::map<usint, EvalKey<Element>>> mapToInsert) {
    // find the tag
    auto onekey                                           = mapToInsert->begin();
    evalAutomorphismKeyMap()[onekey->second->GetKeyTag()] = mapToInsert;
}

template <typename Element>
Ciphertext<Element> CryptoContextImpl<Element>::EvalSum(ConstCiphertext<Element> ciphertext, usint batchSize) const {
    if (ciphertext == nullptr || Mismatched(ciphertext->GetCryptoContext()))
        OPENFHE_THROW(config_error,
                      "Information passed to EvalSum was not generated with this "
                      "crypto context");

    auto evalSumKeys = CryptoContextImpl<Element>::GetEvalSumKeyMap(ciphertext->GetKeyTag());
    auto rv          = GetScheme()->EvalSum(ciphertext, batchSize, evalSumKeys);
    return rv;
}

template <typename Element>
Ciphertext<Element> CryptoContextImpl<Element>::EvalSumRows(ConstCiphertext<Element> ciphertext, usint rowSize,
                                                            const std::map<usint, EvalKey<Element>>& evalSumKeys,
                                                            usint subringDim) const {
    if (ciphertext == nullptr || Mismatched(ciphertext->GetCryptoContext()))
        OPENFHE_THROW(config_error,
                      "Information passed to EvalSum was not generated with this "
                      "crypto context");

    auto rv = GetScheme()->EvalSumRows(ciphertext, rowSize, evalSumKeys, subringDim);
    return rv;
}

template <typename Element>
Ciphertext<Element> CryptoContextImpl<Element>::EvalSumCols(
    ConstCiphertext<Element> ciphertext, usint rowSize,
    const std::map<usint, EvalKey<Element>>& evalSumKeysRight) const {
    if (ciphertext == nullptr || Mismatched(ciphertext->GetCryptoContext()))
        OPENFHE_THROW(config_error,
                      "Information passed to EvalSum was not generated with this "
                      "crypto context");

    auto evalSumKeys = CryptoContextImpl<Element>::GetEvalSumKeyMap(ciphertext->GetKeyTag());

    auto rv = GetScheme()->EvalSumCols(ciphertext, rowSize, evalSumKeys, evalSumKeysRight);
    return rv;
}

template <typename Element>
Ciphertext<Element> CryptoContextImpl<Element>::EvalAtIndex(ConstCiphertext<Element> ciphertext, int32_t index) const {
    if (ciphertext == nullptr || Mismatched(ciphertext->GetCryptoContext()))
        OPENFHE_THROW(config_error,
                      "Information passed to EvalAtIndex was not generated with "
                      "this crypto context");

    // If the index is zero, no rotation is needed, copy the ciphertext and return
    // This is done after the keyMap so that it is protected if there's not a
    // valid key.
    if (0 == index) {
        auto rv = ciphertext->Clone();
        return rv;
    }

    auto evalAutomorphismKeys = CryptoContextImpl<Element>::GetEvalAutomorphismKeyMap(ciphertext->GetKeyTag());

    auto rv = GetScheme()->EvalAtIndex(ciphertext, index, evalAutomorphismKeys);
    return rv;
}

template <typename Element>
Ciphertext<Element> CryptoContextImpl<Element>::EvalMerge(
    const std::vector<Ciphertext<Element>>& ciphertextVector) const {
    if (ciphertextVector[0] == nullptr || Mismatched(ciphertextVector[0]->GetCryptoContext()))
        OPENFHE_THROW(config_error,
                      "Information passed to EvalMerge was not generated with "
                      "this crypto context");

    auto evalAutomorphismKeys = CryptoContextImpl<Element>::GetEvalAutomorphismKeyMap(ciphertextVector[0]->GetKeyTag());

    auto rv = GetScheme()->EvalMerge(ciphertextVector, evalAutomorphismKeys);

    return rv;
}

template <typename Element>
Ciphertext<Element> CryptoContextImpl<Element>::EvalInnerProduct(ConstCiphertext<Element> ct1,
                                                                 ConstCiphertext<Element> ct2, usint batchSize) const {
    if (ct1 == nullptr || ct2 == nullptr || ct1->GetKeyTag() != ct2->GetKeyTag() || Mismatched(ct1->GetCryptoContext()))
        OPENFHE_THROW(config_error,
                      "Information passed to EvalInnerProduct was not generated "
                      "with this crypto context");

    auto evalSumKeys = CryptoContextImpl<Element>::GetEvalSumKeyMap(ct1->GetKeyTag());
    auto ek          = GetEvalMultKeyVector(ct1->GetKeyTag());

    auto rv = GetScheme()->EvalInnerProduct(ct1, ct2, batchSize, evalSumKeys, ek[0]);
    return rv;
}

template <typename Element>
Ciphertext<Element> CryptoContextImpl<Element>::EvalInnerProduct(ConstCiphertext<Element> ct1, ConstPlaintext ct2,
                                                                 usint batchSize) const {
    if (ct1 == nullptr || ct2 == nullptr || Mismatched(ct1->GetCryptoContext()))
        OPENFHE_THROW(config_error,
                      "Information passed to EvalInnerProduct was not generated "
                      "with this crypto context");

    auto evalSumKeys = CryptoContextImpl<Element>::GetEvalSumKeyMap(ct1->GetKeyTag());

    auto rv = GetScheme()->EvalInnerProduct(ct1, ct2, batchSize, evalSumKeys);
    return rv;
}

template <typename Element>
Plaintext CryptoContextImpl<Element>::GetPlaintextForDecrypt(PlaintextEncodings pte, std::shared_ptr<ParmType> evp,
                                                             EncodingParams ep) {
    auto vp = std::make_shared<typename NativePoly::Params>(evp->GetCyclotomicOrder(), ep->GetPlaintextModulus(), 1);

    if (pte == CKKS_PACKED_ENCODING)
        return PlaintextFactory::MakePlaintext(pte, evp, ep);

    return PlaintextFactory::MakePlaintext(pte, vp, ep);
}

template <typename Element>
DecryptResult CryptoContextImpl<Element>::Decrypt(ConstCiphertext<Element> ciphertext,
                                                  const PrivateKey<Element> privateKey, Plaintext* plaintext) {
    if (ciphertext == nullptr)
        OPENFHE_THROW(config_error, "ciphertext passed to Decrypt is empty");
    if (plaintext == nullptr)
        OPENFHE_THROW(config_error, "plaintext passed to Decrypt is empty");
    if (privateKey == nullptr || Mismatched(privateKey->GetCryptoContext()))
        OPENFHE_THROW(config_error,
                      "Information passed to Decrypt was not generated with "
                      "this crypto context");

    // determine which type of plaintext that you need to decrypt into
    // Plaintext decrypted =
    // GetPlaintextForDecrypt(ciphertext->GetEncodingType(),
    // this->GetElementParams(), this->GetEncodingParams());
    Plaintext decrypted = GetPlaintextForDecrypt(ciphertext->GetEncodingType(),
                                                 ciphertext->GetElements()[0].GetParams(), this->GetEncodingParams());

    DecryptResult result;

    if ((ciphertext->GetEncodingType() == CKKS_PACKED_ENCODING) && (typeid(Element) != typeid(NativePoly))) {
        result = GetScheme()->Decrypt(ciphertext, privateKey, &decrypted->GetElement<Poly>());
    }
    else {
        result = GetScheme()->Decrypt(ciphertext, privateKey, &decrypted->GetElement<NativePoly>());
    }

    if (result.isValid == false)  // TODO (dsuponit): why don't we throw an exception here?
        return result;

    decrypted->SetScalingFactorInt(result.scalingFactorInt);

    if (ciphertext->GetEncodingType() == CKKS_PACKED_ENCODING) {
        auto decryptedCKKS = std::dynamic_pointer_cast<CKKSPackedEncoding>(decrypted);
        decryptedCKKS->SetNoiseScaleDeg(ciphertext->GetNoiseScaleDeg());
        decryptedCKKS->SetLevel(ciphertext->GetLevel());
        decryptedCKKS->SetScalingFactor(ciphertext->GetScalingFactor());
        decryptedCKKS->SetSlots(ciphertext->GetSlots());

        const auto cryptoParamsCKKS = std::dynamic_pointer_cast<CryptoParametersRNS>(this->GetCryptoParameters());

        decryptedCKKS->Decode(ciphertext->GetNoiseScaleDeg(), ciphertext->GetScalingFactor(),
                              cryptoParamsCKKS->GetScalingTechnique());
    }
    else {
        decrypted->Decode();
    }

    *plaintext = std::move(decrypted);
    return result;
}

template <typename Element>
DecryptResult CryptoContextImpl<Element>::MultipartyDecryptFusion(
    const std::vector<Ciphertext<Element>>& partialCiphertextVec, Plaintext* plaintext) const {
    DecryptResult result;

    // Make sure we're processing ciphertexts.
    size_t last_ciphertext = partialCiphertextVec.size();
    if (last_ciphertext < 1)
        return result;

    for (size_t i = 0; i < last_ciphertext; i++) {
        if (partialCiphertextVec[i] == nullptr || Mismatched(partialCiphertextVec[i]->GetCryptoContext()))
            OPENFHE_THROW(config_error,
                          "A ciphertext passed to MultipartyDecryptFusion was not "
                          "generated with this crypto context");
        if (partialCiphertextVec[i]->GetEncodingType() != partialCiphertextVec[0]->GetEncodingType())
            OPENFHE_THROW(type_error,
                          "Ciphertexts passed to MultipartyDecryptFusion have "
                          "mismatched encoding types");
    }

    // determine which type of plaintext that you need to decrypt into
    Plaintext decrypted =
        GetPlaintextForDecrypt(partialCiphertextVec[0]->GetEncodingType(),
                               partialCiphertextVec[0]->GetElements()[0].GetParams(), this->GetEncodingParams());

    if ((partialCiphertextVec[0]->GetEncodingType() == CKKS_PACKED_ENCODING) && (typeid(Element) != typeid(NativePoly)))
        result = GetScheme()->MultipartyDecryptFusion(partialCiphertextVec, &decrypted->GetElement<Poly>());
    else
        result = GetScheme()->MultipartyDecryptFusion(partialCiphertextVec, &decrypted->GetElement<NativePoly>());

    if (result.isValid == false)
        return result;

    if (partialCiphertextVec[0]->GetEncodingType() == CKKS_PACKED_ENCODING) {
        auto decryptedCKKS = std::dynamic_pointer_cast<CKKSPackedEncoding>(decrypted);
        decryptedCKKS->SetSlots(partialCiphertextVec[0]->GetSlots());
        const auto cryptoParamsCKKS = std::dynamic_pointer_cast<CryptoParametersRNS>(this->GetCryptoParameters());
        decryptedCKKS->Decode(partialCiphertextVec[0]->GetNoiseScaleDeg(), partialCiphertextVec[0]->GetScalingFactor(),
                              cryptoParamsCKKS->GetScalingTechnique());
    }
    else {
        decrypted->Decode();
    }

    *plaintext = std::move(decrypted);

    return result;
}

//------------------------------------------------------------------------------
// Advanced SHE CHEBYSHEV SERIES EXAMPLES
//------------------------------------------------------------------------------

template <typename Element>
Ciphertext<Element> CryptoContextImpl<Element>::EvalChebyshevFunction(std::function<double(double)> func,
                                                                      ConstCiphertext<Element> ciphertext, double a,
                                                                      double b, uint32_t degree) const {
    std::vector<double> coefficients = EvalChebyshevCoefficients(func, a, b, degree);
    return EvalChebyshevSeries(ciphertext, coefficients, a, b);
}

template <typename Element>
Ciphertext<Element> CryptoContextImpl<Element>::EvalSin(ConstCiphertext<Element> ciphertext, double a, double b,
                                                        uint32_t degree) const {
    return EvalChebyshevFunction([](double x) -> double { return std::sin(x); }, ciphertext, a, b, degree);
}

template <typename Element>
Ciphertext<Element> CryptoContextImpl<Element>::EvalCos(ConstCiphertext<Element> ciphertext, double a, double b,
                                                        uint32_t degree) const {
    return EvalChebyshevFunction([](double x) -> double { return std::cos(x); }, ciphertext, a, b, degree);
}

template <typename Element>
Ciphertext<Element> CryptoContextImpl<Element>::EvalLogistic(ConstCiphertext<Element> ciphertext, double a, double b,
                                                             uint32_t degree) const {
    return EvalChebyshevFunction([](double x) -> double { return 1 / (1 + std::exp(-x)); }, ciphertext, a, b, degree);
}

template <typename Element>
Ciphertext<Element> CryptoContextImpl<Element>::EvalDivide(ConstCiphertext<Element> ciphertext, double a, double b,
                                                           uint32_t degree) const {
    return EvalChebyshevFunction([](double x) -> double { return 1 / x; }, ciphertext, a, b, degree);
}

//------------------------------------------------------------------------------
// Advanced SHE LINEAR TRANSFORMATION
//------------------------------------------------------------------------------

// TODO Andrey add from bootstrapping

//------------------------------------------------------------------------------
// FHE Bootstrap Methods
//------------------------------------------------------------------------------

template <typename Element>
void CryptoContextImpl<Element>::EvalBootstrapSetup(std::vector<uint32_t> levelBudget, std::vector<uint32_t> dim1,
                                                    uint32_t numSlots, uint32_t correctionFactor) {
    GetScheme()->EvalBootstrapSetup(*this, levelBudget, dim1, numSlots, correctionFactor);
}

template <typename Element>
void CryptoContextImpl<Element>::EvalBootstrapKeyGen(const PrivateKey<Element> privateKey, uint32_t slots) {
    if (privateKey == NULL || this->Mismatched(privateKey->GetCryptoContext())) {
        OPENFHE_THROW(config_error,
                      "Private key passed to EvalBootstapKeyGen was not generated with this crypto context");
    }

    auto evalKeys = GetScheme()->EvalBootstrapKeyGen(privateKey, slots);

    auto ekv = GetAllEvalAutomorphismKeys().find(privateKey->GetKeyTag());
    if (ekv == GetAllEvalAutomorphismKeys().end()) {
        GetAllEvalAutomorphismKeys()[privateKey->GetKeyTag()] = evalKeys;
    }
    else {
        auto& currRotMap = GetEvalAutomorphismKeyMap(privateKey->GetKeyTag());
        auto iterRowKeys = evalKeys->begin();
        while (iterRowKeys != evalKeys->end()) {
            auto idx = iterRowKeys->first;
            // Search current rotation key map and add key
            // only if it doesn't exist
            if (currRotMap.find(idx) == currRotMap.end()) {
                currRotMap.insert(*iterRowKeys);
            }
            iterRowKeys++;
        }
    }
}

template <typename Element>
Ciphertext<Element> CryptoContextImpl<Element>::EvalBootstrap(ConstCiphertext<Element> ciphertext,
                                                              uint32_t numIterations, uint32_t precision) const {
    return GetScheme()->EvalBootstrap(ciphertext, numIterations, precision);
}

//------------------------------------------------------------------------------
// SCHEMESWITCHING Methods
//------------------------------------------------------------------------------

template <typename Element>
std::pair<BinFHEContext, LWEPrivateKey> CryptoContextImpl<Element>::EvalCKKStoFHEWSetup(SecurityLevel sl, bool arbFunc,
                                                                                        uint32_t logQ, bool dynamic,
                                                                                        uint32_t numSlotsCKKS) {
    return GetScheme()->EvalCKKStoFHEWSetup(*this, sl, arbFunc, logQ, dynamic, numSlotsCKKS);
}

template <typename Element>
void CryptoContextImpl<Element>::EvalCKKStoFHEWKeyGen(const KeyPair<Element>& keyPair, const LWEPrivateKey& lwesk,
                                                      uint32_t dim1) {
    if (keyPair.secretKey == NULL || this->Mismatched(keyPair.secretKey->GetCryptoContext())) {  // Add test for lwesk?
        OPENFHE_THROW(config_error,
                      "Private key passed to EvalCKKStoFHEWKeyGen was not generated with this crypto context");
    }
    auto evalKeys = GetScheme()->EvalCKKStoFHEWKeyGen(keyPair, lwesk, dim1);

    auto ekv = GetAllEvalAutomorphismKeys().find(keyPair.secretKey->GetKeyTag());
    if (ekv == GetAllEvalAutomorphismKeys().end()) {
        GetAllEvalAutomorphismKeys()[keyPair.secretKey->GetKeyTag()] = evalKeys;
    }
    else {
        auto& currRotMap = GetEvalAutomorphismKeyMap(keyPair.secretKey->GetKeyTag());
        auto iterRowKeys = evalKeys->begin();
        while (iterRowKeys != evalKeys->end()) {
            auto idx = iterRowKeys->first;
            // Search current rotation key map and add key
            // only if it doesn't exist
            if (currRotMap.find(idx) == currRotMap.end()) {
                currRotMap.insert(*iterRowKeys);
            }
            iterRowKeys++;
        }
    }
}

template <typename Element>
void CryptoContextImpl<Element>::EvalCKKStoFHEWPrecompute(double scale, uint32_t dim1) {
    GetScheme()->EvalCKKStoFHEWPrecompute(*this, scale, dim1);
}

template <typename Element>
std::vector<std::shared_ptr<LWECiphertextImpl>> CryptoContextImpl<Element>::EvalCKKStoFHEW(
    ConstCiphertext<Element> ciphertext, uint32_t numCtxts) {
    if (ciphertext == nullptr)
        OPENFHE_THROW(config_error, "ciphertext passed to EvalCKKStoFHEW is empty");
    return GetScheme()->EvalCKKStoFHEW(ciphertext, numCtxts);
}

template <typename Element>
void CryptoContextImpl<Element>::EvalFHEWtoCKKSSetup(const BinFHEContext& ccLWE, uint32_t numSlotsCKKS, uint32_t logQ) {
    GetScheme()->EvalFHEWtoCKKSSetup(*this, ccLWE, numSlotsCKKS, logQ);
}

template <typename Element>
void CryptoContextImpl<Element>::EvalFHEWtoCKKSKeyGen(const KeyPair<Element>& keyPair, const LWEPrivateKey& lwesk,
                                                      uint32_t numSlots) {
    if (keyPair.secretKey == NULL || this->Mismatched(keyPair.secretKey->GetCryptoContext())) {
        OPENFHE_THROW(config_error,
                      "Private key passed to EvalFHEWtoCKKSKeyGen was not generated with this crypto context");
    }
    auto evalKeys = GetScheme()->EvalFHEWtoCKKSKeyGen(keyPair, lwesk, numSlots);

    auto ekv = GetAllEvalAutomorphismKeys().find(keyPair.secretKey->GetKeyTag());
    if (ekv == GetAllEvalAutomorphismKeys().end()) {
        GetAllEvalAutomorphismKeys()[keyPair.secretKey->GetKeyTag()] = evalKeys;
    }
    else {
        auto& currRotMap = GetEvalAutomorphismKeyMap(keyPair.secretKey->GetKeyTag());
        auto iterRowKeys = evalKeys->begin();
        while (iterRowKeys != evalKeys->end()) {
            auto idx = iterRowKeys->first;
            // Search current rotation key map and add key
            // only if it doesn't exist
            if (currRotMap.find(idx) == currRotMap.end()) {
                currRotMap.insert(*iterRowKeys);
            }
            iterRowKeys++;
        }
    }
}

template <typename Element>
Ciphertext<Element> CryptoContextImpl<Element>::EvalFHEWtoCKKS(
    std::vector<std::shared_ptr<LWECiphertextImpl>>& LWECiphertexts, double prescale, uint32_t numCtxts,
    uint32_t numSlots, uint32_t p, double pmin, double pmax) const {
    return GetScheme()->EvalFHEWtoCKKS(LWECiphertexts, prescale, numCtxts, numSlots, p, pmin, pmax);
}

template <typename Element>
Ciphertext<Element> CryptoContextImpl<Element>::EvalFHEWtoCKKSPrototype(
    std::vector<std::shared_ptr<LWECiphertextImpl>>& LWECiphertexts, uint32_t dim1_FC, double scale, uint32_t numSlots,
    double pmin, double pmax) const {
    return GetScheme()->EvalFHEWtoCKKSPrototype(LWECiphertexts, dim1_FC, scale, numSlots, pmin, pmax);
}

template <typename Element>
std::pair<BinFHEContext, LWEPrivateKey> CryptoContextImpl<Element>::EvalSchemeSwitchingSetup(
    SecurityLevel sl, bool arbFunc, uint32_t logQ, bool dynamic, uint32_t numSlotsCKKS) {
    return GetScheme()->EvalSchemeSwitchingSetup(*this, sl, arbFunc, logQ, dynamic, numSlotsCKKS);
}

template <typename Element>
void CryptoContextImpl<Element>::EvalSchemeSwitchingKeyGen(const KeyPair<Element>& keyPair, LWEPrivateKey& lwesk,
                                                           uint32_t dim1CF, uint32_t dim1FC, uint32_t numValues,
                                                           bool oneHot) {
    if (keyPair.secretKey == NULL || this->Mismatched(keyPair.secretKey->GetCryptoContext())) {  // Add test for lwesk?
        OPENFHE_THROW(config_error,
                      "Private key passed to EvalSchemeSwitchingKeyGen was not generated with this crypto context");
    }
    auto evalKeys = GetScheme()->EvalSchemeSwitchingKeyGen(keyPair, lwesk, dim1CF, dim1FC, numValues, oneHot);

    auto ekv = GetAllEvalAutomorphismKeys().find(keyPair.secretKey->GetKeyTag());
    if (ekv == GetAllEvalAutomorphismKeys().end()) {
        GetAllEvalAutomorphismKeys()[keyPair.secretKey->GetKeyTag()] = evalKeys;
    }
    else {
        auto& currRotMap = GetEvalAutomorphismKeyMap(keyPair.secretKey->GetKeyTag());
        auto iterRowKeys = evalKeys->begin();
        while (iterRowKeys != evalKeys->end()) {
            auto idx = iterRowKeys->first;
            // Search current rotation key map and add key
            // only if it doesn't exist
            if (currRotMap.find(idx) == currRotMap.end()) {
                currRotMap.insert(*iterRowKeys);
            }
            iterRowKeys++;
        }
    }
}

template <typename Element>
void CryptoContextImpl<Element>::EvalCompareSSPrecompute(uint32_t pLWE, uint32_t initLevel, double scaleSign) {
    GetScheme()->EvalCompareSSPrecompute(*this, pLWE, initLevel, scaleSign);
}

template <typename Element>
Ciphertext<Element> CryptoContextImpl<Element>::EvalCompareSchemeSwitching(ConstCiphertext<Element> ciphertext1,
                                                                           ConstCiphertext<Element> ciphertext2,
                                                                           uint32_t numCtxts, uint32_t numSlots,
                                                                           uint32_t pLWE, double scaleSign) {
    if (ciphertext1 == nullptr || ciphertext2 == nullptr)
        OPENFHE_THROW(config_error, "ciphertexts passed to EvalCompareSchemeSwitching are empty");
    if (Mismatched(ciphertext1->GetCryptoContext()) || Mismatched(ciphertext2->GetCryptoContext()))
        OPENFHE_THROW(config_error,
                      "A ciphertext passed to EvalCompareSchemeSwitching was not "
                      "generated with this crypto context");
    return GetScheme()->EvalCompareSchemeSwitching(ciphertext1, ciphertext2, numCtxts, numSlots, pLWE, scaleSign);
}

template <typename Element>
std::vector<Ciphertext<Element>> CryptoContextImpl<Element>::EvalMinSchemeSwitching(ConstCiphertext<Element> ciphertext,
                                                                                    PublicKey<Element> publicKey,
                                                                                    uint32_t numValues,
                                                                                    uint32_t numSlots, bool oneHot,
                                                                                    uint32_t pLWE, double scaleSign) {
    if (ciphertext == nullptr)
        OPENFHE_THROW(config_error, "ciphertexts passed to EvalMinSchemeSwitching are empty");
    if (Mismatched(ciphertext->GetCryptoContext()))
        OPENFHE_THROW(config_error,
                      "The ciphertext passed to EvalMinSchemeSwitching was not "
                      "generated with this crypto context");
    return GetScheme()->EvalMinSchemeSwitching(ciphertext, publicKey, numValues, numSlots, oneHot, pLWE, scaleSign);
}

template <typename Element>
std::vector<Ciphertext<Element>> CryptoContextImpl<Element>::EvalMaxSchemeSwitching(ConstCiphertext<Element> ciphertext,
                                                                                    PublicKey<Element> publicKey,
                                                                                    uint32_t numValues,
                                                                                    uint32_t numSlots, bool oneHot,
                                                                                    uint32_t pLWE, double scaleSign) {
    if (ciphertext == nullptr)
        OPENFHE_THROW(config_error, "ciphertexts passed to EvalMaxSchemeSwitching are empty");
    if (Mismatched(ciphertext->GetCryptoContext()))
        OPENFHE_THROW(config_error,
                      "The ciphertext passed to EvalMinSchemeSwitching was not "
                      "generated with this crypto context");
    return GetScheme()->EvalMaxSchemeSwitching(ciphertext, publicKey, numValues, numSlots, oneHot, pLWE, scaleSign);
}

}  // namespace lbcrypto

// the code below is from cryptocontext-impl.cpp
namespace lbcrypto {

template <>
Plaintext CryptoContextImpl<DCRTPoly>::GetPlaintextForDecrypt(PlaintextEncodings pte, std::shared_ptr<ParmType> evp,
                                                              EncodingParams ep) {
    if ((pte == CKKS_PACKED_ENCODING) && (evp->GetParams().size() > 1)) {
        auto vp = std::make_shared<typename Poly::Params>(evp->GetCyclotomicOrder(), ep->GetPlaintextModulus(), 1);
        return PlaintextFactory::MakePlaintext(pte, vp, ep);
    }
    else {
        auto vp =
            std::make_shared<typename NativePoly::Params>(evp->GetCyclotomicOrder(), ep->GetPlaintextModulus(), 1);
        return PlaintextFactory::MakePlaintext(pte, vp, ep);
    }
}

template <>
DecryptResult CryptoContextImpl<DCRTPoly>::Decrypt(ConstCiphertext<DCRTPoly> ciphertext,
                                                   const PrivateKey<DCRTPoly> privateKey, Plaintext* plaintext) {
    if (ciphertext == nullptr)
        OPENFHE_THROW(config_error, "ciphertext passed to Decrypt is empty");
    if (plaintext == nullptr)
        OPENFHE_THROW(config_error, "plaintext passed to Decrypt is empty");
    if (privateKey == nullptr || Mismatched(privateKey->GetCryptoContext()))
        OPENFHE_THROW(config_error,
                      "Information passed to Decrypt was not generated with "
                      "this crypto context");

    // determine which type of plaintext that you need to decrypt into
    // Plaintext decrypted =
    // GetPlaintextForDecrypt(ciphertext->GetEncodingType(),
    // this->GetElementParams(), this->GetEncodingParams());
    Plaintext decrypted = GetPlaintextForDecrypt(ciphertext->GetEncodingType(),
                                                 ciphertext->GetElements()[0].GetParams(), this->GetEncodingParams());

    DecryptResult result;

    if ((ciphertext->GetEncodingType() == CKKS_PACKED_ENCODING) &&
        (ciphertext->GetElements()[0].GetParams()->GetParams().size() >
         1))  // only one tower in DCRTPoly // Andreea: this comment should be the other way around
        result = GetScheme()->Decrypt(ciphertext, privateKey, &decrypted->GetElement<Poly>());
    else
        result = GetScheme()->Decrypt(ciphertext, privateKey, &decrypted->GetElement<NativePoly>());

    if (result.isValid == false)
        return result;

    decrypted->SetScalingFactorInt(result.scalingFactorInt);

    if (ciphertext->GetEncodingType() == CKKS_PACKED_ENCODING) {
        auto decryptedCKKS = std::dynamic_pointer_cast<CKKSPackedEncoding>(decrypted);
        decryptedCKKS->SetNoiseScaleDeg(ciphertext->GetNoiseScaleDeg());
        decryptedCKKS->SetLevel(ciphertext->GetLevel());
        decryptedCKKS->SetScalingFactor(ciphertext->GetScalingFactor());
        decryptedCKKS->SetSlots(ciphertext->GetSlots());

        const auto cryptoParamsCKKS = std::dynamic_pointer_cast<CryptoParametersCKKSRNS>(this->GetCryptoParameters());

        decryptedCKKS->Decode(ciphertext->GetNoiseScaleDeg(), ciphertext->GetScalingFactor(),
                              cryptoParamsCKKS->GetScalingTechnique(), cryptoParamsCKKS->GetExecutionMode());
    }
    else {
        decrypted->Decode();
    }

    *plaintext = std::move(decrypted);
    return result;
}

template <>
DecryptResult CryptoContextImpl<DCRTPoly>::MultipartyDecryptFusion(
    const std::vector<Ciphertext<DCRTPoly>>& partialCiphertextVec, Plaintext* plaintext) const {
    DecryptResult result;

    // Make sure we're processing ciphertexts.
    size_t last_ciphertext = partialCiphertextVec.size();
    if (last_ciphertext < 1)
        return result;

    for (size_t i = 0; i < last_ciphertext; i++) {
        if (partialCiphertextVec[i] == nullptr || Mismatched(partialCiphertextVec[i]->GetCryptoContext()))
            OPENFHE_THROW(config_error,
                          "A ciphertext passed to MultipartyDecryptFusion was not "
                          "generated with this crypto context");
        if (partialCiphertextVec[i]->GetEncodingType() != partialCiphertextVec[0]->GetEncodingType())
            OPENFHE_THROW(type_error,
                          "Ciphertexts passed to MultipartyDecryptFusion have "
                          "mismatched encoding types");
    }

    // determine which type of plaintext that you need to decrypt into
    Plaintext decrypted =
        GetPlaintextForDecrypt(partialCiphertextVec[0]->GetEncodingType(),
                               partialCiphertextVec[0]->GetElements()[0].GetParams(), this->GetEncodingParams());

    if ((partialCiphertextVec[0]->GetEncodingType() == CKKS_PACKED_ENCODING) &&
        (partialCiphertextVec[0]->GetElements()[0].GetParams()->GetParams().size() > 1))
        result = GetScheme()->MultipartyDecryptFusion(partialCiphertextVec, &decrypted->GetElement<Poly>());
    else
        result = GetScheme()->MultipartyDecryptFusion(partialCiphertextVec, &decrypted->GetElement<NativePoly>());

    if (result.isValid == false)
        return result;

    decrypted->SetScalingFactorInt(result.scalingFactorInt);

    if (partialCiphertextVec[0]->GetEncodingType() == CKKS_PACKED_ENCODING) {
        auto decryptedCKKS = std::dynamic_pointer_cast<CKKSPackedEncoding>(decrypted);
        decryptedCKKS->SetSlots(partialCiphertextVec[0]->GetSlots());
        const auto cryptoParamsCKKS = std::dynamic_pointer_cast<CryptoParametersCKKSRNS>(this->GetCryptoParameters());
        decryptedCKKS->Decode(partialCiphertextVec[0]->GetNoiseScaleDeg(), partialCiphertextVec[0]->GetScalingFactor(),
                              cryptoParamsCKKS->GetScalingTechnique(), cryptoParamsCKKS->GetExecutionMode());
    }
    else {
        decrypted->Decode();
    }

    *plaintext = std::move(decrypted);

    return result;
}

// Function for sharing and recovery of secret for Threshold FHE with aborts
template <>
std::unordered_map<uint32_t, DCRTPoly> CryptoContextImpl<DCRTPoly>::ShareKeys(const PrivateKey<DCRTPoly>& sk, usint N,
                                                                              usint threshold, usint index,
                                                                              const std::string& shareType) const {
    // conditions on N and threshold for security with aborts
    if (N < 2)
        OPENFHE_THROW(config_error, "Number of parties needs to be at least 3 for aborts");

    if (threshold <= N / 2)
        OPENFHE_THROW(config_error, "Threshold required to be majority (more than N/2)");

    const auto cryptoParams = sk->GetCryptoContext()->GetCryptoParameters();
    auto elementParams      = cryptoParams->GetElementParams();
    auto vecSize            = elementParams->GetParams().size();
    auto ring_dimension     = elementParams->GetRingDimension();

    // condition for inverse in lagrange coeff to exist.
    for (usint k = 0; k < vecSize; k++) {
        auto modq_k = elementParams->GetParams()[k]->GetModulus();
        if (N >= modq_k)
            OPENFHE_THROW(math_error, "Number of parties N needs to be less than DCRTPoly moduli");
    }

    // secret sharing
    const usint num_of_shares = N - 1;
    std::unordered_map<uint32_t, DCRTPoly> SecretShares;

    if (shareType == "additive") {
        // generate a random share of N-2 elements and create the last share as sk - (sk_1 + ... + sk_N-2)
        typename DCRTPoly::DugType dug;
        DCRTPoly rsum(dug, elementParams, Format::EVALUATION);

        std::vector<DCRTPoly> SecretSharesVec;
        SecretSharesVec.reserve(num_of_shares);
        SecretSharesVec.push_back(rsum);
        for (size_t i = 1; i < num_of_shares - 1; ++i) {
            DCRTPoly r(dug, elementParams, Format::EVALUATION);  // should re-generate uniform r for each share
            rsum += r;
            SecretSharesVec.push_back(std::move(r));
        }
        SecretSharesVec.push_back(sk->GetPrivateElement() - rsum);

        usint ctr = 0;
        for (size_t i = 1; i <= N; i++) {
            if (i != index) {
                SecretShares[i] = SecretSharesVec[ctr];
                ctr++;
            }
        }
    }
    else if (shareType == "shamir") {
        // vector to store columnwise randomly generated coefficients for polynomial f from Z_q for every secret key entry
        std::vector<DCRTPoly> fs;
        fs.reserve(threshold);

        // set constant term of polynomial f_i to s_i
        DCRTPoly ske = sk->GetPrivateElement();
        // set the secret element in coefficient format
        ske.SetFormat(Format::COEFFICIENT);

        fs.push_back(std::move(ske));
        // generate random coefficients
        typename DCRTPoly::DugType dug;
        for (size_t i = 1; i < threshold; i++) {
            fs.push_back(DCRTPoly(dug, elementParams, Format::COEFFICIENT));
        }

        // evaluate the polynomial at the index of the parties 1 to N

        for (size_t i = 1; i <= N; i++) {
            if (i != index) {
                DCRTPoly feval(elementParams, Format::COEFFICIENT, true);
                for (size_t k = 0; k < vecSize; k++) {
                    auto modq_k = elementParams->GetParams()[k]->GetModulus();

                    NativeVector powtempvec(ring_dimension, modq_k);
                    NativePoly powtemppoly(elementParams->GetParams()[k], Format::COEFFICIENT);
                    NativePoly fevalpoly(elementParams->GetParams()[k], Format::COEFFICIENT, true);

                    NativeInteger powtemp(1);
                    for (size_t t = 1; t < threshold; t++) {
                        powtemp = powtemp.ModMul(i, modq_k);

                        for (size_t d = 0; d < ring_dimension; d++) {
                            powtempvec.at(d) = powtemp;
                        }

                        powtemppoly.SetValues(powtempvec, Format::COEFFICIENT);

                        auto fst = fs[t].GetElementAtIndex(k);

                        for (size_t l = 0; l < ring_dimension; l++) {
                            fevalpoly.at(l) += powtemppoly.at(l).ModMul(fst.at(l), modq_k);
                        }
                    }
                    fevalpoly += fs[0].GetElementAtIndex(k);

                    fevalpoly.SetFormat(Format::COEFFICIENT);
                    feval.SetElementAtIndex(k, fevalpoly);
                }
                // assign fi
                SecretShares[i] = feval;
            }
        }
    }
    return SecretShares;
}

//**********************************************************************************
template <>
void CryptoContextImpl<DCRTPoly>::RecoverSharedKey(PrivateKey<DCRTPoly>& sk,
                                                   std::unordered_map<uint32_t, DCRTPoly>& sk_shares, usint N,
                                                   usint threshold, const std::string& shareType) const {
    if (!sk)
        OPENFHE_THROW(config_error, "sk is null");

    if (sk_shares.size() < threshold)
        OPENFHE_THROW(config_error, "Number of shares available less than threshold of the sharing scheme");

    // conditions on N and threshold for security with aborts
    if (N < 2)
        OPENFHE_THROW(config_error, "Number of parties needs to be at least 3 for aborts");

    if (threshold <= N / 2)
        OPENFHE_THROW(config_error, "Threshold required to be majority (more than N/2)");

    const auto cryptoParams = sk->GetCryptoContext()->GetCryptoParameters();
    auto elementParams      = cryptoParams->GetElementParams();
    auto ring_dimension     = elementParams->GetRingDimension();
    auto vecSize            = elementParams->GetParams().size();

    // condition for inverse in lagrange coeff to exist.
    for (size_t k = 0; k < vecSize; k++) {
        auto modq_k = elementParams->GetParams()[k]->GetModulus();
        if (N >= modq_k)
            OPENFHE_THROW(not_implemented_error, "Number of parties N needs to be less than DCRTPoly moduli");
    }

    // vector of indexes of the clients
    std::vector<int64_t> client_indexes;
    for (size_t i = 1; i <= N; i++) {
        if (sk_shares.find(i) != sk_shares.end()) {
            client_indexes.push_back(i);
        }
    }

    const auto client_indexes_size = client_indexes.size();
    std::sort(client_indexes.begin(), client_indexes.end());

    std::vector<int64_t> duplicate_indexes;
    for (size_t i = 1; i < client_indexes_size; i++) {
        if (client_indexes[i - 1] == client_indexes[i]) {
            // STL function to push the duplicate words in a new vector string
            if (duplicate_indexes.empty())
                duplicate_indexes.push_back(client_indexes[i]);
            else if (client_indexes[i] != duplicate_indexes.back())
                duplicate_indexes.push_back(client_indexes[i]);
        }
    }
    if (duplicate_indexes.size() != 0 && sk_shares.size() == threshold)
        OPENFHE_THROW(config_error, "Not enough shares to recover the secret");

    if (shareType == "additive") {
        // check that the vector size is the threshold for the sharing scheme

        // retrieve all shares from the map
        std::vector<DCRTPoly> sk_sharesElements;
        for (size_t i = 1; i <= N; i++) {
            if (sk_shares.find(i) != sk_shares.end()) {
                sk_sharesElements.push_back(sk_shares[i]);
            }
        }

        DCRTPoly sum_of_elems(sk_sharesElements[0]);
        // recover secret from the shares
        // sk = sk_1 + ... + sk_N-1;
        for (size_t i = 1; i < threshold; i++) {
            sum_of_elems += sk_sharesElements[i];
        }
        sk->SetPrivateElement(sum_of_elems);
    }
    else if (shareType == "shamir") {
        // use lagrange interpolation to recover the secret

        // vector of lagrange coefficients L_j = Pdt_i ne j (i (i-j)^-1)
        std::unordered_map<usint, DCRTPoly> Lagrange_coeffs;
        // initialize the coefficients
        for (size_t i = 0; i < client_indexes_size; i++) {
            Lagrange_coeffs[client_indexes[i]] = DCRTPoly(elementParams, Format::EVALUATION);
        }

        // recovery of the secret with lagrange coefficients and the secret shares
        for (size_t j = 0; j < client_indexes_size; j++) {
            for (size_t k = 0; k < vecSize; k++) {
                auto modq_k = elementParams->GetParams()[k]->GetModulus();

                NativeVector multvec(ring_dimension, modq_k);
                NativeInteger mult(1);
                for (size_t d = 0; d < ring_dimension; d++) {
                    multvec.at(d) = mult;
                }

                NativePoly multpoly(elementParams->GetParams()[k], Format::COEFFICIENT);
                multpoly.SetValues(multvec, Format::COEFFICIENT);
                for (size_t i = 0; i < client_indexes_size; i++) {
                    if (client_indexes[j] != client_indexes[i]) {
                        auto denominator = client_indexes[i] - client_indexes[j];
                        NativeInteger denom_positive(0);
                        if (denominator < 0) {
                            denom_positive = NativeInteger(-1) * denominator;
                            denom_positive = modq_k - denom_positive;
                        }
                        else {
                            denom_positive = denominator;
                        }
                        NativeInteger den_nativeint(denom_positive);
                        NativeInteger denom_inv(den_nativeint.ModInverse(modq_k));

                        NativeVector indexvec(ring_dimension, modq_k);
                        NativeVector denomvec(ring_dimension, modq_k);
                        for (size_t d = 0; d < ring_dimension; d++) {
                            indexvec.at(d) = client_indexes[i];
                            denomvec.at(d) = denom_inv;
                        }

                        NativePoly indexpoly(elementParams->GetParams()[k], Format::COEFFICIENT);
                        NativePoly denompoly(elementParams->GetParams()[k], Format::COEFFICIENT);
                        indexpoly.SetValues(indexvec, Format::COEFFICIENT);
                        denompoly.SetValues(denomvec, Format::COEFFICIENT);

                        for (size_t l = 0; l < ring_dimension; l++) {
                            multpoly.at(l) =
                                multpoly.at(l).ModMul(indexpoly.at(l).ModMul(denompoly.at(l), modq_k), modq_k);
                        }
                    }
                }
                multpoly.SetFormat(Format::EVALUATION);

                Lagrange_coeffs[client_indexes[j]].SetElementAtIndex(k, multpoly);
            }
        }

        DCRTPoly lagrange_sum_of_elems(elementParams, Format::COEFFICIENT, true);
        for (size_t k = 0; k < vecSize; k++) {
            NativePoly lagrange_sum_of_elems_poly(elementParams->GetParams()[k], Format::COEFFICIENT, true);
            auto modq_k = elementParams->GetParams()[k]->GetModulus();
            for (size_t i = 0; i < client_indexes_size; i++) {
                Lagrange_coeffs[client_indexes[i]].SetFormat(Format::COEFFICIENT);
                for (size_t l = 0; l < ring_dimension; l++) {
                    lagrange_sum_of_elems_poly.at(l) +=
                        Lagrange_coeffs[client_indexes[i]].GetElementAtIndex(k).at(l).ModMul(
                            sk_shares[client_indexes[i]].GetElementAtIndex(k).at(l), modq_k);
                }
            }
            lagrange_sum_of_elems.SetElementAtIndex(k, lagrange_sum_of_elems_poly);
        }

        lagrange_sum_of_elems.SetFormat(Format::EVALUATION);
        sk->SetPrivateElement(lagrange_sum_of_elems);
    }
}

template class CryptoContextImpl<DCRTPoly>;

}  // namespace lbcrypto
