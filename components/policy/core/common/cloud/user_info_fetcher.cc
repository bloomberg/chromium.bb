// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/user_info_fetcher.h"

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

namespace policy {

UserInfoFetcher::UserInfoFetcher(Delegate* delegate,
                                 net::URLRequestContextGetter* context)
    : delegate_(delegate), gaia_client_(context) {
  DCHECK(delegate);
}

UserInfoFetcher::~UserInfoFetcher() {
}

void UserInfoFetcher::Start(const std::string& access_token) {
  // Create a URLFetcher and start it.
  gaia_client_.GetUserInfo(access_token, 0, &delegate_);
}

UserInfoFetcher::GaiaDelegate::GaiaDelegate(UserInfoFetcher::Delegate* delegate)
    : delegate_(delegate) {
}

void UserInfoFetcher::GaiaDelegate::OnGetUserInfoResponse(
    scoped_ptr<base::DictionaryValue> user_info) {
  delegate_->OnGetUserInfoSuccess(user_info.get());
}

void UserInfoFetcher::GaiaDelegate::OnOAuthError() {
  GoogleServiceAuthError error =
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  delegate_->OnGetUserInfoFailure(error);
}

void UserInfoFetcher::GaiaDelegate::OnNetworkError(int response_code) {
  GoogleServiceAuthError error =
      GoogleServiceAuthError::FromConnectionError(response_code);
  delegate_->OnGetUserInfoFailure(error);
}

};  // namespace policy
