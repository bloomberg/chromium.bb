// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/crypto/rsa_private_key.h"

#include <cryptohi.h>
#include <keyhi.h>
#include <pk11pub.h>

#include <iostream>
#include <list>

#include "base/logging.h"
#include "base/nss_init.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"

// TODO(rafaelw): Consider refactoring common functions and definitions from
// rsa_private_key_win.cc or using NSS's ASN.1 encoder.
namespace {

// ASN.1 encoding of the AlgorithmIdentifier from PKCS #8.
const uint8 kRsaAlgorithmIdentifier[] = {
  0x30, 0x0D, 0x06, 0x09, 0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x01,
  0x05, 0x00
};

// ASN.1 tags for some types we use.
const uint8 kSequenceTag = 0x30;
const uint8 kIntegerTag = 0x02;
const uint8 kNullTag = 0x05;
const uint8 kOctetStringTag = 0x04;

static void PrependBytesInOrder(uint8* val, int start, int num_bytes,
                                std::list<uint8>* data) {
  while(num_bytes > start) {
    --num_bytes;
    data->push_front(val[num_bytes]);
  }
}

// Helper to prepend an ASN.1 length field.
static void PrependLength(size_t size, std::list<uint8>* data) {
  // The high bit is used to indicate whether additional octets are needed to
  // represent the length.
  if (size < 0x80) {
    data->push_front(static_cast<uint8>(size));
  } else {
    uint8 num_bytes = 0;
    while (size > 0) {
      data->push_front(static_cast<uint8>(size & 0xFF));
      size >>= 8;
      num_bytes++;
    }
    CHECK(num_bytes <= 4);
    data->push_front(0x80 | num_bytes);
  }
}

// Helper to prepend an ASN.1 type header.
static void PrependTypeHeaderAndLength(uint8 type, uint32 length,
                                       std::list<uint8>* output) {
  PrependLength(length, output);
  output->push_front(type);
}

// Helper to prepend an ASN.1 integer.
static void PrependInteger(uint8* val, int num_bytes, std::list<uint8>* data) {
  // ASN.1 integers are unpadded byte arrays, so skip any null padding bytes
  // from the most-significant end of the integer.
  int start = 0;
  while (start < (num_bytes - 1) && val[start] == 0x00)
    start++;

  PrependBytesInOrder(val, start, num_bytes, data);

  // ASN.1 integers are signed. To encode a positive integer whose sign bit
  // (the most significant bit) would otherwise be set and make the number
  // negative, ASN.1 requires a leading null byte to force the integer to be
  // positive.
  if ((val[start] & 0x80) != 0) {
    data->push_front(0x00);
    num_bytes++;
  }

  PrependTypeHeaderAndLength(kIntegerTag, num_bytes, data);
}

static bool ReadAttributeAndPrependInteger(SECKEYPrivateKey* key,
                                           CK_ATTRIBUTE_TYPE type,
                                           std::list<uint8>* output) {
  SECItem item;
  SECStatus rv;
  rv = PK11_ReadRawAttribute(PK11_TypePrivKey, key, type, &item);
  if (rv != SECSuccess) {
    NOTREACHED();
    return false;
  }

  PrependInteger(item.data, item.len, output);
  SECITEM_FreeItem(&item, PR_FALSE);
  return true;
}

}  // namespace


namespace base {

// static
RSAPrivateKey* RSAPrivateKey::Create(uint16 num_bits) {
  scoped_ptr<RSAPrivateKey> result(new RSAPrivateKey);

  PK11SlotInfo *slot = PK11_GetInternalSlot();
  if (!slot)
    return NULL;

  PK11RSAGenParams param;
  param.keySizeInBits = num_bits;
  param.pe = 65537L;
  result->key_ = PK11_GenerateKeyPair(slot, CKM_RSA_PKCS_KEY_PAIR_GEN, &param,
      &result->public_key_, PR_FALSE, PR_FALSE, NULL);
  PK11_FreeSlot(slot);
  if (!result->key_)
    return NULL;

  return result.release();
}

// static
RSAPrivateKey* RSAPrivateKey::CreateFromPrivateKeyInfo(
    const std::vector<uint8>& input) {
  scoped_ptr<RSAPrivateKey> result(new RSAPrivateKey);

  PK11SlotInfo *slot = PK11_GetInternalSlot();
  if (!slot)
    return NULL;

  SECItem der_private_key_info;
  der_private_key_info.data = const_cast<unsigned char*>(&input.front());
  der_private_key_info.len = input.size();
  SECStatus rv =  PK11_ImportDERPrivateKeyInfoAndReturnKey(slot,
      &der_private_key_info, NULL, NULL, PR_FALSE, PR_FALSE,
      KU_DIGITAL_SIGNATURE, &result->key_, NULL);
  PK11_FreeSlot(slot);
  if (rv != SECSuccess) {
    NOTREACHED();
    return NULL;
  }

  result->public_key_ = SECKEY_ConvertToPublicKey(result->key_);
  if (!result->public_key_) {
    NOTREACHED();
    return NULL;
  }

  return result.release();
}

RSAPrivateKey::RSAPrivateKey() : key_(NULL), public_key_(NULL) {
  EnsureNSSInit();
}

RSAPrivateKey::~RSAPrivateKey() {
  if (key_)
    SECKEY_DestroyPrivateKey(key_);
  if (public_key_)
    SECKEY_DestroyPublicKey(public_key_);
}

bool RSAPrivateKey::ExportPrivateKey(std::vector<uint8>* output) {
  std::list<uint8> content;

  // Version (always zero)
  uint8 version = 0;

  // Manually read the component attributes of the private key and build up the
  // output in reverse order to prevent having to do copies to figure out the
  // length.
  if (!ReadAttributeAndPrependInteger(key_, CKA_COEFFICIENT, &content) ||
      !ReadAttributeAndPrependInteger(key_, CKA_EXPONENT_2, &content) ||
      !ReadAttributeAndPrependInteger(key_, CKA_EXPONENT_1, &content) ||
      !ReadAttributeAndPrependInteger(key_, CKA_PRIME_2, &content) ||
      !ReadAttributeAndPrependInteger(key_, CKA_PRIME_1, &content) ||
      !ReadAttributeAndPrependInteger(key_, CKA_PRIVATE_EXPONENT, &content) ||
      !ReadAttributeAndPrependInteger(key_, CKA_PUBLIC_EXPONENT, &content) ||
      !ReadAttributeAndPrependInteger(key_, CKA_MODULUS, &content)) {
    NOTREACHED();
    return false;
  }
  PrependInteger(&version, 1, &content);
  PrependTypeHeaderAndLength(kSequenceTag, content.size(), &content);
  PrependTypeHeaderAndLength(kOctetStringTag, content.size(), &content);

  // RSA algorithm OID
  for (size_t i = sizeof(kRsaAlgorithmIdentifier); i > 0; --i)
    content.push_front(kRsaAlgorithmIdentifier[i - 1]);

  PrependInteger(&version, 1, &content);
  PrependTypeHeaderAndLength(kSequenceTag, content.size(), &content);

  // Copy everying into the output.
  output->reserve(content.size());
  for (std::list<uint8>::iterator i = content.begin(); i != content.end(); ++i)
    output->push_back(*i);

  return true;
}

bool RSAPrivateKey::ExportPublicKey(std::vector<uint8>* output) {
  SECItem* der_pubkey = PK11_DEREncodePublicKey(public_key_);
  if (!der_pubkey) {
    NOTREACHED();
    return false;
  }

  for (size_t i = 0; i < der_pubkey->len; ++i)
    output->push_back(der_pubkey->data[i]);

  SECITEM_FreeItem(der_pubkey, PR_TRUE);
  return true;
}

}  // namespace base
