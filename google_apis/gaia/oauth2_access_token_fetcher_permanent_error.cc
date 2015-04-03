// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/oauth2_access_token_fetcher_permanent_error.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "google_apis/gaia/google_service_auth_error.h"


OAuth2AccessTokenFetcherPermanentError::FailCaller::FailCaller(
    OAuth2AccessTokenFetcherPermanentError* fetcher)
    : fetcher_(fetcher) {
  base::MessageLoop* looper = base::MessageLoop::current();
  DCHECK(looper);
  looper->PostTask(
      FROM_HERE,
      base::Bind(&OAuth2AccessTokenFetcherPermanentError::FailCaller::run,
                 this));
}

OAuth2AccessTokenFetcherPermanentError::FailCaller::~FailCaller() {
}

void OAuth2AccessTokenFetcherPermanentError::FailCaller::run() {
  if (fetcher_) {
    fetcher_->Fail();
    fetcher_ = NULL;
  }
}

void OAuth2AccessTokenFetcherPermanentError::FailCaller::detach() {
  fetcher_ = NULL;
}


OAuth2AccessTokenFetcherPermanentError::OAuth2AccessTokenFetcherPermanentError(
    OAuth2AccessTokenConsumer* consumer,
    const GoogleServiceAuthError& error)
    : OAuth2AccessTokenFetcher(consumer),
      permanent_error_(error) {
  DCHECK(!permanent_error_.IsTransientError());
}

OAuth2AccessTokenFetcherPermanentError::
    ~OAuth2AccessTokenFetcherPermanentError() {
  CancelRequest();
}

void OAuth2AccessTokenFetcherPermanentError::CancelRequest() {
  if (failer_) {
    failer_->detach();
    failer_ = NULL;
  }
}

void OAuth2AccessTokenFetcherPermanentError::Start(
    const std::string& client_id,
    const std::string& client_secret,
    const std::vector<std::string>& scopes) {
  failer_ = new FailCaller(this);
}

void OAuth2AccessTokenFetcherPermanentError::Fail() {
  // The call below will likely destruct this object.  We have to make a copy
  // of the error into a local variable because the class member thus will
  // be destroyed after which the copy-passed-by-reference will cause a
  // memory violation when accessed.
  GoogleServiceAuthError error_copy = permanent_error_;
  FireOnGetTokenFailure(error_copy);
}
