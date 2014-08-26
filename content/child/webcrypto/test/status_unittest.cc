// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/stl_util.h"
#include "content/child/webcrypto/algorithm_dispatch.h"
#include "content/child/webcrypto/crypto_data.h"
#include "content/child/webcrypto/status.h"
#include "content/child/webcrypto/test/test_helpers.h"
#include "content/child/webcrypto/webcrypto_util.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithm.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithmParams.h"
#include "third_party/WebKit/public/platform/WebCryptoKeyAlgorithm.h"

namespace content {

namespace webcrypto {

namespace {

// Tests several Status objects against their expected hard coded values, as
// well as ensuring that comparison of Status objects works.
// Comparison should take into account both the error details, as well as the
// error type.
TEST(WebCryptoStatusTest, Basic) {
  // Even though the error message is the same, these should not be considered
  // the same by the tests because the error type is different.
  EXPECT_NE(Status::DataError(), Status::OperationError());
  EXPECT_NE(Status::Success(), Status::OperationError());

  EXPECT_EQ(Status::Success(), Status::Success());
  EXPECT_EQ(Status::ErrorJwkPropertyWrongType("kty", "string"),
            Status::ErrorJwkPropertyWrongType("kty", "string"));

  Status status = Status::Success();

  EXPECT_FALSE(status.IsError());
  EXPECT_EQ("", status.error_details());

  status = Status::OperationError();
  EXPECT_TRUE(status.IsError());
  EXPECT_EQ("", status.error_details());
  EXPECT_EQ(blink::WebCryptoErrorTypeOperation, status.error_type());

  status = Status::DataError();
  EXPECT_TRUE(status.IsError());
  EXPECT_EQ("", status.error_details());
  EXPECT_EQ(blink::WebCryptoErrorTypeData, status.error_type());

  status = Status::ErrorUnsupported();
  EXPECT_TRUE(status.IsError());
  EXPECT_EQ("The requested operation is unsupported", status.error_details());
  EXPECT_EQ(blink::WebCryptoErrorTypeNotSupported, status.error_type());

  status = Status::ErrorJwkPropertyMissing("kty");
  EXPECT_TRUE(status.IsError());
  EXPECT_EQ("The required JWK property \"kty\" was missing",
            status.error_details());
  EXPECT_EQ(blink::WebCryptoErrorTypeData, status.error_type());

  status = Status::ErrorJwkPropertyWrongType("kty", "string");
  EXPECT_TRUE(status.IsError());
  EXPECT_EQ("The JWK property \"kty\" must be a string",
            status.error_details());
  EXPECT_EQ(blink::WebCryptoErrorTypeData, status.error_type());

  status = Status::ErrorJwkBase64Decode("n");
  EXPECT_TRUE(status.IsError());
  EXPECT_EQ("The JWK property \"n\" could not be base64 decoded",
            status.error_details());
  EXPECT_EQ(blink::WebCryptoErrorTypeData, status.error_type());
}

}  // namespace

}  // namespace webcrypto

}  // namespace content
