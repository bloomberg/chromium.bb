// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/credential_provider_signin_dialog_win_test_data.h"

#include <string>

CredentialProviderSigninDialogTestDataStorage::
    CredentialProviderSigninDialogTestDataStorage()
    : expected_success_signin_result_(base::Value::Type::DICTIONARY),
      expected_success_fetch_result_(base::Value::Type::DICTIONARY) {
  expected_success_signin_result_.SetKey("id", base::Value("gaia_user_id"));
  expected_success_signin_result_.SetKey("password", base::Value("password"));
  expected_success_signin_result_.SetKey("email", base::Value("foo@xyz.com"));
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
