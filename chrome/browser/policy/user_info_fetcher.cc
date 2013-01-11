// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/user_info_fetcher.h"

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "googleurl/src/gurl.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"

namespace {

static const char kAuthorizationHeaderFormat[] =
    "Authorization: Bearer %s";

static std::string MakeAuthorizationHeader(const std::string& auth_token) {
  return StringPrintf(kAuthorizationHeaderFormat, auth_token.c_str());
}

}  // namespace

namespace policy {

UserInfoFetcher::UserInfoFetcher(Delegate* delegate,
                                 net::URLRequestContextGetter* context)
    : delegate_(delegate),
      context_(context) {
  DCHECK(delegate);
}

UserInfoFetcher::~UserInfoFetcher() {
}

void UserInfoFetcher::Start(const std::string& access_token) {
  // Create a URLFetcher and start it.
  url_fetcher_.reset(net::URLFetcher::Create(
      0, GURL(GaiaUrls::GetInstance()->oauth_user_info_url()),
      net::URLFetcher::GET, this));
  url_fetcher_->SetRequestContext(context_);
  url_fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                             net::LOAD_DO_NOT_SAVE_COOKIES);
  url_fetcher_->AddExtraRequestHeader(MakeAuthorizationHeader(access_token));
  url_fetcher_->Start();  // Results in a call to OnURLFetchComplete().
}

void UserInfoFetcher::OnURLFetchComplete(const net::URLFetcher* source) {
  net::URLRequestStatus status = source->GetStatus();
  GoogleServiceAuthError error = GoogleServiceAuthError::None();
  if (!status.is_success()) {
    if (status.status() == net::URLRequestStatus::CANCELED)
      error = GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED);
    else
      error = GoogleServiceAuthError::FromConnectionError(status.error());
  } else if (source->GetResponseCode() != net::HTTP_OK) {
    DLOG(WARNING) << "UserInfo request failed with HTTP code: "
                  << source->GetResponseCode();
    error = GoogleServiceAuthError(
        GoogleServiceAuthError::CONNECTION_FAILED);
  }
  if (error.state() != GoogleServiceAuthError::NONE) {
    delegate_->OnGetUserInfoFailure(error);
    return;
  }

  // Successfully fetched userinfo from the server - parse it and hand it off
  // to the delegate.
  std::string unparsed_data;
  source->GetResponseAsString(&unparsed_data);
  DVLOG(1) << "Received UserInfo response: " << unparsed_data;
  scoped_ptr<base::Value> parsed_value(base::JSONReader::Read(unparsed_data));
  base::DictionaryValue* dict;
  if (parsed_value.get() && parsed_value->GetAsDictionary(&dict)) {
    delegate_->OnGetUserInfoSuccess(dict);
  } else {
    NOTREACHED() << "Could not parse userinfo response from server";
    delegate_->OnGetUserInfoFailure(GoogleServiceAuthError(
        GoogleServiceAuthError::CONNECTION_FAILED));
  }
}

};  // namespace policy
