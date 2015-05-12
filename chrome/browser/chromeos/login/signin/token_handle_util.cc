// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/signin/token_handle_util.h"

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/user_manager/user_id.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/gaia/gaia_oauth_client.h"

namespace {

const char kTokenHandlePref[] = "PasswordTokenHandle";
const char kTokenHandleStatusPref[] = "TokenHandleStatus";

const char kHandleStatusValid[] = "valid";
const char kHandleStatusInvalid[] = "invalid";
const char* kDefaultHandleStatus = kHandleStatusValid;

static const int kMaxRetries = 3;

}  // namespace

TokenHandleUtil::TokenHandleUtil(user_manager::UserManager* user_manager)
    : user_manager_(user_manager), weak_factory_(this) {
}

TokenHandleUtil::~TokenHandleUtil() {
  weak_factory_.InvalidateWeakPtrs();
  gaia_client_.reset();
}

bool TokenHandleUtil::HasToken(const user_manager::UserID& user_id) {
  const base::DictionaryValue* dict = nullptr;
  std::string token;
  if (!user_manager_->FindKnownUserPrefs(user_id, &dict))
    return false;
  if (!dict->GetString(kTokenHandlePref, &token))
    return false;
  return !token.empty();
}

bool TokenHandleUtil::ShouldObtainHandle(const user_manager::UserID& user_id) {
  const base::DictionaryValue* dict = nullptr;
  std::string token;
  if (!user_manager_->FindKnownUserPrefs(user_id, &dict))
    return true;
  if (!dict->GetString(kTokenHandlePref, &token))
    return true;
  if (token.empty())
    return true;
  std::string status(kDefaultHandleStatus);
  dict->GetString(kTokenHandleStatusPref, &status);
  return kHandleStatusInvalid == status;
}

void TokenHandleUtil::DeleteHandle(const user_manager::UserID& user_id) {
  const base::DictionaryValue* dict = nullptr;
  if (!user_manager_->FindKnownUserPrefs(user_id, &dict))
    return;
  scoped_ptr<base::DictionaryValue> dict_copy(dict->DeepCopy());
  dict_copy->Remove(kTokenHandlePref, nullptr);
  dict_copy->Remove(kTokenHandleStatusPref, nullptr);
  user_manager_->UpdateKnownUserPrefs(user_id, *dict_copy.get(),
                                      /* replace values */ true);
}

void TokenHandleUtil::MarkHandleInvalid(const user_manager::UserID& user_id) {
  user_manager_->SetKnownUserStringPref(user_id, kTokenHandleStatusPref,
                                        kHandleStatusInvalid);
}

void TokenHandleUtil::CheckToken(const user_manager::UserID& user_id,
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
      token, scoped_ptr<TokenDelegate>(new TokenDelegate(
                 weak_factory_.GetWeakPtr(), false /* validate */, user_id,
                 token, callback)));
  gaia_client_->GetTokenHandleInfo(token, kMaxRetries,
                                   validation_delegates_.get(token));
}

void TokenHandleUtil::StoreTokenHandle(const user_manager::UserID& user_id,
                                       const std::string& handle) {
  user_manager_->SetKnownUserStringPref(user_id, kTokenHandlePref, handle);
  user_manager_->SetKnownUserStringPref(user_id, kTokenHandleStatusPref,
                                        kHandleStatusValid);
}

void TokenHandleUtil::OnValidationComplete(const std::string& token) {
  validation_delegates_.erase(token);
}

void TokenHandleUtil::OnObtainTokenComplete(
    const user_manager::UserID& user_id) {
  obtain_delegates_.erase(user_id);
}

void TokenHandleUtil::GetTokenHandle(const user_manager::UserID& user_id,
                                     const std::string& access_token,
                                     const TokenValidationCallback& callback) {
  if (!gaia_client_.get()) {
    auto request_context =
        chromeos::ProfileHelper::Get()->GetSigninProfile()->GetRequestContext();
    gaia_client_.reset(new gaia::GaiaOAuthClient(request_context));
  }

  obtain_delegates_.set(
      user_id, scoped_ptr<TokenDelegate>(new TokenDelegate(
                   weak_factory_.GetWeakPtr(), true /* obtain */, user_id,
                   std::string(), callback)));
  gaia_client_->GetTokenInfo(access_token, kMaxRetries,
                             obtain_delegates_.get(user_id));
}

TokenHandleUtil::TokenDelegate::TokenDelegate(
    const base::WeakPtr<TokenHandleUtil>& owner,
    bool obtain,
    const user_manager::UserID& user_id,
    const std::string& token,
    const TokenValidationCallback& callback)
    : owner_(owner),
      obtain_(obtain),
      user_id_(user_id),
      token_(token),
      callback_(callback) {
}

TokenHandleUtil::TokenDelegate::~TokenDelegate() {
}

void TokenHandleUtil::TokenDelegate::OnOAuthError() {
  callback_.Run(user_id_, INVALID);
  NotifyDone();
}

void TokenHandleUtil::TokenDelegate::NotifyDone() {
  if (owner_) {
    if (obtain_)
      owner_->OnObtainTokenComplete(user_id_);
    else
      owner_->OnValidationComplete(token_);
  }
}

void TokenHandleUtil::TokenDelegate::OnNetworkError(int response_code) {
  callback_.Run(user_id_, UNKNOWN);
  NotifyDone();
}

void TokenHandleUtil::TokenDelegate::OnGetTokenInfoResponse(
    scoped_ptr<base::DictionaryValue> token_info) {
  TokenHandleStatus outcome = UNKNOWN;
  if (obtain_) {
    if (!token_info->HasKey("error")) {
      std::string handle;
      if (token_info->GetString("token_handle", &handle)) {
        outcome = VALID;
        if (owner_)
          owner_->StoreTokenHandle(user_id_, handle);
      }
    }
  } else {
    if (!token_info->HasKey("error")) {
      int expires_in = 0;
      if (token_info->GetInteger("expires_in", &expires_in))
        outcome = (expires_in < 0) ? INVALID : VALID;
    }
  }
  callback_.Run(user_id_, outcome);
  NotifyDone();
}
