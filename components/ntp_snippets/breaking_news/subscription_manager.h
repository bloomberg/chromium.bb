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

class SubscriptionManager {
 public:
  SubscriptionManager(
      scoped_refptr<net::URLRequestContextGetter> url_request_context_getter,
      PrefService* pref_service,
      const GURL& subscribe_url);

  ~SubscriptionManager();

  void Subscribe(const std::string& token);
  void Unsubscribe(const std::string& token);

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

 private:
  std::string subscription_token_;

  // Holds the URL request context.
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;

  std::unique_ptr<internal::SubscriptionJsonRequest> subscription_request_;

  PrefService* pref_service_;

  // API endpoint for subscribing.
  const GURL subscribe_url_;

  void DidSubscribe(const ntp_snippets::Status& status);

  DISALLOW_COPY_AND_ASSIGN(SubscriptionManager);
};
}
#endif  // COMPONENTS_NTP_SNIPPETS_BREAKING_NEWS_SUBSCRIPTION_MANAGER_H_
