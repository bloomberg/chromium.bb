// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/onc/onc_utils.h"

#include <string>

#include "base/values.h"
#include "chromeos/network/onc/onc_signature.h"
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
  virtual bool GetSubstitute(const std::string& placeholder,
                             std::string* substitute) const OVERRIDE {
    if (placeholder == ::onc::substitutes::kLoginIDField)
      *substitute = kLoginId;
    else if (placeholder ==::onc::substitutes::kEmailField)
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
      test_utils::ReadTestDictionary("wifi_clientcert_with_cert_pems.onc");

  StringSubstitutionStub substitution;
  ExpandStringsInOncObject(kNetworkConfigurationSignature, substitution,
                           wifi_onc.get());

  std::string actual_expanded;
  wifi_onc->GetString("WiFi.EAP.Identity", &actual_expanded);
  EXPECT_EQ(actual_expanded, std::string("abc ") + kLoginId + "@my.domain.com");
}

TEST(ONCResolveServerCertRefs, ResolveServerCertRefs) {
  scoped_ptr<base::DictionaryValue> test_cases =
      test_utils::ReadTestDictionary(
          "network_configs_with_resolved_certs.json");

  CertPEMsByGUIDMap certs;
  certs["cert_google"] = "pem_google";
  certs["cert_webkit"] = "pem_webkit";

  for (base::DictionaryValue::Iterator it(*test_cases);
       !it.IsAtEnd(); it.Advance()) {
    SCOPED_TRACE("Test case: " + it.key());

    const base::DictionaryValue* test_case = NULL;
    it.value().GetAsDictionary(&test_case);

    const base::ListValue* networks_with_cert_refs = NULL;
    test_case->GetList("WithCertRefs", &networks_with_cert_refs);

    const base::ListValue* expected_resolved_onc = NULL;
    test_case->GetList("WithResolvedRefs", &expected_resolved_onc);

    bool expected_success = (networks_with_cert_refs->GetSize() ==
                             expected_resolved_onc->GetSize());

    scoped_ptr<base::ListValue> actual_resolved_onc(
        networks_with_cert_refs->DeepCopy());

    bool success = ResolveServerCertRefsInNetworks(certs,
                                                   actual_resolved_onc.get());
    EXPECT_EQ(expected_success, success);
    EXPECT_TRUE(test_utils::Equals(expected_resolved_onc,
                                   actual_resolved_onc.get()));
  }
}

}  // namespace onc
}  // namespace chromeos
