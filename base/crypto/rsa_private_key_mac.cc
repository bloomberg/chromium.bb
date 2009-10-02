// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/crypto/rsa_private_key.h"

#include <list>

#include "base/crypto/cssm_init.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"

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
const uint8 kBitStringTag = 0x03;

// TODO(hawk): Move the App* functions into a shared location,
// perhaps cssm_init.cc.
void* AppMalloc(CSSM_SIZE size, void *alloc_ref) {
  return malloc(size);
}

void AppFree(void* mem_ptr, void* alloc_ref) {
  free(mem_ptr);
}

void* AppRealloc(void* ptr, CSSM_SIZE size, void* alloc_ref) {
  return realloc(ptr, size);
}

void* AppCalloc(uint32 num, CSSM_SIZE size, void* alloc_ref) {
  return calloc(num, size);
}

const CSSM_API_MEMORY_FUNCS mem_funcs = {
  AppMalloc,
  AppFree,
  AppRealloc,
  AppCalloc,
  NULL
};

// Helper for error handling during key import.
#define READ_ASSERT(truth) \
  if (!(truth)) { \
    NOTREACHED(); \
    return false; \
  }

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

// Helper to prepend an ASN.1 bit string
static void PrependBitString(uint8* val, int num_bytes,
                             std::list<uint8>* output) {
  // Start with the data.
  PrependBytesInOrder(val, 0, num_bytes, output);
  // Zero unused bits.
  output->push_front(0);
  // Add the length.
  PrependLength(num_bytes + 1, output);
  // Finally, add the bit string tag.
  output->push_front(kBitStringTag);
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

// Read an ASN.1 length field. This also checks that the length does not extend
// beyond |end|.
static bool ReadLength(uint8** pos, uint8* end, uint32* result) {
  READ_ASSERT(*pos < end);
  int length = 0;

  // If the MSB is not set, the length is just the byte itself.
  if (!(**pos & 0x80)) {
    length = **pos;
    (*pos)++;
  } else {
    // Otherwise, the lower 7 indicate the length of the length.
    int length_of_length = **pos & 0x7F;
    READ_ASSERT(length_of_length <= 4);
    (*pos)++;
    READ_ASSERT(*pos + length_of_length < end);

    length = 0;
    for (int i = 0; i < length_of_length; ++i) {
      length <<= 8;
      length |= **pos;
      (*pos)++;
    }
  }

  READ_ASSERT(*pos + length <= end);
  if (result) *result = length;
  return true;
}

// Read an ASN.1 type header and its length.
static bool ReadTypeHeaderAndLength(uint8** pos, uint8* end,
                                    uint8 expected_tag, uint32* length) {
  READ_ASSERT(*pos < end);
  READ_ASSERT(**pos == expected_tag);
  (*pos)++;

  return ReadLength(pos, end, length);
}

// Read an ASN.1 sequence declaration. This consumes the type header and length
// field, but not the contents of the sequence.
static bool ReadSequence(uint8** pos, uint8* end) {
  return ReadTypeHeaderAndLength(pos, end, kSequenceTag, NULL);
}

// Read the RSA AlgorithmIdentifier.
static bool ReadAlgorithmIdentifier(uint8** pos, uint8* end) {
  READ_ASSERT(*pos + sizeof(kRsaAlgorithmIdentifier) < end);
  READ_ASSERT(memcmp(*pos, kRsaAlgorithmIdentifier,
                     sizeof(kRsaAlgorithmIdentifier)) == 0);
  (*pos) += sizeof(kRsaAlgorithmIdentifier);
  return true;
}

// Read one of the two version fields in PrivateKeyInfo.
static bool ReadVersion(uint8** pos, uint8* end) {
  uint32 length = 0;
  if (!ReadTypeHeaderAndLength(pos, end, kIntegerTag, &length))
    return false;

  // The version should be zero.
  for (uint32 i = 0; i < length; ++i) {
    READ_ASSERT(**pos == 0x00);
    (*pos)++;
  }

  return true;
}

// Read an ASN.1 integer.
static bool ReadInteger(uint8** pos, uint8* end, std::vector<uint8>* out) {
  uint32 length = 0;
  if (!ReadTypeHeaderAndLength(pos, end, kIntegerTag, &length) || !length)
    return false;

  // The first byte can be zero to force positiveness. We can ignore this.
  if (**pos == 0x00) {
    ++(*pos);
    --length;
  }

  if (length)
    out->insert(out->end(), *pos, (*pos) + length);

  (*pos) += length;
  return true;
}

}  // namespace

namespace base {

// static
RSAPrivateKey* RSAPrivateKey::Create(uint16 num_bits) {
  scoped_ptr<RSAPrivateKey> result(new RSAPrivateKey);

  CSSM_CC_HANDLE cc_handle;
  CSSM_RETURN crtn;
  crtn = CSSM_CSP_CreateKeyGenContext(result->csp_handle(), CSSM_ALGID_RSA,
                                      num_bits, NULL, NULL, NULL, NULL, NULL,
                                      &cc_handle);
  if (crtn) {
    NOTREACHED() << "CSSM_CSP_CreateKeyGenContext failed: " << crtn;
    return NULL;
  }

  CSSM_KEY public_key;
  memset(&public_key, 0, sizeof(CSSM_KEY));
  CSSM_DATA label = { 9,
      const_cast<uint8*>(reinterpret_cast<const uint8*>("temp_key")) };
  crtn = CSSM_GenerateKeyPair(cc_handle,
      CSSM_KEYUSE_VERIFY,
      CSSM_KEYATTR_RETURN_DATA | CSSM_KEYATTR_EXTRACTABLE, &label,
      &public_key, CSSM_KEYUSE_SIGN,
      CSSM_KEYATTR_RETURN_DATA | CSSM_KEYATTR_EXTRACTABLE, &label, NULL,
      result->key());
  CSSM_DeleteContext(cc_handle);
  if (crtn) {
    NOTREACHED() << "CSSM_CSP_CreateKeyGenContext failed: " << crtn;
    return NULL;
  }

  // Public key is not needed.
  CSSM_FreeKey(result->csp_handle(), NULL, &public_key, CSSM_FALSE);

  return result.release();
}

// static
RSAPrivateKey* RSAPrivateKey::CreateFromPrivateKeyInfo(
    const std::vector<uint8>& input) {
  if (input.empty())
    return NULL;

  scoped_ptr<RSAPrivateKey> result(new RSAPrivateKey);

  CSSM_KEY key;
  memset(&key, 0, sizeof(key));
  key.KeyData.Data = const_cast<uint8*>(&input.front());
  key.KeyData.Length = input.size();
  key.KeyHeader.Format = CSSM_KEYBLOB_RAW_FORMAT_PKCS8;
  key.KeyHeader.HeaderVersion = CSSM_KEYHEADER_VERSION;
  key.KeyHeader.BlobType = CSSM_KEYBLOB_RAW;
  key.KeyHeader.AlgorithmId = CSSM_ALGID_RSA;
  key.KeyHeader.KeyClass = CSSM_KEYCLASS_PRIVATE_KEY;
  key.KeyHeader.KeyAttr = CSSM_KEYATTR_EXTRACTABLE;
  key.KeyHeader.KeyUsage = CSSM_KEYUSE_ANY;

  CSSM_KEY_SIZE key_size;
  CSSM_RETURN crtn;
  crtn = CSSM_QueryKeySizeInBits(result->csp_handle(), NULL, &key, &key_size);
  if (crtn) {
    NOTREACHED() << "CSSM_QueryKeySizeInBits failed: " << crtn;
    return NULL;
  }
  key.KeyHeader.LogicalKeySizeInBits = key_size.LogicalKeySizeInBits;

  // Perform a NULL unwrap operation on the key so that result's key_
  // instance variable points to a key that can be released via CSSM_FreeKey().
  CSSM_ACCESS_CREDENTIALS creds;
  memset(&creds, 0, sizeof(CSSM_ACCESS_CREDENTIALS));
  CSSM_CC_HANDLE cc_handle;
  crtn = CSSM_CSP_CreateSymmetricContext(result->csp_handle(), CSSM_ALGID_NONE,
      CSSM_ALGMODE_NONE, &creds, NULL, NULL, CSSM_PADDING_NONE, 0, &cc_handle);
  if (crtn) {
    NOTREACHED() << "CSSM_CSP_CreateSymmetricContext failed: " << crtn;
    return NULL;
  }
  CSSM_DATA label_data, desc_data = { 0, NULL };
  label_data.Data =
      const_cast<uint8*>(reinterpret_cast<const uint8*>("unwrapped"));
  label_data.Length = 9;
  crtn = CSSM_UnwrapKey(cc_handle, NULL, &key, CSSM_KEYUSE_ANY,
      CSSM_KEYATTR_RETURN_DATA | CSSM_KEYATTR_EXTRACTABLE, &label_data,
      NULL, result->key(), &desc_data);
  if (crtn) {
    NOTREACHED() << "CSSM_UnwrapKey failed: " << crtn;
    return NULL;
  }

  return result.release();
}

RSAPrivateKey::RSAPrivateKey() : csp_handle_(0) {
  memset(&key_, 0, sizeof(key_));

  EnsureCSSMInit();

  static CSSM_VERSION version = {2, 0};
  CSSM_RETURN crtn;
  crtn = CSSM_ModuleAttach(&gGuidAppleCSP, &version, &mem_funcs, 0,
                           CSSM_SERVICE_CSP, 0, CSSM_KEY_HIERARCHY_NONE,
                           NULL, 0, NULL, &csp_handle_);
  DCHECK(crtn == CSSM_OK);
}

RSAPrivateKey::~RSAPrivateKey() {
  if (csp_handle_) {
    if (key_.KeyData.Data) {
      CSSM_FreeKey(csp_handle_, NULL, &key_, CSSM_FALSE);
    }
    CSSM_RETURN crtn = CSSM_ModuleDetach(csp_handle_);
    DCHECK(crtn == CSSM_OK);
  }
}

bool RSAPrivateKey::ExportPrivateKey(std::vector<uint8>* output) {
  if (!key_.KeyData.Data || !key_.KeyData.Length) {
    return false;
  }
  output->clear();
  output->insert(output->end(), key_.KeyData.Data,
                key_.KeyData.Data + key_.KeyData.Length);
  return true;
}

bool RSAPrivateKey::ExportPublicKey(std::vector<uint8>* output) {
  if (!key_.KeyData.Data || !key_.KeyData.Length) {
    return false;
  }

  // Parse the private key info up to the public key values, ignoring
  // the subsequent private key values.
  uint8* src = key_.KeyData.Data;
  uint8* end = src + key_.KeyData.Length;
  std::vector<uint8> modulus;
  std::vector<uint8> public_exponent;
  if (!ReadSequence(&src, end) ||
      !ReadVersion(&src, end) ||
      !ReadAlgorithmIdentifier(&src, end) ||
      !ReadTypeHeaderAndLength(&src, end, kOctetStringTag, NULL) ||
      !ReadSequence(&src, end) ||
      !ReadVersion(&src, end) ||
      !ReadInteger(&src, end, &modulus) ||
      !ReadInteger(&src, end, &public_exponent))
    return false;

  // Create a sequence with the modulus (n) and public exponent (e).
  std::list<uint8> content;
  PrependInteger(&public_exponent[0], static_cast<int>(public_exponent.size()),
                 &content);
  PrependInteger(&modulus[0],  static_cast<int>(modulus.size()), &content);
  PrependTypeHeaderAndLength(kSequenceTag, content.size(), &content);

  // Copy the sequence with n and e into a buffer.
  std::vector<uint8> bit_string;
  for (std::list<uint8>::iterator i = content.begin(); i != content.end(); ++i)
    bit_string.push_back(*i);
  content.clear();
  // Add the sequence as the contents of a bit string.
  PrependBitString(&bit_string[0], static_cast<int>(bit_string.size()),
                   &content);

  // Add the RSA algorithm OID.
  for (size_t i = sizeof(kRsaAlgorithmIdentifier); i > 0; --i)
    content.push_front(kRsaAlgorithmIdentifier[i - 1]);

  // Finally, wrap everything in a sequence.
  PrependTypeHeaderAndLength(kSequenceTag, content.size(), &content);

  // Copy everything into the output.
  output->reserve(content.size());
  for (std::list<uint8>::iterator i = content.begin(); i != content.end(); ++i)
    output->push_back(*i);

  return true;
}

}  // namespace base
