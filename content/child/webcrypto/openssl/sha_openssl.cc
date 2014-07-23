// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>
#include <openssl/evp.h>
#include <openssl/sha.h>

#include "base/logging.h"
#include "base/stl_util.h"
#include "content/child/webcrypto/algorithm_implementation.h"
#include "content/child/webcrypto/crypto_data.h"
#include "content/child/webcrypto/openssl/util_openssl.h"
#include "content/child/webcrypto/status.h"
#include "content/child/webcrypto/webcrypto_util.h"
#include "crypto/openssl_util.h"
#include "crypto/scoped_openssl_types.h"

namespace content {

namespace webcrypto {

namespace {

// Implementation of blink::WebCryptoDigester, an internal Blink detail not
// part of WebCrypto, that allows chunks of data to be streamed in before
// computing a SHA-* digest (as opposed to ShaImplementation, which computes
// digests over complete messages)
class DigestorOpenSsl : public blink::WebCryptoDigestor {
 public:
  explicit DigestorOpenSsl(blink::WebCryptoAlgorithmId algorithm_id)
      : initialized_(false),
        digest_context_(EVP_MD_CTX_create()),
        algorithm_id_(algorithm_id) {}

  virtual bool consume(const unsigned char* data, unsigned int size) {
    return ConsumeWithStatus(data, size).IsSuccess();
  }

  Status ConsumeWithStatus(const unsigned char* data, unsigned int size) {
    crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);
    Status error = Init();
    if (!error.IsSuccess())
      return error;

    if (!EVP_DigestUpdate(digest_context_.get(), data, size))
      return Status::OperationError();

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
    const int hash_expected_size = EVP_MD_CTX_size(digest_context_.get());
    result->resize(hash_expected_size);
    unsigned char* const hash_buffer = vector_as_array(result);
    unsigned int hash_buffer_size;  // ignored
    return FinishInternal(hash_buffer, &hash_buffer_size);
  }

 private:
  Status Init() {
    if (initialized_)
      return Status::Success();

    const EVP_MD* digest_algorithm = GetDigest(algorithm_id_);
    if (!digest_algorithm)
      return Status::ErrorUnexpected();

    if (!digest_context_.get())
      return Status::OperationError();

    if (!EVP_DigestInit_ex(digest_context_.get(), digest_algorithm, NULL))
      return Status::OperationError();

    initialized_ = true;
    return Status::Success();
  }

  Status FinishInternal(unsigned char* result, unsigned int* result_size) {
    crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);
    Status error = Init();
    if (!error.IsSuccess())
      return error;

    const int hash_expected_size = EVP_MD_CTX_size(digest_context_.get());
    if (hash_expected_size <= 0)
      return Status::ErrorUnexpected();
    DCHECK_LE(hash_expected_size, EVP_MAX_MD_SIZE);

    if (!EVP_DigestFinal_ex(digest_context_.get(), result, result_size) ||
        static_cast<int>(*result_size) != hash_expected_size)
      return Status::OperationError();

    return Status::Success();
  }

  bool initialized_;
  crypto::ScopedEVP_MD_CTX digest_context_;
  blink::WebCryptoAlgorithmId algorithm_id_;
  unsigned char result_[EVP_MAX_MD_SIZE];
};

class ShaImplementation : public AlgorithmImplementation {
 public:
  virtual Status Digest(const blink::WebCryptoAlgorithm& algorithm,
                        const CryptoData& data,
                        std::vector<uint8_t>* buffer) const OVERRIDE {
    DigestorOpenSsl digestor(algorithm.id());
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
  return scoped_ptr<blink::WebCryptoDigestor>(new DigestorOpenSsl(algorithm));
}

}  // namespace webcrypto

}  // namespace content
