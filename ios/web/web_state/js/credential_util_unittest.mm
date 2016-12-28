// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/web_state/js/credential_util.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "ios/web/public/web_state/credential.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace web {
namespace {

// "type" value for a DictionaryValue representation of PasswordCredential.
const char* kTestCredentialTypePassword = "PasswordCredential";

// "type" value for a DictionaryValue representation of FederatedCredential.
const char* kTestCredentialTypeFederated = "FederatedCredential";

// "id" value for a DictionaryValue representation of a credential.
const char* kTestCredentialID = "foo";

// "name" value for a DictionaryValue representation of a credential.
const char* kTestCredentialName = "Foo Bar";

// "avatarURL" value for a DictionaryValue representation of a credential.
const char* kTestCredentialAvatarURL = "https://foo.com/bar.jpg";

// "password" value for a DictionaryValue representation of a credential.
const char* kTestCredentialPassword = "baz";

// "federationURL" value for a DictionaryValue representation of a credential.
const char* kTestCredentialFederationURL = "https://foo.com/";

// Returns a Credential with Password type.
Credential GetTestPasswordCredential() {
  Credential credential;
  credential.type = CredentialType::CREDENTIAL_TYPE_PASSWORD;
  credential.id = base::ASCIIToUTF16(kTestCredentialID);
  credential.name = base::ASCIIToUTF16(kTestCredentialName);
  credential.avatar_url = GURL(kTestCredentialAvatarURL);
  credential.password = base::ASCIIToUTF16(kTestCredentialPassword);
  return credential;
}

// Returns a credential with Federated type.
Credential GetTestFederatedCredential() {
  Credential credential;
  credential.type = CredentialType::CREDENTIAL_TYPE_FEDERATED;
  credential.id = base::ASCIIToUTF16(kTestCredentialID);
  credential.name = base::ASCIIToUTF16(kTestCredentialName);
  credential.avatar_url = GURL(kTestCredentialAvatarURL);
  credential.federation_origin =
      url::Origin(GURL(kTestCredentialFederationURL));
  return credential;
}

// Returns a value representing the credential returned by
// |GetTestPasswordCredential()|.
std::unique_ptr<base::DictionaryValue>
GetTestPasswordCredentialDictionaryValue() {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue);
  value->SetString("type", kTestCredentialTypePassword);
  value->SetString("id", kTestCredentialID);
  value->SetString("name", kTestCredentialName);
  value->SetString("avatarURL", kTestCredentialAvatarURL);
  value->SetString("password", kTestCredentialPassword);
  return value;
}

// Returns a value representing the credential returned by
// |GetTestFederatedCredentialDictionaryValue()|.
std::unique_ptr<base::DictionaryValue>
GetTestFederatedCredentialDictionaryValue() {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue);
  value->SetString("type", kTestCredentialTypeFederated);
  value->SetString("id", kTestCredentialID);
  value->SetString("name", kTestCredentialName);
  value->SetString("avatarURL", kTestCredentialAvatarURL);
  value->SetString("federation",
                   url::Origin(GURL(kTestCredentialFederationURL)).Serialize());
  return value;
}

// Tests that parsing an empty value fails.
TEST(CredentialUtilTest, ParsingEmptyValueFails) {
  base::DictionaryValue value;
  Credential credential;
  EXPECT_FALSE(DictionaryValueToCredential(value, &credential));
}

// Tests that parsing a value with a bad type fails.
TEST(CredentialUtilTest, ParsingValueWithBadTypeFails) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue);
  value->SetString("type", "FooCredential");
  Credential credential;
  EXPECT_FALSE(DictionaryValueToCredential(*value, &credential));
}

// Tests that parsing a correctly-formed value representing a PasswordCredential
// succeeds.
TEST(CredentialUtilTest, ParsingPasswordCredentialSucceeds) {
  Credential credential;
  EXPECT_TRUE(DictionaryValueToCredential(
      *GetTestPasswordCredentialDictionaryValue(), &credential));
  EXPECT_TRUE(CredentialsEqual(GetTestPasswordCredential(), credential));
}

// Tests that parsing a value representing a PasswordCredential but with no ID
// specified fails.
TEST(CredentialUtilTest, ParsingPasswordCredentialWithNoIDFails) {
  std::unique_ptr<base::DictionaryValue> value(
      GetTestPasswordCredentialDictionaryValue());
  value->RemoveWithoutPathExpansion("id", nullptr);
  Credential credential;
  EXPECT_FALSE(DictionaryValueToCredential(*value, &credential));
}

// Tests that parsing a value representing a PasswordCredential with a badly-
// formed avatarURL fails.
TEST(CredentialUtilTest, ParsingPasswordCredentialWithBadAvatarURLFails) {
  std::unique_ptr<base::DictionaryValue> value(
      GetTestPasswordCredentialDictionaryValue());
  value->SetString("avatarURL", "foo");
  Credential credential;
  EXPECT_FALSE(DictionaryValueToCredential(*value, &credential));
}

// Tests that parsing a value representing a PasswordCredential with no password
// specified fails.
TEST(CredentialUtilTest, ParsingPasswordCredentialWithNoPasswordFails) {
  std::unique_ptr<base::DictionaryValue> value(
      GetTestPasswordCredentialDictionaryValue());
  value->Remove("password", nullptr);
  Credential credential;
  EXPECT_FALSE(DictionaryValueToCredential(*value, &credential));
}

// Tests that parsing a correctly-formed value representing a
// FederatedCredential succeeds.
TEST(CredentialUtilTest, ParsingFederatedCredentialSucceeds) {
  Credential credential;
  EXPECT_TRUE(DictionaryValueToCredential(
      *GetTestFederatedCredentialDictionaryValue(), &credential));
  EXPECT_TRUE(CredentialsEqual(GetTestFederatedCredential(), credential));
}

// Tests that parsing a value representing a FederatedCredential with no ID
// fails.
TEST(CredentialUtilTest, ParsingFederatedCredentialWithNoIDFails) {
  std::unique_ptr<base::DictionaryValue> value(
      GetTestFederatedCredentialDictionaryValue());
  value->RemoveWithoutPathExpansion("id", nullptr);
  Credential credential;
  EXPECT_FALSE(DictionaryValueToCredential(*value, &credential));
}

// Tests that parsing a value representing a FederatedCredential with a badly-
// formed avatarURL fails.
TEST(CredentialUtilTest, ParsingFederatedCredentialWithBadAvatarURLFails) {
  std::unique_ptr<base::DictionaryValue> value(
      GetTestFederatedCredentialDictionaryValue());
  value->SetString("avatarURL", "foo");
  Credential credential;
  EXPECT_FALSE(DictionaryValueToCredential(*value, &credential));
}

// Tests that parsing a value representing a FederatedCredential with no
// federation URL fails.
TEST(CredentialUtilTest, ParsingFederatedValueWithNoFederationURLFails) {
  std::unique_ptr<base::DictionaryValue> value(
      GetTestFederatedCredentialDictionaryValue());
  value->Remove("federation", nullptr);
  Credential credential;
  EXPECT_FALSE(DictionaryValueToCredential(*value, &credential));
}

// Tests that parsing a value representing a FederatedCredential with a badly-
// formed federationURL fails.
TEST(CredentialUtilTest, ParsingFederatedValueWithBadFederationURLFails) {
  std::unique_ptr<base::DictionaryValue> value(
      GetTestFederatedCredentialDictionaryValue());
  value->SetString("federation", "bar");
  Credential credential;
  EXPECT_FALSE(DictionaryValueToCredential(*value, &credential));
}

// Tests that serializing a FederatedCredential to a DictionaryValue results
// in the expected structure.
TEST(CredentialUtilTest, SerializeFederatedCredential) {
  base::DictionaryValue value;
  Credential credential(GetTestFederatedCredential());
  CredentialToDictionaryValue(credential, &value);
  EXPECT_TRUE(GetTestFederatedCredentialDictionaryValue()->Equals(&value));
}

// Tests that serializing a PasswordCredential to a DictionaryValue results in
// the
// expected structure.
TEST(CredentialUtilTest, SerializePasswordCredential) {
  base::DictionaryValue value;
  Credential credential(GetTestPasswordCredential());
  CredentialToDictionaryValue(credential, &value);
  EXPECT_TRUE(GetTestPasswordCredentialDictionaryValue()->Equals(&value));
}

TEST(CredentialUtilTest, SerializeEmptyCredential) {
  base::DictionaryValue value;
  Credential credential;
  CredentialToDictionaryValue(credential, &value);
  EXPECT_TRUE(base::WrapUnique(new base::DictionaryValue)->Equals(&value));
}

TEST(CredentialUtilTest, SerializeEmptyCredentialIntoNonEmptyDictionary) {
  base::DictionaryValue value;
  value.SetString("foo", "bar");
  Credential credential;
  CredentialToDictionaryValue(credential, &value);
  EXPECT_TRUE(base::WrapUnique(new base::DictionaryValue)->Equals(&value));
}

}  // namespace
}  // namespace web
