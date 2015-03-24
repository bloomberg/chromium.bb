// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/signin/token_handler_util.h"

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/user_manager/user_id.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/gaia/gaia_oauth_client.h"

namespace {

const char kTokenHandlePref[] = "PasswordTokenHandle";
static const int kMaxRetries = 3;
}

TokenHandlerUtil::TokenHandlerUtil(user_manager::UserManager* user_manager)
    : user_manager_(user_manager), weak_factory_(this) {
}

TokenHandlerUtil::~TokenHandlerUtil() {
  weak_factory_.InvalidateWeakPtrs();
  gaia_client_.reset();
}

bool TokenHandlerUtil::HasToken(const user_manager::UserID& user_id) {
  const base::DictionaryValue* dict = nullptr;
  std::string token;
  if (!user_manager_->FindKnownUserPrefs(user_id, &dict))
    return false;
  if (!dict->GetString(kTokenHandlePref, &token))
    return false;
  return !token.empty();
}

void TokenHandlerUtil::DeleteToken(const user_manager::UserID& user_id) {
  const base::DictionaryValue* dict = nullptr;
  if (!user_manager_->FindKnownUserPrefs(user_id, &dict))
    return;
  scoped_ptr<base::DictionaryValue> dict_copy(dict->DeepCopy());
  if (!dict_copy->Remove(kTokenHandlePref, nullptr))
    return;
  user_manager_->UpdateKnownUserPrefs(user_id, *dict_copy.get(),
                                      /* replace values */ true);
}

void TokenHandlerUtil::CheckToken(const user_manager::UserID& user_id,
                                  const TokenValidationCallback& callback) {
  const base::DictionaryValue* dict = nullptr;
  std::string token;
  if (!user_manager_->FindKnownUserPrefs(user_id, &dict)) {
    callback.Run(user_id, UNKNOWN);
    return;
  }
  if (!dict->GetString(kTokenHandlePref, &token)) {
    callback.Run(user_id, UNKNOWN);
    return;
  }

  if (!gaia_client_.get()) {
    auto request_context =
        chromeos::ProfileHelper::Get()->GetSigninProfile()->GetRequestContext();
    gaia_client_.reset(new gaia::GaiaOAuthClient(request_context));
  }

  validation_delegates_.set(
      token, scoped_ptr<TokenValidationDelegate>(new TokenValidationDelegate(
                 weak_factory_.GetWeakPtr(), user_id, token, callback)));
  gaia_client_->GetTokenHandleInfo(token, kMaxRetries,
                                   validation_delegates_.get(token));
}

void TokenHandlerUtil::OnValidationComplete(const std::string& token) {
  validation_delegates_.erase(token);
}

TokenHandlerUtil::TokenValidationDelegate::TokenValidationDelegate(
    const base::WeakPtr<TokenHandlerUtil>& owner,
    const user_manager::UserID& user_id,
    const std::string& token,
    const TokenValidationCallback& callback)
    : owner_(owner), user_id_(user_id), token_(token), callback_(callback) {
}

TokenHandlerUtil::TokenValidationDelegate::~TokenValidationDelegate() {
}

void TokenHandlerUtil::TokenValidationDelegate::OnOAuthError() {
  callback_.Run(user_id_, INVALID);
  if (owner_)
    owner_->OnValidationComplete(token_);
}

void TokenHandlerUtil::TokenValidationDelegate::OnNetworkError(
    int response_code) {
  callback_.Run(user_id_, UNKNOWN);
  if (owner_)
    owner_->OnValidationComplete(token_);
}

void TokenHandlerUtil::TokenValidationDelegate::OnGetTokenInfoResponse(
    scoped_ptr<base::DictionaryValue> token_info) {
  TokenHandleStatus outcome = UNKNOWN;
  if (!token_info->HasKey("error")) {
    int expires_in = 0;
    if (token_info->GetInteger("expires_in", &expires_in))
      outcome = (expires_in < 0) ? INVALID : VALID;
  }
  callback_.Run(user_id_, outcome);
  if (owner_)
    owner_->OnValidationComplete(token_);
}
