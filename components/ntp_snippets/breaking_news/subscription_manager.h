// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_BREAKING_NEWS_SUBSCRIPTION_MANAGER_H_
#define COMPONENTS_NTP_SNIPPETS_BREAKING_NEWS_SUBSCRIPTION_MANAGER_H_

#include "components/ntp_snippets/breaking_news/subscription_json_request.h"
#include "components/prefs/pref_registry_simple.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

class PrefRegistrySimple;

namespace ntp_snippets {

// Class that wraps around the functionality of SubscriptionJsonRequest. It uses
// the SubscriptionJsonRequest to send subscription and unsubscription requests
// to the content suggestions server and does the bookkeeping for the data used
// for subscription. Bookkeeping is required to detect any change (e.g. the
// token render invalid), and resubscribe accordingly.
class SubscriptionManager {
 public:
  SubscriptionManager(
      scoped_refptr<net::URLRequestContextGetter> url_request_context_getter,
      PrefService* pref_service,
      const GURL& subscribe_url,
      const GURL& unsubscribe_url);

  ~SubscriptionManager();

  void Subscribe(const std::string& token);
  bool CanSubscribeNow();
  void Unsubscribe(const std::string& token);
  bool CanUnsubscribeNow();
  bool IsSubscribed();

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

 private:
  std::string subscription_token_;
  std::string unsubscription_token_;

  // Holds the URL request context.
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;

  std::unique_ptr<internal::SubscriptionJsonRequest> subscription_request_;
  std::unique_ptr<internal::SubscriptionJsonRequest> unsubscription_request_;

  PrefService* pref_service_;

  // API endpoint for subscribing and unsubscribing.
  const GURL subscribe_url_;
  const GURL unsubscribe_url_;

  void DidSubscribe(const ntp_snippets::Status& status);
  void DidUnsubscribe(const ntp_snippets::Status& status);

  DISALLOW_COPY_AND_ASSIGN(SubscriptionManager);
};
}
#endif  // COMPONENTS_NTP_SNIPPETS_BREAKING_NEWS_SUBSCRIPTION_MANAGER_H_
