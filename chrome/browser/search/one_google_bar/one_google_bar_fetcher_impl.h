// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ONE_GOOGLE_BAR_ONE_GOOGLE_BAR_FETCHER_IMPL_H_
#define CHROME_BROWSER_SEARCH_ONE_GOOGLE_BAR_ONE_GOOGLE_BAR_FETCHER_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/search/one_google_bar/one_google_bar_fetcher.h"

class GoogleURLTracker;
class OAuth2TokenService;
class SigninManagerBase;

namespace base {
class Value;
}

namespace net {
class URLFetcher;
class URLRequestContextGetter;
}  // namespace net

struct OneGoogleBarData;

class OneGoogleBarFetcherImpl : public OneGoogleBarFetcher {
 public:
  OneGoogleBarFetcherImpl(SigninManagerBase* signin_manager,
                          OAuth2TokenService* token_service,
                          net::URLRequestContextGetter* request_context,
                          GoogleURLTracker* google_url_tracker);
  ~OneGoogleBarFetcherImpl();

  void Fetch(OneGoogleCallback callback) override;

 private:
  class AuthenticatedURLFetcher;

  void IssueRequestIfNoneOngoing();

  void FetchDone(const net::URLFetcher* source);

  void JsonParsed(std::unique_ptr<base::Value> value);
  void JsonParseFailed(const std::string& message);

  void Respond(const base::Optional<OneGoogleBarData>& data);

  SigninManagerBase* signin_manager_;
  OAuth2TokenService* token_service_;
  net::URLRequestContextGetter* request_context_;
  GoogleURLTracker* google_url_tracker_;

  std::vector<OneGoogleCallback> callbacks_;
  std::unique_ptr<AuthenticatedURLFetcher> pending_request_;

  base::WeakPtrFactory<OneGoogleBarFetcherImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OneGoogleBarFetcherImpl);
};

#endif  // CHROME_BROWSER_SEARCH_ONE_GOOGLE_BAR_ONE_GOOGLE_BAR_FETCHER_IMPL_H_
