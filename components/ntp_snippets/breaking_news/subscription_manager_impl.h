// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_BREAKING_NEWS_SUBSCRIPTION_MANAGER_IMPL_H_
#define COMPONENTS_NTP_SNIPPETS_BREAKING_NEWS_SUBSCRIPTION_MANAGER_IMPL_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "components/ntp_snippets/breaking_news/subscription_json_request.h"
#include "components/ntp_snippets/breaking_news/subscription_manager.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

class AccessTokenFetcher;
class OAuth2TokenService;
class PrefRegistrySimple;
class PrefService;

namespace variations {
class VariationsService;
}

namespace ntp_snippets {

// Class that wraps around the functionality of SubscriptionJsonRequest. It uses
// the SubscriptionJsonRequest to send subscription and unsubscription requests
// to the content suggestions server and does the bookkeeping for the data used
// for subscription. Bookkeeping is required to detect any change (e.g. the
// token render invalid), and resubscribe accordingly.
class SubscriptionManagerImpl : public SubscriptionManager {
 public:
  SubscriptionManagerImpl(
      scoped_refptr<net::URLRequestContextGetter> url_request_context_getter,
      PrefService* pref_service,
      variations::VariationsService* variations_service,
      SigninManagerBase* signin_manager,
      OAuth2TokenService* access_token_service,
      const std::string& locale,
      const std::string& api_key,
      const GURL& subscribe_url,
      const GURL& unsubscribe_url);

  ~SubscriptionManagerImpl() override;

  // SubscriptionManager implementation.
  void Subscribe(const std::string& token) override;
  void Unsubscribe() override;
  bool IsSubscribed() override;

  void Resubscribe(const std::string& new_token) override;

  // Checks if some data that has been used when subscribing has changed. For
  // example, the user has signed in.
  bool NeedsToResubscribe() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

 private:
  class SigninObserver;

  void SigninStatusChanged();

  void DidSubscribe(const std::string& subscription_token,
                    bool is_authenticated,
                    const Status& status);
  void DidUnsubscribe(const std::string& new_token, const Status& status);
  void SubscribeInternal(const std::string& subscription_token,
                         const std::string& access_token);

  // If |new_token| is empty, this will just unsubscribe. If |new_token| is
  // non-empty, a subscription request with the |new_token| will be started upon
  // successful unsubscription.
  void ResubscribeInternal(const std::string& old_token,
                           const std::string& new_token);

  // |subscription_token| is the token when subscribing after obtaining the
  // access token.
  void StartAccessTokenRequest(const std::string& subscription_token);
  void AccessTokenFetchFinished(const std::string& subscription_token,
                                const GoogleServiceAuthError& error,
                                const std::string& access_token);

  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;

  std::unique_ptr<internal::SubscriptionJsonRequest> request_;
  std::unique_ptr<AccessTokenFetcher> access_token_fetcher_;

  PrefService* pref_service_;

  variations::VariationsService* const variations_service_;

  // Authentication for signed-in users.
  SigninManagerBase* signin_manager_;
  std::unique_ptr<SigninObserver> signin_observer_;
  OAuth2TokenService* access_token_service_;

  const std::string locale_;

  // API key to use for non-authenticated requests.
  const std::string api_key_;

  // API endpoint for subscribing and unsubscribing.
  const GURL subscribe_url_;
  const GURL unsubscribe_url_;

  DISALLOW_COPY_AND_ASSIGN(SubscriptionManagerImpl);
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_BREAKING_NEWS_SUBSCRIPTION_MANAGER_IMPL_H_
