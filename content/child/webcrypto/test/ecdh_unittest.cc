// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/stl_util.h"
#include "content/child/webcrypto/algorithm_dispatch.h"
#include "content/child/webcrypto/crypto_data.h"
#include "content/child/webcrypto/jwk.h"
#include "content/child/webcrypto/status.h"
#include "content/child/webcrypto/test/test_helpers.h"
#include "content/child/webcrypto/webcrypto_util.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithmParams.h"
#include "third_party/WebKit/public/platform/WebCryptoKeyAlgorithm.h"

namespace content {

namespace webcrypto {

namespace {

bool SupportsEcdh() {
#if defined(USE_OPENSSL)
  return true;
#else
  LOG(ERROR) << "Skipping ECDH test because unsupported";
  return false;
#endif
}

// TODO(eroman): Test passing an RSA public key instead of ECDH key.
// TODO(eroman): Test passing an ECDSA public key

blink::WebCryptoAlgorithm CreateEcdhImportAlgorithm(
    blink::WebCryptoNamedCurve named_curve) {
  return CreateEcImportAlgorithm(blink::WebCryptoAlgorithmIdEcdh, named_curve);
}

blink::WebCryptoAlgorithm CreateEcdhDeriveParams(
    const blink::WebCryptoKey& public_key) {
  return blink::WebCryptoAlgorithm::adoptParamsAndCreate(
      blink::WebCryptoAlgorithmIdEcdh,
      new blink::WebCryptoEcdhKeyDeriveParams(public_key));
}

TEST(WebCryptoEcdhTest, VerifyKnownAnswer) {
  if (!SupportsEcdh())
    return;

  scoped_ptr<base::ListValue> tests;
  ASSERT_TRUE(ReadJsonTestFileToList("ecdh.json", &tests));

  for (size_t test_index = 0; test_index < tests->GetSize(); ++test_index) {
    SCOPED_TRACE(test_index);

    const base::DictionaryValue* test;
    ASSERT_TRUE(tests->GetDictionary(test_index, &test));

    // Import the public key.
    const base::DictionaryValue* public_key_json = NULL;
    EXPECT_TRUE(test->GetDictionary("public_key", &public_key_json));
    blink::WebCryptoNamedCurve curve =
        GetCurveNameFromDictionary(public_key_json, "crv");
    blink::WebCryptoKey public_key;
    ASSERT_EQ(
        Status::Success(),
        ImportKey(blink::WebCryptoKeyFormatJwk,
                  CryptoData(MakeJsonVector(*public_key_json)),
                  CreateEcdhImportAlgorithm(curve), true, 0, &public_key));

    // Import the private key.
    const base::DictionaryValue* private_key_json = NULL;
    EXPECT_TRUE(test->GetDictionary("private_key", &private_key_json));
    curve = GetCurveNameFromDictionary(private_key_json, "crv");
    blink::WebCryptoKey private_key;
    ASSERT_EQ(Status::Success(),
              ImportKey(blink::WebCryptoKeyFormatJwk,
                        CryptoData(MakeJsonVector(*private_key_json)),
                        CreateEcdhImportAlgorithm(curve), true,
                        blink::WebCryptoKeyUsageDeriveBits, &private_key));

    // Now try to derive bytes.
    std::vector<uint8_t> derived_bytes;
    int length_bits = 0;
    ASSERT_TRUE(test->GetInteger("length_bits", &length_bits));

    // If the test didn't specify an error, that implies it expects success.
    std::string expected_error = "Success";
    test->GetString("error", &expected_error);

    Status status = DeriveBits(CreateEcdhDeriveParams(public_key), private_key,
                               length_bits, &derived_bytes);
    ASSERT_EQ(expected_error, StatusToString(status));
    if (status.IsError())
      continue;

    std::vector<uint8_t> expected_bytes =
        GetBytesFromHexString(test, "derived_bytes");

    EXPECT_EQ(CryptoData(expected_bytes), CryptoData(derived_bytes));
  }
}

}  // namespace

}  // namespace webcrypto

}  // namespace content
