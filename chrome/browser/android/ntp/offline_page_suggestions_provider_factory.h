// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_NTP_OFFLINE_PAGE_SUGGESTIONS_PROVIDER_FACTORY_H_
#define CHROME_BROWSER_ANDROID_NTP_OFFLINE_PAGE_SUGGESTIONS_PROVIDER_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace ntp_snippets {
class ContentSuggestionsService;
class OfflinePageSuggestionsProvider;
}  // namespace ntp_snippets

class OfflinePageSuggestionsProviderFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static OfflinePageSuggestionsProviderFactory* GetInstance();
  static ntp_snippets::OfflinePageSuggestionsProvider* GetForProfile(
      Profile* profile);

 private:
  friend struct base::DefaultSingletonTraits<
      OfflinePageSuggestionsProviderFactory>;

  OfflinePageSuggestionsProviderFactory();
  ~OfflinePageSuggestionsProviderFactory() override;

  // BrowserContextKeyedServiceFactory implementation.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageSuggestionsProviderFactory);
};

#endif  // CHROME_BROWSER_ANDROID_NTP_OFFLINE_PAGE_SUGGESTIONS_PROVIDER_FACTORY_H_
