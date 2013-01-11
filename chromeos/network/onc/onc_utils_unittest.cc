// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/onc/onc_utils.h"

#include <string>

#include "base/values.h"
#include "chromeos/network/onc/onc_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace onc {

TEST(ONCDecrypterTest, BrokenEncryptionIterations) {
  scoped_ptr<base::DictionaryValue> encrypted_onc =
      test_utils::ReadTestDictionary("broken-encrypted-iterations.onc");

  scoped_ptr<base::DictionaryValue> decrypted_onc =
      Decrypt("test0000", *encrypted_onc);

  EXPECT_EQ(NULL, decrypted_onc.get());
}

TEST(ONCDecrypterTest, BrokenEncryptionZeroIterations) {
  scoped_ptr<base::DictionaryValue> encrypted_onc =
      test_utils::ReadTestDictionary("broken-encrypted-zero-iterations.onc");

  std::string error;
  scoped_ptr<base::DictionaryValue> decrypted_onc =
      Decrypt("test0000", *encrypted_onc);

  EXPECT_EQ(NULL, decrypted_onc.get());
}

TEST(ONCDecrypterTest, LoadEncryptedOnc) {
  scoped_ptr<base::DictionaryValue> encrypted_onc =
      test_utils::ReadTestDictionary("encrypted.onc");
  scoped_ptr<base::DictionaryValue> expected_decrypted_onc =
      test_utils::ReadTestDictionary("decrypted.onc");

  std::string error;
  scoped_ptr<base::DictionaryValue> actual_decrypted_onc =
      Decrypt("test0000", *encrypted_onc);

  base::DictionaryValue emptyDict;
  EXPECT_TRUE(test_utils::Equals(expected_decrypted_onc.get(),
                                 actual_decrypted_onc.get()));
}

namespace {

const char* kLoginId = "hans";
const char* kLoginEmail = "hans@my.domain.com";

class StringSubstitutionStub : public StringSubstitution {
 public:
  StringSubstitutionStub() {}
  virtual bool GetSubstitute(std::string placeholder,
                             std::string* substitute) const {
    if (placeholder == substitutes::kLoginIDField)
      *substitute = kLoginId;
    else if (placeholder == substitutes::kEmailField)
      *substitute = kLoginEmail;
    else
      return false;
    return true;
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(StringSubstitutionStub);
};

}  // namespace

TEST(ONCStringExpansion, OpenVPN) {
  scoped_ptr<base::DictionaryValue> vpn_onc =
      test_utils::ReadTestDictionary("valid_openvpn.onc");

  StringSubstitutionStub substitution;
  ExpandStringsInOncObject(kNetworkConfigurationSignature, substitution,
                           vpn_onc.get());

  std::string actual_expanded;
  vpn_onc->GetString("VPN.OpenVPN.Username", &actual_expanded);
  EXPECT_EQ(actual_expanded, std::string("abc ") + kLoginEmail + " def");
}

TEST(ONCStringExpansion, WiFi_EAP) {
  scoped_ptr<base::DictionaryValue> wifi_onc =
      test_utils::ReadTestDictionary("valid_wifi_clientcert.onc");

  StringSubstitutionStub substitution;
  ExpandStringsInOncObject(kNetworkConfigurationSignature, substitution,
                           wifi_onc.get());

  std::string actual_expanded;
  wifi_onc->GetString("WiFi.EAP.Identity", &actual_expanded);
  EXPECT_EQ(actual_expanded, std::string("abc ") + kLoginId + "@my.domain.com");
}

}  // namespace onc
}  // namespace chromeos
