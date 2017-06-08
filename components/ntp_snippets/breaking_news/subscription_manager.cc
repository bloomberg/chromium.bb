// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/breaking_news/subscription_manager.h"
#include "base/bind.h"
#include "components/ntp_snippets/breaking_news/subscription_json_request.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/prefs/pref_service.h"

namespace ntp_snippets {

using internal::SubscriptionJsonRequest;

SubscriptionManager::SubscriptionManager(
    scoped_refptr<net::URLRequestContextGetter> url_request_context_getter,
    PrefService* pref_service,
    const GURL& subscribe_url)
    : url_request_context_getter_(std::move(url_request_context_getter)),
      pref_service_(pref_service),
      subscribe_url_(subscribe_url) {}

SubscriptionManager::~SubscriptionManager() = default;

void SubscriptionManager::Subscribe(const std::string& token) {
  DCHECK(!subscription_request_);
  subscription_token_ = token;
  SubscriptionJsonRequest::Builder builder;
  builder.SetToken(token)
      .SetUrlRequestContextGetter(url_request_context_getter_)
      .SetUrl(subscribe_url_);

  subscription_request_ = builder.Build();
  subscription_request_->Start(base::BindOnce(
      &SubscriptionManager::DidSubscribe, base::Unretained(this)));
}

void SubscriptionManager::DidSubscribe(const ntp_snippets::Status& status) {
  subscription_request_.reset();

  switch (status.code) {
    case ntp_snippets::StatusCode::SUCCESS:
      // In case of successful subscription, store the same data used for
      // subscription in order to be able to re-subscribe in case of data
      // change.
      // TODO(mamir): store region and language.
      pref_service_->SetString(
          ntp_snippets::prefs::kContentSuggestionsSubscriptionDataToken,
          subscription_token_);
      break;
    default:
      // TODO(mamir): handle failure.
      break;
  }
}

void SubscriptionManager::Unsubscribe(const std::string& token) {
  // TODO(mamir): Implement.
}

void SubscriptionManager::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kContentSuggestionsSubscriptionDataToken,
                               std::string());
}
}  // namespace ntp_snippets
