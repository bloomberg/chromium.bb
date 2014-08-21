// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/public/test/fake_profile_oauth2_token_service_ios_provider.h"

#include <Foundation/Foundation.h>

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"

namespace ios {

FakeProfileOAuth2TokenServiceIOSProvider::
    FakeProfileOAuth2TokenServiceIOSProvider() {}

FakeProfileOAuth2TokenServiceIOSProvider::
    ~FakeProfileOAuth2TokenServiceIOSProvider() {}

void FakeProfileOAuth2TokenServiceIOSProvider::GetAccessToken(
    const std::string& account_id,
    const std::string& client_id,
    const std::string& client_secret,
    const std::set<std::string>& scopes,
    const AccessTokenCallback& callback) {
  requests_.push_back(AccessTokenRequest(account_id, callback));
}

std::vector<std::string>
FakeProfileOAuth2TokenServiceIOSProvider::GetAllAccountIds() {
  return accounts_;
}

void FakeProfileOAuth2TokenServiceIOSProvider::AddAccount(
    const std::string& account_id) {
  accounts_.push_back(account_id);
}

void FakeProfileOAuth2TokenServiceIOSProvider::SetAccounts(
    const std::vector<std::string>& accounts) {
  accounts_ = accounts;
}

void FakeProfileOAuth2TokenServiceIOSProvider::ClearAccounts() {
  accounts_.clear();
}

void
FakeProfileOAuth2TokenServiceIOSProvider::IssueAccessTokenForAllRequests() {
  for (auto i = requests_.begin(); i != requests_.end(); ++i) {
    std::string account_id = i->first;
    AccessTokenCallback callback = i->second;
    NSString* access_token = [NSString
        stringWithFormat:@"fake_access_token [account=%s]", account_id.c_str()];
    NSDate* one_hour_from_now = [NSDate dateWithTimeIntervalSinceNow:3600];
    callback.Run(access_token, one_hour_from_now, nil);
  }
  requests_.clear();
}

void FakeProfileOAuth2TokenServiceIOSProvider::
    IssueAccessTokenErrorForAllRequests() {
  for (auto i = requests_.begin(); i != requests_.end(); ++i) {
    std::string account_id = i->first;
    AccessTokenCallback callback = i->second;
    NSError* error = [[[NSError alloc] initWithDomain:@"fake_access_token_error"
                                                 code:-1
                                             userInfo:nil] autorelease];
    callback.Run(nil, nil, error);
  }
  requests_.clear();
}

void
FakeProfileOAuth2TokenServiceIOSProvider::InitializeSharedAuthentication() {}

AuthenticationErrorCategory
FakeProfileOAuth2TokenServiceIOSProvider::GetAuthenticationErrorCategory(
    NSError* error) const {
  DCHECK(error);
  return ios::kAuthenticationErrorCategoryAuthorizationErrors;
}

}  // namespace ios
