// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NTP_SNIPPETS_NTP_SNIPPETS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_NTP_SNIPPETS_NTP_SNIPPETS_SERVICE_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace ntp_snippets {
class NTPSnippetsService;
}  // namespace ntp_snippets

// A factory to create NTPSnippetsService and associate them to its browser
// context.
class NTPSnippetsServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static NTPSnippetsServiceFactory* GetInstance();
  static ntp_snippets::NTPSnippetsService* GetForProfile(Profile* profile);

 private:
  friend struct base::DefaultSingletonTraits<NTPSnippetsServiceFactory>;

  NTPSnippetsServiceFactory();
  ~NTPSnippetsServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(NTPSnippetsServiceFactory);
};

#endif  // CHROME_BROWSER_NTP_SNIPPETS_NTP_SNIPPETS_SERVICE_FACTORY_H_
