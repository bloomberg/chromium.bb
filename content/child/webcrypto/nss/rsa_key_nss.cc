// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/webcrypto/nss/rsa_key_nss.h"

#include "base/logging.h"
#include "base/numerics/safe_math.h"
#include "content/child/webcrypto/crypto_data.h"
#include "content/child/webcrypto/jwk.h"
#include "content/child/webcrypto/nss/key_nss.h"
#include "content/child/webcrypto/nss/util_nss.h"
#include "content/child/webcrypto/status.h"
#include "content/child/webcrypto/webcrypto_util.h"
#include "crypto/scoped_nss_types.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithmParams.h"
#include "third_party/WebKit/public/platform/WebCryptoKeyAlgorithm.h"

namespace content {

namespace webcrypto {

namespace {

// Converts a (big-endian) WebCrypto BigInteger, with or without leading zeros,
// to unsigned long.
bool BigIntegerToLong(const uint8_t* data,
                      unsigned int data_size,
                      unsigned long* result) {
  // TODO(eroman): Fix handling of empty biginteger. http://crubg.com/373552
  if (data_size == 0)
    return false;

  *result = 0;
  for (size_t i = 0; i < data_size; ++i) {
    size_t reverse_i = data_size - i - 1;

    if (reverse_i >= sizeof(unsigned long) && data[i])
      return false;  // Too large for a long.

    *result |= data[i] << 8 * reverse_i;
  }
  return true;
}

bool CreatePublicKeyAlgorithm(const blink::WebCryptoAlgorithm& algorithm,
                              SECKEYPublicKey* key,
                              blink::WebCryptoKeyAlgorithm* key_algorithm) {
  // TODO(eroman): What about other key types rsaPss, rsaOaep.
  if (!key || key->keyType != rsaKey)
    return false;

  unsigned int modulus_length_bits = SECKEY_PublicKeyStrength(key) * 8;
  CryptoData public_exponent(key->u.rsa.publicExponent.data,
                             key->u.rsa.publicExponent.len);

  switch (algorithm.paramsType()) {
    case blink::WebCryptoAlgorithmParamsTypeRsaHashedImportParams:
    case blink::WebCryptoAlgorithmParamsTypeRsaHashedKeyGenParams:
      *key_algorithm = blink::WebCryptoKeyAlgorithm::createRsaHashed(
          algorithm.id(),
          modulus_length_bits,
          public_exponent.bytes(),
          public_exponent.byte_length(),
          GetInnerHashAlgorithm(algorithm).id());
      return true;
    default:
      return false;
  }
}

bool CreatePrivateKeyAlgorithm(const blink::WebCryptoAlgorithm& algorithm,
                               SECKEYPrivateKey* key,
                               blink::WebCryptoKeyAlgorithm* key_algorithm) {
  crypto::ScopedSECKEYPublicKey public_key(SECKEY_ConvertToPublicKey(key));
  return CreatePublicKeyAlgorithm(algorithm, public_key.get(), key_algorithm);
}

#if defined(USE_NSS) && !defined(OS_CHROMEOS)
Status ErrorRsaKeyImportNotSupported() {
  return Status::ErrorUnsupported(
      "NSS version must be at least 3.16.2 for RSA key import. See "
      "http://crbug.com/380424");
}

// Prior to NSS 3.16.2 RSA key parameters were not validated. This is
// a security problem for RSA private key import from JWK which uses a
// CKA_ID based on the public modulus to retrieve the private key.
Status NssSupportsRsaKeyImport() {
  if (!NSS_VersionCheck("3.16.2"))
    return ErrorRsaKeyImportNotSupported();

  // Also ensure that the version of Softoken is 3.16.2 or later.
  crypto::ScopedPK11Slot slot(PK11_GetInternalSlot());
  CK_SLOT_INFO info = {};
  if (PK11_GetSlotInfo(slot.get(), &info) != SECSuccess)
    return ErrorRsaKeyImportNotSupported();

  // CK_SLOT_INFO.hardwareVersion contains the major.minor
  // version info for Softoken in the corresponding .major/.minor
  // fields, and .firmwareVersion contains the patch.build
  // version info (in the .major/.minor fields)
  if ((info.hardwareVersion.major > 3) ||
      (info.hardwareVersion.major == 3 &&
       (info.hardwareVersion.minor > 16 ||
        (info.hardwareVersion.minor == 16 &&
         info.firmwareVersion.major >= 2)))) {
    return Status::Success();
  }

  return ErrorRsaKeyImportNotSupported();
}
#else
Status NssSupportsRsaKeyImport() {
  return Status::Success();
}
#endif

bool CreateRsaHashedPublicKeyAlgorithm(
    const blink::WebCryptoAlgorithm& algorithm,
    SECKEYPublicKey* key,
    blink::WebCryptoKeyAlgorithm* key_algorithm) {
  // TODO(eroman): What about other key types rsaPss, rsaOaep.
  if (!key || key->keyType != rsaKey)
    return false;

  unsigned int modulus_length_bits = SECKEY_PublicKeyStrength(key) * 8;
  CryptoData public_exponent(key->u.rsa.publicExponent.data,
                             key->u.rsa.publicExponent.len);

  switch (algorithm.paramsType()) {
    case blink::WebCryptoAlgorithmParamsTypeRsaHashedImportParams:
    case blink::WebCryptoAlgorithmParamsTypeRsaHashedKeyGenParams:
      *key_algorithm = blink::WebCryptoKeyAlgorithm::createRsaHashed(
          algorithm.id(),
          modulus_length_bits,
          public_exponent.bytes(),
          public_exponent.byte_length(),
          GetInnerHashAlgorithm(algorithm).id());
      return true;
    default:
      return false;
  }
}

bool CreateRsaHashedPrivateKeyAlgorithm(
    const blink::WebCryptoAlgorithm& algorithm,
    SECKEYPrivateKey* key,
    blink::WebCryptoKeyAlgorithm* key_algorithm) {
  crypto::ScopedSECKEYPublicKey public_key(SECKEY_ConvertToPublicKey(key));
  if (!public_key)
    return false;
  return CreateRsaHashedPublicKeyAlgorithm(
      algorithm, public_key.get(), key_algorithm);
}

// From PKCS#1 [http://tools.ietf.org/html/rfc3447]:
//
//    RSAPrivateKey ::= SEQUENCE {
//      version           Version,
//      modulus           INTEGER,  -- n
//      publicExponent    INTEGER,  -- e
//      privateExponent   INTEGER,  -- d
//      prime1            INTEGER,  -- p
//      prime2            INTEGER,  -- q
//      exponent1         INTEGER,  -- d mod (p-1)
//      exponent2         INTEGER,  -- d mod (q-1)
//      coefficient       INTEGER,  -- (inverse of q) mod p
//      otherPrimeInfos   OtherPrimeInfos OPTIONAL
//    }
//
// Note that otherPrimeInfos is only applicable for version=1. Since NSS
// doesn't use multi-prime can safely use version=0.
struct RSAPrivateKey {
  SECItem version;
  SECItem modulus;
  SECItem public_exponent;
  SECItem private_exponent;
  SECItem prime1;
  SECItem prime2;
  SECItem exponent1;
  SECItem exponent2;
  SECItem coefficient;
};

// The system NSS library doesn't have the new PK11_ExportDERPrivateKeyInfo
// function yet (https://bugzilla.mozilla.org/show_bug.cgi?id=519255). So we
// provide a fallback implementation.
#if defined(USE_NSS)
const SEC_ASN1Template RSAPrivateKeyTemplate[] = {
    {SEC_ASN1_SEQUENCE, 0, NULL, sizeof(RSAPrivateKey)},
    {SEC_ASN1_INTEGER, offsetof(RSAPrivateKey, version)},
    {SEC_ASN1_INTEGER, offsetof(RSAPrivateKey, modulus)},
    {SEC_ASN1_INTEGER, offsetof(RSAPrivateKey, public_exponent)},
    {SEC_ASN1_INTEGER, offsetof(RSAPrivateKey, private_exponent)},
    {SEC_ASN1_INTEGER, offsetof(RSAPrivateKey, prime1)},
    {SEC_ASN1_INTEGER, offsetof(RSAPrivateKey, prime2)},
    {SEC_ASN1_INTEGER, offsetof(RSAPrivateKey, exponent1)},
    {SEC_ASN1_INTEGER, offsetof(RSAPrivateKey, exponent2)},
    {SEC_ASN1_INTEGER, offsetof(RSAPrivateKey, coefficient)},
    {0}};
#endif  // defined(USE_NSS)

// On success |value| will be filled with data which must be freed by
// SECITEM_FreeItem(value, PR_FALSE);
bool ReadUint(SECKEYPrivateKey* key,
              CK_ATTRIBUTE_TYPE attribute,
              SECItem* value) {
  SECStatus rv = PK11_ReadRawAttribute(PK11_TypePrivKey, key, attribute, value);

  // PK11_ReadRawAttribute() returns items of type siBuffer. However in order
  // for the ASN.1 encoding to be correct, the items must be of type
  // siUnsignedInteger.
  value->type = siUnsignedInteger;

  return rv == SECSuccess;
}

// Fills |out| with the RSA private key properties. Returns true on success.
// Regardless of the return value, the caller must invoke FreeRSAPrivateKey()
// to free up any allocated memory.
//
// The passed in RSAPrivateKey must be zero-initialized.
bool InitRSAPrivateKey(SECKEYPrivateKey* key, RSAPrivateKey* out) {
  if (key->keyType != rsaKey)
    return false;

  // Everything should be zero-ed out. These are just some spot checks.
  DCHECK(!out->version.data);
  DCHECK(!out->version.len);
  DCHECK(!out->modulus.data);
  DCHECK(!out->modulus.len);

  // Always use version=0 since not using multi-prime.
  if (!SEC_ASN1EncodeInteger(NULL, &out->version, 0))
    return false;

  if (!ReadUint(key, CKA_MODULUS, &out->modulus))
    return false;
  if (!ReadUint(key, CKA_PUBLIC_EXPONENT, &out->public_exponent))
    return false;
  if (!ReadUint(key, CKA_PRIVATE_EXPONENT, &out->private_exponent))
    return false;
  if (!ReadUint(key, CKA_PRIME_1, &out->prime1))
    return false;
  if (!ReadUint(key, CKA_PRIME_2, &out->prime2))
    return false;
  if (!ReadUint(key, CKA_EXPONENT_1, &out->exponent1))
    return false;
  if (!ReadUint(key, CKA_EXPONENT_2, &out->exponent2))
    return false;
  if (!ReadUint(key, CKA_COEFFICIENT, &out->coefficient))
    return false;

  return true;
}

struct FreeRsaPrivateKey {
  void operator()(RSAPrivateKey* out) {
    SECITEM_FreeItem(&out->version, PR_FALSE);
    SECITEM_FreeItem(&out->modulus, PR_FALSE);
    SECITEM_FreeItem(&out->public_exponent, PR_FALSE);
    SECITEM_FreeItem(&out->private_exponent, PR_FALSE);
    SECITEM_FreeItem(&out->prime1, PR_FALSE);
    SECITEM_FreeItem(&out->prime2, PR_FALSE);
    SECITEM_FreeItem(&out->exponent1, PR_FALSE);
    SECITEM_FreeItem(&out->exponent2, PR_FALSE);
    SECITEM_FreeItem(&out->coefficient, PR_FALSE);
  }
};

typedef scoped_ptr<CERTSubjectPublicKeyInfo,
                   crypto::NSSDestroyer<CERTSubjectPublicKeyInfo,
                                        SECKEY_DestroySubjectPublicKeyInfo> >
    ScopedCERTSubjectPublicKeyInfo;

struct DestroyGenericObject {
  void operator()(PK11GenericObject* o) const {
    if (o)
      PK11_DestroyGenericObject(o);
  }
};

typedef scoped_ptr<PK11GenericObject, DestroyGenericObject>
    ScopedPK11GenericObject;

// Helper to add an attribute to a template.
void AddAttribute(CK_ATTRIBUTE_TYPE type,
                  void* value,
                  unsigned long length,
                  std::vector<CK_ATTRIBUTE>* templ) {
  CK_ATTRIBUTE attribute = {type, value, length};
  templ->push_back(attribute);
}

// Helper to optionally add an attribute to a template, if the provided data is
// non-empty.
void AddOptionalAttribute(CK_ATTRIBUTE_TYPE type,
                          const CryptoData& data,
                          std::vector<CK_ATTRIBUTE>* templ) {
  if (!data.byte_length())
    return;
  CK_ATTRIBUTE attribute = {type, const_cast<unsigned char*>(data.bytes()),
                            data.byte_length()};
  templ->push_back(attribute);
}

void AddOptionalAttribute(CK_ATTRIBUTE_TYPE type,
                          const std::string& data,
                          std::vector<CK_ATTRIBUTE>* templ) {
  AddOptionalAttribute(type, CryptoData(data), templ);
}

Status ExportKeyPkcs8Nss(SECKEYPrivateKey* key, std::vector<uint8_t>* buffer) {
  if (key->keyType != rsaKey)
    return Status::ErrorUnsupported();

// TODO(rsleevi): Implement OAEP support according to the spec.

#if defined(USE_NSS)
  // PK11_ExportDERPrivateKeyInfo isn't available. Use our fallback code.
  const SECOidTag algorithm = SEC_OID_PKCS1_RSA_ENCRYPTION;
  const int kPrivateKeyInfoVersion = 0;

  SECKEYPrivateKeyInfo private_key_info = {};
  RSAPrivateKey rsa_private_key = {};
  scoped_ptr<RSAPrivateKey, FreeRsaPrivateKey> free_private_key(
      &rsa_private_key);

  // http://crbug.com/366427: the spec does not define any other failures for
  // exporting, so none of the subsequent errors are spec compliant.
  if (!InitRSAPrivateKey(key, &rsa_private_key))
    return Status::OperationError();

  crypto::ScopedPLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
  if (!arena.get())
    return Status::OperationError();

  if (!SEC_ASN1EncodeItem(arena.get(),
                          &private_key_info.privateKey,
                          &rsa_private_key,
                          RSAPrivateKeyTemplate))
    return Status::OperationError();

  if (SECSuccess !=
      SECOID_SetAlgorithmID(
          arena.get(), &private_key_info.algorithm, algorithm, NULL))
    return Status::OperationError();

  if (!SEC_ASN1EncodeInteger(
          arena.get(), &private_key_info.version, kPrivateKeyInfoVersion))
    return Status::OperationError();

  crypto::ScopedSECItem encoded_key(
      SEC_ASN1EncodeItem(NULL,
                         NULL,
                         &private_key_info,
                         SEC_ASN1_GET(SECKEY_PrivateKeyInfoTemplate)));
#else   // defined(USE_NSS)
  crypto::ScopedSECItem encoded_key(PK11_ExportDERPrivateKeyInfo(key, NULL));
#endif  // defined(USE_NSS)

  if (!encoded_key.get())
    return Status::OperationError();

  buffer->assign(encoded_key->data, encoded_key->data + encoded_key->len);
  return Status::Success();
}

Status ImportRsaPrivateKey(const blink::WebCryptoAlgorithm& algorithm,
                           bool extractable,
                           blink::WebCryptoKeyUsageMask usage_mask,
                           const JwkRsaInfo& params,
                           blink::WebCryptoKey* key) {
  Status status = NssSupportsRsaKeyImport();
  if (status.IsError())
    return status;

  CK_OBJECT_CLASS obj_class = CKO_PRIVATE_KEY;
  CK_KEY_TYPE key_type = CKK_RSA;
  CK_BBOOL ck_false = CK_FALSE;

  std::vector<CK_ATTRIBUTE> key_template;

  AddAttribute(CKA_CLASS, &obj_class, sizeof(obj_class), &key_template);
  AddAttribute(CKA_KEY_TYPE, &key_type, sizeof(key_type), &key_template);
  AddAttribute(CKA_TOKEN, &ck_false, sizeof(ck_false), &key_template);
  AddAttribute(CKA_SENSITIVE, &ck_false, sizeof(ck_false), &key_template);
  AddAttribute(CKA_PRIVATE, &ck_false, sizeof(ck_false), &key_template);

  // Required properties.
  AddOptionalAttribute(CKA_MODULUS, params.n, &key_template);
  AddOptionalAttribute(CKA_PUBLIC_EXPONENT, params.e, &key_template);
  AddOptionalAttribute(CKA_PRIVATE_EXPONENT, params.d, &key_template);

  // Manufacture a CKA_ID so the created key can be retrieved later as a
  // SECKEYPrivateKey using FindKeyByKeyID(). Unfortunately there isn't a more
  // direct way to do this in NSS.
  //
  // For consistency with other NSS key creation methods, set the CKA_ID to
  // PK11_MakeIDFromPubKey(). There are some problems with
  // this approach:
  //
  //  (1) Prior to NSS 3.16.2, there is no parameter validation when creating
  //      private keys. It is therefore possible to construct a key using the
  //      known public modulus, and where all the other parameters are bogus.
  //      FindKeyByKeyID() returns the first key matching the ID. So this would
  //      effectively allow an attacker to retrieve a private key of their
  //      choice.
  //
  //  (2) The ID space is shared by different key types. So theoretically
  //      possible to retrieve a key of the wrong type which has a matching
  //      CKA_ID. In practice I am told this is not likely except for small key
  //      sizes, since would require constructing keys with the same public
  //      data.
  //
  //  (3) FindKeyByKeyID() doesn't necessarily return the object that was just
  //      created by CreateGenericObject. If the pre-existing key was
  //      provisioned with flags incompatible with WebCrypto (for instance
  //      marked sensitive) then this will break things.
  SECItem modulus_item = MakeSECItemForBuffer(CryptoData(params.n));
  crypto::ScopedSECItem object_id(PK11_MakeIDFromPubKey(&modulus_item));
  AddOptionalAttribute(
      CKA_ID, CryptoData(object_id->data, object_id->len), &key_template);

  // Optional properties (all of these will have been specified or none).
  AddOptionalAttribute(CKA_PRIME_1, params.p, &key_template);
  AddOptionalAttribute(CKA_PRIME_2, params.q, &key_template);
  AddOptionalAttribute(CKA_EXPONENT_1, params.dp, &key_template);
  AddOptionalAttribute(CKA_EXPONENT_2, params.dq, &key_template);
  AddOptionalAttribute(CKA_COEFFICIENT, params.qi, &key_template);

  crypto::ScopedPK11Slot slot(PK11_GetInternalSlot());

  ScopedPK11GenericObject key_object(PK11_CreateGenericObject(
      slot.get(), &key_template[0], key_template.size(), PR_FALSE));

  if (!key_object)
    return Status::OperationError();

  crypto::ScopedSECKEYPrivateKey private_key_tmp(
      PK11_FindKeyByKeyID(slot.get(), object_id.get(), NULL));

  // PK11_FindKeyByKeyID() may return a handle to an existing key, rather than
  // the object created by PK11_CreateGenericObject().
  crypto::ScopedSECKEYPrivateKey private_key(
      SECKEY_CopyPrivateKey(private_key_tmp.get()));

  if (!private_key)
    return Status::OperationError();

  blink::WebCryptoKeyAlgorithm key_algorithm;
  if (!CreatePrivateKeyAlgorithm(algorithm, private_key.get(), &key_algorithm))
    return Status::ErrorUnexpected();

  std::vector<uint8_t> pkcs8_data;
  status = ExportKeyPkcs8Nss(private_key.get(), &pkcs8_data);
  if (status.IsError())
    return status;

  scoped_ptr<PrivateKeyNss> key_handle(
      new PrivateKeyNss(private_key.Pass(), CryptoData(pkcs8_data)));

  *key = blink::WebCryptoKey::create(key_handle.release(),
                                     blink::WebCryptoKeyTypePrivate,
                                     extractable,
                                     key_algorithm,
                                     usage_mask);
  return Status::Success();
}

Status ExportKeySpkiNss(SECKEYPublicKey* key, std::vector<uint8_t>* buffer) {
  const crypto::ScopedSECItem spki_der(
      SECKEY_EncodeDERSubjectPublicKeyInfo(key));
  if (!spki_der)
    return Status::OperationError();

  buffer->assign(spki_der->data, spki_der->data + spki_der->len);
  return Status::Success();
}

Status ImportRsaPublicKey(const blink::WebCryptoAlgorithm& algorithm,
                          bool extractable,
                          blink::WebCryptoKeyUsageMask usage_mask,
                          const CryptoData& modulus_data,
                          const CryptoData& exponent_data,
                          blink::WebCryptoKey* key) {
  if (!modulus_data.byte_length())
    return Status::ErrorImportRsaEmptyModulus();

  if (!exponent_data.byte_length())
    return Status::ErrorImportRsaEmptyExponent();

  DCHECK(modulus_data.bytes());
  DCHECK(exponent_data.bytes());

  // NSS does not provide a way to create an RSA public key directly from the
  // modulus and exponent values, but it can import an DER-encoded ASN.1 blob
  // with these values and create the public key from that. The code below
  // follows the recommendation described in
  // https://developer.mozilla.org/en-US/docs/NSS/NSS_Tech_Notes/nss_tech_note7

  // Pack the input values into a struct compatible with NSS ASN.1 encoding, and
  // set up an ASN.1 encoder template for it.
  struct RsaPublicKeyData {
    SECItem modulus;
    SECItem exponent;
  };
  const RsaPublicKeyData pubkey_in = {
      {siUnsignedInteger, const_cast<unsigned char*>(modulus_data.bytes()),
       modulus_data.byte_length()},
      {siUnsignedInteger, const_cast<unsigned char*>(exponent_data.bytes()),
       exponent_data.byte_length()}};
  const SEC_ASN1Template rsa_public_key_template[] = {
      {SEC_ASN1_SEQUENCE, 0, NULL, sizeof(RsaPublicKeyData)},
      {
       SEC_ASN1_INTEGER, offsetof(RsaPublicKeyData, modulus),
      },
      {
       SEC_ASN1_INTEGER, offsetof(RsaPublicKeyData, exponent),
      },
      {
       0,
      }};

  // DER-encode the public key.
  crypto::ScopedSECItem pubkey_der(
      SEC_ASN1EncodeItem(NULL, NULL, &pubkey_in, rsa_public_key_template));
  if (!pubkey_der)
    return Status::OperationError();

  // Import the DER-encoded public key to create an RSA SECKEYPublicKey.
  crypto::ScopedSECKEYPublicKey pubkey(
      SECKEY_ImportDERPublicKey(pubkey_der.get(), CKK_RSA));
  if (!pubkey)
    return Status::OperationError();

  blink::WebCryptoKeyAlgorithm key_algorithm;
  if (!CreatePublicKeyAlgorithm(algorithm, pubkey.get(), &key_algorithm))
    return Status::ErrorUnexpected();

  std::vector<uint8_t> spki_data;
  Status status = ExportKeySpkiNss(pubkey.get(), &spki_data);
  if (status.IsError())
    return status;

  scoped_ptr<PublicKeyNss> key_handle(
      new PublicKeyNss(pubkey.Pass(), CryptoData(spki_data)));

  *key = blink::WebCryptoKey::create(key_handle.release(),
                                     blink::WebCryptoKeyTypePublic,
                                     extractable,
                                     key_algorithm,
                                     usage_mask);
  return Status::Success();
}

}  // namespace

Status RsaHashedAlgorithm::VerifyKeyUsagesBeforeGenerateKeyPair(
    blink::WebCryptoKeyUsageMask combined_usage_mask,
    blink::WebCryptoKeyUsageMask* public_usage_mask,
    blink::WebCryptoKeyUsageMask* private_usage_mask) const {
  Status status = CheckKeyCreationUsages(
      all_public_key_usages_ | all_private_key_usages_, combined_usage_mask);
  if (status.IsError())
    return status;

  *public_usage_mask = combined_usage_mask & all_public_key_usages_;
  *private_usage_mask = combined_usage_mask & all_private_key_usages_;

  return Status::Success();
}

Status RsaHashedAlgorithm::GenerateKeyPair(
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask public_usage_mask,
    blink::WebCryptoKeyUsageMask private_usage_mask,
    blink::WebCryptoKey* public_key,
    blink::WebCryptoKey* private_key) const {
  const blink::WebCryptoRsaHashedKeyGenParams* params =
      algorithm.rsaHashedKeyGenParams();

  if (!params->modulusLengthBits())
    return Status::ErrorGenerateRsaZeroModulus();

  unsigned long public_exponent = 0;
  if (!BigIntegerToLong(params->publicExponent().data(),
                        params->publicExponent().size(),
                        &public_exponent) ||
      (public_exponent != 3 && public_exponent != 65537)) {
    return Status::ErrorGenerateKeyPublicExponent();
  }

  crypto::ScopedPK11Slot slot(PK11_GetInternalKeySlot());
  if (!slot)
    return Status::OperationError();

  PK11RSAGenParams rsa_gen_params;
  // keySizeInBits is a signed type, don't pass in a negative value.
  base::CheckedNumeric<int> signed_modulus(params->modulusLengthBits());
  if (!signed_modulus.IsValid())
    return Status::OperationError();
  rsa_gen_params.keySizeInBits = signed_modulus.ValueOrDie();
  rsa_gen_params.pe = public_exponent;

  const CK_FLAGS operation_flags_mask =
      CKF_ENCRYPT | CKF_DECRYPT | CKF_SIGN | CKF_VERIFY | CKF_WRAP | CKF_UNWRAP;

  // The private key must be marked as insensitive and extractable, otherwise it
  // cannot later be exported in unencrypted form or structured-cloned.
  const PK11AttrFlags attribute_flags =
      PK11_ATTR_INSENSITIVE | PK11_ATTR_EXTRACTABLE;

  // Note: NSS does not generate an sec_public_key if the call below fails,
  // so there is no danger of a leaked sec_public_key.
  SECKEYPublicKey* sec_public_key;
  crypto::ScopedSECKEYPrivateKey scoped_sec_private_key(
      PK11_GenerateKeyPairWithOpFlags(slot.get(),
                                      CKM_RSA_PKCS_KEY_PAIR_GEN,
                                      &rsa_gen_params,
                                      &sec_public_key,
                                      attribute_flags,
                                      generate_flags_,
                                      operation_flags_mask,
                                      NULL));
  if (!scoped_sec_private_key)
    return Status::OperationError();

  blink::WebCryptoKeyAlgorithm key_algorithm;
  if (!CreatePublicKeyAlgorithm(algorithm, sec_public_key, &key_algorithm))
    return Status::ErrorUnexpected();

  std::vector<uint8_t> spki_data;
  Status status = ExportKeySpkiNss(sec_public_key, &spki_data);
  if (status.IsError())
    return status;

  scoped_ptr<PublicKeyNss> public_key_handle(new PublicKeyNss(
      crypto::ScopedSECKEYPublicKey(sec_public_key), CryptoData(spki_data)));

  std::vector<uint8_t> pkcs8_data;
  status = ExportKeyPkcs8Nss(scoped_sec_private_key.get(), &pkcs8_data);
  if (status.IsError())
    return status;

  scoped_ptr<PrivateKeyNss> private_key_handle(
      new PrivateKeyNss(scoped_sec_private_key.Pass(), CryptoData(pkcs8_data)));

  *public_key = blink::WebCryptoKey::create(public_key_handle.release(),
                                            blink::WebCryptoKeyTypePublic,
                                            true,
                                            key_algorithm,
                                            public_usage_mask);
  *private_key = blink::WebCryptoKey::create(private_key_handle.release(),
                                             blink::WebCryptoKeyTypePrivate,
                                             extractable,
                                             key_algorithm,
                                             private_usage_mask);

  return Status::Success();
}

Status RsaHashedAlgorithm::VerifyKeyUsagesBeforeImportKey(
    blink::WebCryptoKeyFormat format,
    blink::WebCryptoKeyUsageMask usage_mask) const {
  switch (format) {
    case blink::WebCryptoKeyFormatSpki:
      return CheckKeyCreationUsages(all_public_key_usages_, usage_mask);
    case blink::WebCryptoKeyFormatPkcs8:
      return CheckKeyCreationUsages(all_private_key_usages_, usage_mask);
    case blink::WebCryptoKeyFormatJwk:
      return CheckKeyCreationUsages(
          all_public_key_usages_ | all_private_key_usages_, usage_mask);
    default:
      return Status::ErrorUnsupportedImportKeyFormat();
  }
}

Status RsaHashedAlgorithm::ImportKeyPkcs8(
    const CryptoData& key_data,
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usage_mask,
    blink::WebCryptoKey* key) const {
  Status status = NssSupportsRsaKeyImport();
  if (status.IsError())
    return status;

  if (!key_data.byte_length())
    return Status::ErrorImportEmptyKeyData();

  // The binary blob 'key_data' is expected to be a DER-encoded ASN.1 PKCS#8
  // private key info object.
  SECItem pki_der = MakeSECItemForBuffer(key_data);

  SECKEYPrivateKey* seckey_private_key = NULL;
  crypto::ScopedPK11Slot slot(PK11_GetInternalSlot());
  if (PK11_ImportDERPrivateKeyInfoAndReturnKey(slot.get(),
                                               &pki_der,
                                               NULL,    // nickname
                                               NULL,    // publicValue
                                               false,   // isPerm
                                               false,   // isPrivate
                                               KU_ALL,  // usage
                                               &seckey_private_key,
                                               NULL) != SECSuccess) {
    return Status::DataError();
  }
  DCHECK(seckey_private_key);
  crypto::ScopedSECKEYPrivateKey private_key(seckey_private_key);

  const KeyType sec_key_type = SECKEY_GetPrivateKeyType(private_key.get());
  if (sec_key_type != rsaKey)
    return Status::DataError();

  blink::WebCryptoKeyAlgorithm key_algorithm;
  if (!CreateRsaHashedPrivateKeyAlgorithm(
          algorithm, private_key.get(), &key_algorithm))
    return Status::ErrorUnexpected();

  // TODO(eroman): This is probably going to be the same as the input.
  std::vector<uint8_t> pkcs8_data;
  status = ExportKeyPkcs8Nss(private_key.get(), &pkcs8_data);
  if (status.IsError())
    return status;

  scoped_ptr<PrivateKeyNss> key_handle(
      new PrivateKeyNss(private_key.Pass(), CryptoData(pkcs8_data)));

  *key = blink::WebCryptoKey::create(key_handle.release(),
                                     blink::WebCryptoKeyTypePrivate,
                                     extractable,
                                     key_algorithm,
                                     usage_mask);

  return Status::Success();
}

Status RsaHashedAlgorithm::ImportKeySpki(
    const CryptoData& key_data,
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usage_mask,
    blink::WebCryptoKey* key) const {
  Status status = NssSupportsRsaKeyImport();
  if (status.IsError())
    return status;

  if (!key_data.byte_length())
    return Status::ErrorImportEmptyKeyData();

  // The binary blob 'key_data' is expected to be a DER-encoded ASN.1 Subject
  // Public Key Info. Decode this to a CERTSubjectPublicKeyInfo.
  SECItem spki_item = MakeSECItemForBuffer(key_data);
  const ScopedCERTSubjectPublicKeyInfo spki(
      SECKEY_DecodeDERSubjectPublicKeyInfo(&spki_item));
  if (!spki)
    return Status::DataError();

  crypto::ScopedSECKEYPublicKey sec_public_key(
      SECKEY_ExtractPublicKey(spki.get()));
  if (!sec_public_key)
    return Status::DataError();

  const KeyType sec_key_type = SECKEY_GetPublicKeyType(sec_public_key.get());
  if (sec_key_type != rsaKey)
    return Status::DataError();

  blink::WebCryptoKeyAlgorithm key_algorithm;
  if (!CreateRsaHashedPublicKeyAlgorithm(
          algorithm, sec_public_key.get(), &key_algorithm))
    return Status::ErrorUnexpected();

  // TODO(eroman): This is probably going to be the same as the input.
  std::vector<uint8_t> spki_data;
  status = ExportKeySpkiNss(sec_public_key.get(), &spki_data);
  if (status.IsError())
    return status;

  scoped_ptr<PublicKeyNss> key_handle(
      new PublicKeyNss(sec_public_key.Pass(), CryptoData(spki_data)));

  *key = blink::WebCryptoKey::create(key_handle.release(),
                                     blink::WebCryptoKeyTypePublic,
                                     extractable,
                                     key_algorithm,
                                     usage_mask);

  return Status::Success();
}

Status RsaHashedAlgorithm::ExportKeyPkcs8(const blink::WebCryptoKey& key,
                                          std::vector<uint8_t>* buffer) const {
  if (key.type() != blink::WebCryptoKeyTypePrivate)
    return Status::ErrorUnexpectedKeyType();
  *buffer = PrivateKeyNss::Cast(key)->pkcs8_data();
  return Status::Success();
}

Status RsaHashedAlgorithm::ExportKeySpki(const blink::WebCryptoKey& key,
                                         std::vector<uint8_t>* buffer) const {
  if (key.type() != blink::WebCryptoKeyTypePublic)
    return Status::ErrorUnexpectedKeyType();
  *buffer = PublicKeyNss::Cast(key)->spki_data();
  return Status::Success();
}

Status RsaHashedAlgorithm::ImportKeyJwk(
    const CryptoData& key_data,
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usage_mask,
    blink::WebCryptoKey* key) const {
  const char* jwk_algorithm =
      GetJwkAlgorithm(algorithm.rsaHashedImportParams()->hash().id());

  if (!jwk_algorithm)
    return Status::ErrorUnexpected();

  JwkRsaInfo jwk;
  Status status =
      ReadRsaKeyJwk(key_data, jwk_algorithm, extractable, usage_mask, &jwk);
  if (status.IsError())
    return status;

  // Once the key type is known, verify the usages.
  status = CheckKeyCreationUsages(
      jwk.is_private_key ? all_private_key_usages_ : all_public_key_usages_,
      usage_mask);
  if (status.IsError())
    return Status::ErrorCreateKeyBadUsages();

  return jwk.is_private_key
             ? ImportRsaPrivateKey(algorithm, extractable, usage_mask, jwk, key)
             : ImportRsaPublicKey(algorithm,
                                  extractable,
                                  usage_mask,
                                  CryptoData(jwk.n),
                                  CryptoData(jwk.e),
                                  key);
}

Status RsaHashedAlgorithm::ExportKeyJwk(const blink::WebCryptoKey& key,
                                        std::vector<uint8_t>* buffer) const {
  const char* jwk_algorithm =
      GetJwkAlgorithm(key.algorithm().rsaHashedParams()->hash().id());

  if (!jwk_algorithm)
    return Status::ErrorUnexpected();

  switch (key.type()) {
    case blink::WebCryptoKeyTypePublic: {
      SECKEYPublicKey* nss_key = PublicKeyNss::Cast(key)->key();
      if (nss_key->keyType != rsaKey)
        return Status::ErrorUnsupported();

      WriteRsaPublicKeyJwk(SECItemToCryptoData(nss_key->u.rsa.modulus),
                           SECItemToCryptoData(nss_key->u.rsa.publicExponent),
                           jwk_algorithm,
                           key.extractable(),
                           key.usages(),
                           buffer);

      return Status::Success();
    }

    case blink::WebCryptoKeyTypePrivate: {
      SECKEYPrivateKey* nss_key = PrivateKeyNss::Cast(key)->key();
      RSAPrivateKey key_props = {};
      scoped_ptr<RSAPrivateKey, FreeRsaPrivateKey> free_private_key(&key_props);

      if (!InitRSAPrivateKey(nss_key, &key_props))
        return Status::OperationError();

      WriteRsaPrivateKeyJwk(SECItemToCryptoData(key_props.modulus),
                            SECItemToCryptoData(key_props.public_exponent),
                            SECItemToCryptoData(key_props.private_exponent),
                            SECItemToCryptoData(key_props.prime1),
                            SECItemToCryptoData(key_props.prime2),
                            SECItemToCryptoData(key_props.exponent1),
                            SECItemToCryptoData(key_props.exponent2),
                            SECItemToCryptoData(key_props.coefficient),
                            jwk_algorithm,
                            key.extractable(),
                            key.usages(),
                            buffer);

      return Status::Success();
    }
    default:
      return Status::ErrorUnexpected();
  }
}

}  // namespace webcrypto

}  // namespace content
