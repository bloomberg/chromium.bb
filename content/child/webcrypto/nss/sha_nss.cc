// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sechash.h>
#include <vector>

#include "content/child/webcrypto/algorithm_implementation.h"
#include "content/child/webcrypto/crypto_data.h"
#include "content/child/webcrypto/nss/util_nss.h"
#include "content/child/webcrypto/status.h"
#include "content/child/webcrypto/webcrypto_util.h"
#include "crypto/nss_util.h"
#include "crypto/scoped_nss_types.h"

namespace content {

namespace webcrypto {

namespace {

HASH_HashType WebCryptoAlgorithmToNSSHashType(
    blink::WebCryptoAlgorithmId algorithm) {
  switch (algorithm) {
    case blink::WebCryptoAlgorithmIdSha1:
      return HASH_AlgSHA1;
    case blink::WebCryptoAlgorithmIdSha256:
      return HASH_AlgSHA256;
    case blink::WebCryptoAlgorithmIdSha384:
      return HASH_AlgSHA384;
    case blink::WebCryptoAlgorithmIdSha512:
      return HASH_AlgSHA512;
    default:
      // Not a digest algorithm.
      return HASH_AlgNULL;
  }
}

// Implementation of blink::WebCryptoDigester, an internal Blink detail not
// part of WebCrypto, that allows chunks of data to be streamed in before
// computing a SHA-* digest (as opposed to ShaImplementation, which computes
// digests over complete messages)
class DigestorNSS : public blink::WebCryptoDigestor {
 public:
  explicit DigestorNSS(blink::WebCryptoAlgorithmId algorithm_id)
      : hash_context_(NULL), algorithm_id_(algorithm_id) {}

  virtual ~DigestorNSS() {
    if (!hash_context_)
      return;

    HASH_Destroy(hash_context_);
    hash_context_ = NULL;
  }

  virtual bool consume(const unsigned char* data, unsigned int size) {
    return ConsumeWithStatus(data, size).IsSuccess();
  }

  Status ConsumeWithStatus(const unsigned char* data, unsigned int size) {
    // Initialize everything if the object hasn't been initialized yet.
    if (!hash_context_) {
      Status error = Init();
      if (!error.IsSuccess())
        return error;
    }

    HASH_Update(hash_context_, data, size);

    return Status::Success();
  }

  virtual bool finish(unsigned char*& result_data,
                      unsigned int& result_data_size) {
    Status error = FinishInternal(result_, &result_data_size);
    if (!error.IsSuccess())
      return false;
    result_data = result_;
    return true;
  }

  Status FinishWithVectorAndStatus(std::vector<uint8_t>* result) {
    if (!hash_context_)
      return Status::ErrorUnexpected();

    unsigned int result_length = HASH_ResultLenContext(hash_context_);
    result->resize(result_length);
    unsigned char* digest = Uint8VectorStart(result);
    unsigned int digest_size;  // ignored
    return FinishInternal(digest, &digest_size);
  }

 private:
  Status Init() {
    HASH_HashType hash_type = WebCryptoAlgorithmToNSSHashType(algorithm_id_);

    if (hash_type == HASH_AlgNULL)
      return Status::ErrorUnsupported();

    hash_context_ = HASH_Create(hash_type);
    if (!hash_context_)
      return Status::OperationError();

    HASH_Begin(hash_context_);

    return Status::Success();
  }

  Status FinishInternal(unsigned char* result, unsigned int* result_size) {
    if (!hash_context_) {
      Status error = Init();
      if (!error.IsSuccess())
        return error;
    }

    unsigned int hash_result_length = HASH_ResultLenContext(hash_context_);
    DCHECK_LE(hash_result_length, static_cast<size_t>(HASH_LENGTH_MAX));

    HASH_End(hash_context_, result, result_size, hash_result_length);

    if (*result_size != hash_result_length)
      return Status::ErrorUnexpected();
    return Status::Success();
  }

  HASHContext* hash_context_;
  blink::WebCryptoAlgorithmId algorithm_id_;
  unsigned char result_[HASH_LENGTH_MAX];
};

class ShaImplementation : public AlgorithmImplementation {
 public:
  virtual Status Digest(const blink::WebCryptoAlgorithm& algorithm,
                        const CryptoData& data,
                        std::vector<uint8_t>* buffer) const OVERRIDE {
    DigestorNSS digestor(algorithm.id());
    Status error = digestor.ConsumeWithStatus(data.bytes(), data.byte_length());
    // http://crbug.com/366427: the spec does not define any other failures for
    // digest, so none of the subsequent errors are spec compliant.
    if (!error.IsSuccess())
      return error;
    return digestor.FinishWithVectorAndStatus(buffer);
  }
};

}  // namespace

AlgorithmImplementation* CreatePlatformShaImplementation() {
  return new ShaImplementation();
}

scoped_ptr<blink::WebCryptoDigestor> CreatePlatformDigestor(
    blink::WebCryptoAlgorithmId algorithm) {
  return scoped_ptr<blink::WebCryptoDigestor>(new DigestorNSS(algorithm));
}

}  // namespace webcrypto

}  // namespace content
