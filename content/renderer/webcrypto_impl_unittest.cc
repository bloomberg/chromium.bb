// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webcrypto_impl.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_number_conversions.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/renderer_webkitplatformsupport_impl.h"
#include "content/renderer/webcrypto_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebArrayBuffer.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithm.h"

namespace content {

const WebKit::WebCryptoAlgorithmId kAlgorithmIds[] = {
    WebKit::WebCryptoAlgorithmIdSha1,
    WebKit::WebCryptoAlgorithmIdSha224,
    WebKit::WebCryptoAlgorithmIdSha256,
    WebKit::WebCryptoAlgorithmIdSha384,
    WebKit::WebCryptoAlgorithmIdSha512
};

class WebCryptoImplTest : public testing::Test, public WebCryptoImpl {
};

TEST_F(WebCryptoImplTest, DigestSampleSets) {
  // The results are stored here in hex format for readability.
  //
  // TODO(bryaneyler): Eventually, all these sample test sets should be replaced
  // with the sets here: http://csrc.nist.gov/groups/STM/cavp/index.html#03
  struct {
    const char* input;
    size_t input_length;
    const char* hex_result[arraysize(kAlgorithmIds)];
  } input_set[] = {
    {
      "", 0,
      {
        // echo -n "" | sha1sum
        "da39a3ee5e6b4b0d3255bfef95601890afd80709",
        // echo -n "" | sha224sum
        "d14a028c2a3a2bc9476102bb288234c415a2b01f828ea62ac5b3e42f",
        // echo -n "" | sha256sum
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
        // echo -n "" | sha384sum
        "38b060a751ac96384cd9327eb1b1e36a21fdb71114be07434c0cc7bf63f6e1da274e"
        "debfe76f65fbd51ad2f14898b95b",
        // echo -n "" | sha512sum
        "cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0"
        "d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e",
      },
    },
    {
      "\000", 1,
      {
        // echo -n -e "\000" | sha1sum
        "5ba93c9db0cff93f52b521d7420e43f6eda2784f",
        // echo -n -e "\000" | sha224sum
        "fff9292b4201617bdc4d3053fce02734166a683d7d858a7f5f59b073",
        // echo -n -e "\000" | sha256sum
        "6e340b9cffb37a989ca544e6bb780a2c78901d3fb33738768511a30617afa01d",
        // echo -n -e "\000" | sha384sum
        "bec021b4f368e3069134e012c2b4307083d3a9bdd206e24e5f0d86e13d6636655933"
        "ec2b413465966817a9c208a11717",
        // echo -n -e "\000" | sha512sum
        "b8244d028981d693af7b456af8efa4cad63d282e19ff14942c246e50d9351d22704a"
        "802a71c3580b6370de4ceb293c324a8423342557d4e5c38438f0e36910ee",
      },
    },
    {
      "\000\001\002\003\004\005", 6,
      {
        // echo -n -e "\000\001\002\003\004\005" | sha1sum
        "868460d98d09d8bbb93d7b6cdd15cc7fbec676b9",
        // echo -n -e "\000\001\002\003\004\005" | sha224sum
        "7d92e7f1cad1818ed1d13ab41f04ebabfe1fef6bb4cbeebac34c29bc",
        // echo -n -e "\000\001\002\003\004\005" | sha256sum
        "17e88db187afd62c16e5debf3e6527cd006bc012bc90b51a810cd80c2d511f43",
        // echo -n -e "\000\001\002\003\004\005" | sha384sum
        "79f4738706fce9650ac60266675c3cd07298b09923850d525604d040e6e448adc7dc"
        "22780d7e1b95bfeaa86a678e4552",
        // echo -n -e "\000\001\002\003\004\005" | sha512sum
        "2f3831bccc94cf061bcfa5f8c23c1429d26e3bc6b76edad93d9025cb91c903af6cf9"
        "c935dc37193c04c2c66e7d9de17c358284418218afea2160147aaa912f4c",
      },
    },
  };

  for (size_t id_index = 0; id_index < arraysize(kAlgorithmIds); id_index++) {
    WebKit::WebCryptoAlgorithm algorithm(
        WebKit::WebCryptoAlgorithm::adoptParamsAndCreate(
            kAlgorithmIds[id_index], NULL));

    for (size_t set_index = 0;
         set_index < ARRAYSIZE_UNSAFE(input_set);
         set_index++) {
      WebKit::WebArrayBuffer array_buffer;

      WebCryptoImpl crypto;
      crypto.DigestInternal(
          algorithm,
          reinterpret_cast<const unsigned char*>(input_set[set_index].input),
          input_set[set_index].input_length,
          &array_buffer);

      // Ignore case, it's checking the hex value.
      EXPECT_STRCASEEQ(
          input_set[set_index].hex_result[id_index],
          base::HexEncode(
              array_buffer.data(), array_buffer.byteLength()).c_str());
    }
  }
}

}  // namespace content
