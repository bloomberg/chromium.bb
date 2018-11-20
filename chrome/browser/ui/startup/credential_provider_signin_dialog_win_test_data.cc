// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/credential_provider_signin_dialog_win_test_data.h"

#include <string>

#include "base/json/json_writer.h"

const char
    CredentialProviderSigninDialogTestDataStorage::kInvalidTokenInfoResponse[] =
        "{"
        "  \"error\": \"invalid_token\""
        "}";
const char
    CredentialProviderSigninDialogTestDataStorage::kInvalidUserInfoResponse[] =
        "{"
        "  \"error\": \"invalid_token\""
        "}";
const char CredentialProviderSigninDialogTestDataStorage::
    kInvalidTokenFetchResponse[] =
        "{"
        "  \"error\": \"invalid_token\""
        "}";

const char
    CredentialProviderSigninDialogTestDataStorage::kInvalidEmailForSignin[] =
        "foo_bar@example.com";
CredentialProviderSigninDialogTestDataStorage::
    CredentialProviderSigninDialogTestDataStorage()
    : expected_success_signin_result_(base::Value::Type::DICTIONARY),
      expected_success_fetch_result_(base::Value::Type::DICTIONARY) {
  SetSigninPassword("password");
}

void CredentialProviderSigninDialogTestDataStorage::SetSigninPassword(
    const std::string& password) {
  expected_success_signin_result_.SetKey("id", base::Value("gaia_user_id"));
  expected_success_signin_result_.SetKey("password", base::Value(password));
  expected_success_signin_result_.SetKey("email",
                                         base::Value("foo_bar@gmail.com"));
  expected_success_signin_result_.SetKey("access_token",
                                         base::Value("access_token"));
  expected_success_signin_result_.SetKey("refresh_token",
                                         base::Value("refresh_token"));
  expected_success_fetch_result_.SetKey("token_handle",
                                        base::Value("token_handle"));
  expected_success_fetch_result_.SetKey("mdm_id_token",
                                        base::Value("mdm_token"));
  expected_success_fetch_result_.SetKey("full_name", base::Value("Foo Bar"));

  expected_success_full_result_ = expected_success_signin_result_.Clone();
  expected_success_full_result_.MergeDictionary(
      &expected_success_fetch_result_);
}

// static
base::Value
CredentialProviderSigninDialogTestDataStorage::MakeSignInResponseValue(
    const std::string& id,
    const std::string& password,
    const std::string& email,
    const std::string& access_token,
    const std::string& refresh_token) {
  base::Value args(base::Value::Type::DICTIONARY);
  if (!email.empty())
    args.SetKey("email", base::Value(email));
  if (!password.empty())
    args.SetKey("password", base::Value(password));
  if (!id.empty())
    args.SetKey("id", base::Value(id));
  if (!refresh_token.empty())
    args.SetKey("refresh_token", base::Value(refresh_token));
  if (!access_token.empty())
    args.SetKey("access_token", base::Value(access_token));

  return args;
}

base::Value
CredentialProviderSigninDialogTestDataStorage::MakeValidSignInResponseValue()
    const {
  return MakeSignInResponseValue(GetSuccessId(), GetSuccessPassword(),
                                 GetSuccessEmail(), GetSuccessAccessToken(),
                                 GetSuccessRefreshToken());
}

std::string
CredentialProviderSigninDialogTestDataStorage::GetSuccessfulSigninResult()
    const {
  std::string json;
  base::JSONWriter::Write(expected_success_signin_result_, &json);
  return json;
}

std::string CredentialProviderSigninDialogTestDataStorage::
    GetSuccessfulSigninTokenFetchResult() const {
  return "{"
         "  \"access_token\": \"" +
         GetSuccessAccessToken() +
         "\","
         "  \"refresh_token\": \"" +
         GetSuccessRefreshToken() +
         "\","
         "  \"id_token\": \"signin_id_token\","
         "  \"expires_in\": 1800"
         "}";
}

std::string CredentialProviderSigninDialogTestDataStorage::
    GetSuccessfulMdmTokenFetchResult() const {
  return "{"
         "  \"access_token\": \"123456789\","
         "  \"refresh_token\": \"mdm_refresh_token\","
         "  \"id_token\": \"" +
         GetSuccessMdmIdToken() +
         "\","
         "  \"expires_in\": 1800"
         "}";
}

std::string CredentialProviderSigninDialogTestDataStorage::
    GetSuccessfulTokenInfoFetchResult() const {
  return "{"
         "  \"audience\": \"blah.apps.googleusercontent.blah.com\","
         "  \"used_id\": \"1234567890\","
         "  \"scope\": \"all the things\","
         "  \"expires_in\": 1800,"
         "  \"token_type\": \"Bearer\","
         "  \"token_handle\": \"" +
         GetSuccessTokenHandle() +
         "\""
         "}";
}

std::string CredentialProviderSigninDialogTestDataStorage::
    GetSuccessfulUserInfoFetchResult() const {
  return "{"
         "  \"name\": \"" +
         GetSuccessFullName() +
         "\""
         "}";
}
