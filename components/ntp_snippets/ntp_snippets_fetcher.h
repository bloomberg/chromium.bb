// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_NTP_SNIPPETS_FETCHER_H_
#define COMPONENTS_NTP_SNIPPETS_NTP_SNIPPETS_FETCHER_H_

#include <string>

#include "base/callback.h"
#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequenced_task_runner.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "net/url_request/url_request_context_getter.h"

namespace net {
class URLFetcher;
class URLFetcherDelegate;
}  // namespace net

class SigninManagerBase;

namespace ntp_snippets {

// Fetches snippet data for the NTP from the server
class NTPSnippetsFetcher : public OAuth2TokenService::Consumer,
                           public OAuth2TokenService::Observer,
                           public net::URLFetcherDelegate {
 public:
  using SnippetsAvailableCallback = base::Callback<void(const base::FilePath&)>;
  using SnippetsAvailableCallbackList =
      base::CallbackList<void(const base::FilePath&)>;

  NTPSnippetsFetcher(scoped_refptr<base::SequencedTaskRunner> file_task_runner,
                     SigninManagerBase* signin_manager,
                     OAuth2TokenService* oauth2_token_service,
                     scoped_refptr<net::URLRequestContextGetter>
                        url_request_context_getter,
                     const base::FilePath& base_download_path);
  ~NTPSnippetsFetcher() override;

  // Adds a callback that is called when a new set of snippets are downloaded.
  scoped_ptr<SnippetsAvailableCallbackList::Subscription> AddCallback(
      const SnippetsAvailableCallback& callback) WARN_UNUSED_RESULT;

  // Fetches snippets from the server.
  void FetchSnippets();

 private:
  void StartTokenRequest();
  void OnFileMoveDone(bool success);
  void NotifyObservers();

  // OAuth2TokenService::Consumer overrides:
  void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                         const std::string& access_token,
                         const base::Time& expiration_time) override;
  void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                         const GoogleServiceAuthError& error) override;

  // OAuth2TokenService::Observer overrides:
  void OnRefreshTokenAvailable(const std::string& account_id) override;

  // URLFetcherDelegate implementation.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  // The SequencedTaskRunner on which file system operations will be run.
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  // Holds the URL request context.
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;

  scoped_ptr<net::URLFetcher> url_fetcher_;
  scoped_ptr<SigninManagerBase> signin_manager_;
  scoped_ptr<OAuth2TokenService> token_service_;
  scoped_ptr<OAuth2TokenService::Request> oauth_request_;

  base::FilePath download_path_;
  bool waiting_for_refresh_token_;

  SnippetsAvailableCallbackList callback_list_;

  base::WeakPtrFactory<NTPSnippetsFetcher> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NTPSnippetsFetcher);
};
}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_NTP_SNIPPETS_FETCHER_H_
