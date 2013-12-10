// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/mutable_profile_oauth2_token_service.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/webdata/token_web_data.h"
#include "components/webdata/common/web_data_service_base.h"
#include "google_apis/gaia/gaia_constants.h"

#if defined(ENABLE_MANAGED_USERS)
#include "chrome/browser/managed_mode/managed_user_constants.h"
#endif

namespace {

const char kAccountIdPrefix[] = "AccountId-";
const size_t kAccountIdPrefixLength = 10;

bool IsLegacyRefreshTokenId(const std::string& service_id) {
  return service_id == GaiaConstants::kGaiaOAuth2LoginRefreshToken;
}

bool IsLegacyServiceId(const std::string& account_id) {
  return account_id.compare(0u, kAccountIdPrefixLength, kAccountIdPrefix) != 0;
}

std::string RemoveAccountIdPrefix(const std::string& prefixed_account_id) {
  return prefixed_account_id.substr(kAccountIdPrefixLength);
}

}  // namespace

MutableProfileOAuth2TokenService::MutableProfileOAuth2TokenService()
    : web_data_service_request_(0)  {
}

MutableProfileOAuth2TokenService::~MutableProfileOAuth2TokenService() {
}

void MutableProfileOAuth2TokenService::Shutdown() {
  if (web_data_service_request_ != 0) {
    scoped_refptr<TokenWebData> token_web_data =
        TokenWebData::FromBrowserContext(profile());
    DCHECK(token_web_data.get());
    token_web_data->CancelRequest(web_data_service_request_);
    web_data_service_request_  = 0;
  }
  ProfileOAuth2TokenService::Shutdown();
}

void MutableProfileOAuth2TokenService::LoadCredentials() {
  DCHECK_EQ(0, web_data_service_request_);

  CancelAllRequests();
  refresh_tokens().clear();
  scoped_refptr<TokenWebData> token_web_data =
      TokenWebData::FromBrowserContext(profile());
  if (token_web_data.get())
    web_data_service_request_ = token_web_data->GetAllTokens(this);
}

void MutableProfileOAuth2TokenService::OnWebDataServiceRequestDone(
    WebDataServiceBase::Handle handle,
    const WDTypedResult* result) {
  DCHECK_EQ(web_data_service_request_, handle);
  web_data_service_request_ = 0;

  if (result) {
    DCHECK(result->GetType() == TOKEN_RESULT);
    const WDResult<std::map<std::string, std::string> > * token_result =
        static_cast<const WDResult<std::map<std::string, std::string> > * > (
            result);
    LoadAllCredentialsIntoMemory(token_result->GetValue());
  }

  std::string account_id = GetPrimaryAccountId();

  // If |account_id| is not empty, make sure that we have an entry in the
  // map for it.  The entry could be missing if there is a corruption in
  // the token DB while this profile is connected to an account.
  if (!account_id.empty() && refresh_tokens().count(account_id) == 0) {
    refresh_tokens()[account_id].reset(
        new AccountInfo(this, account_id, std::string()));
  }

  // If we don't have a refresh token for the primary account, signal a signin
  // error.
  if (!account_id.empty() && !RefreshTokenIsAvailable(account_id)) {
    UpdateAuthError(
        account_id,
        GoogleServiceAuthError(
            GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  }
}

void MutableProfileOAuth2TokenService::LoadAllCredentialsIntoMemory(
    const std::map<std::string, std::string>& db_tokens) {
  std::string old_login_token;

  for (std::map<std::string, std::string>::const_iterator iter =
           db_tokens.begin();
       iter != db_tokens.end();
       ++iter) {
    std::string prefixed_account_id = iter->first;
    std::string refresh_token = iter->second;

    if (IsLegacyRefreshTokenId(prefixed_account_id) && !refresh_token.empty())
      old_login_token = refresh_token;

    if (IsLegacyServiceId(prefixed_account_id)) {
      scoped_refptr<TokenWebData> token_web_data =
          TokenWebData::FromBrowserContext(profile());
      if (token_web_data.get())
        token_web_data->RemoveTokenForService(prefixed_account_id);
    } else {
      DCHECK(!refresh_token.empty());
      std::string account_id = RemoveAccountIdPrefix(prefixed_account_id);
      refresh_tokens()[account_id].reset(
          new AccountInfo(this, account_id, refresh_token));
      FireRefreshTokenAvailable(account_id);
      // TODO(fgorski): Notify diagnostic observers.
    }
  }

  if (!old_login_token.empty()) {
    std::string account_id = GetAccountIdForMigratingRefreshToken();

    if (refresh_tokens().count(account_id) == 0)
      UpdateCredentials(account_id, old_login_token);
  }

  FireRefreshTokensLoaded();
}

std::string
MutableProfileOAuth2TokenService::GetAccountIdForMigratingRefreshToken() {
#if defined(ENABLE_MANAGED_USERS)
  // TODO(bauerb): Make sure that only services that can deal with supervised
  // users see the supervised user token.
  if (profile()->IsManaged())
    return managed_users::kManagedUserPseudoEmail;
#endif

  return GetPrimaryAccountId();
}
