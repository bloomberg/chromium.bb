// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_SNIPPETS_IOS_CHROME_NTP_SNIPPETS_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_NTP_SNIPPETS_IOS_CHROME_NTP_SNIPPETS_SERVICE_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace ios {
class ChromeBrowserState;
}  // namespace ios

namespace ntp_snippets {
class NTPSnippetsService;
}  // namespace ntp_snippets

// A factory to create NTPSnippetsService and associate them to
// ios::ChromeBrowserState.
class IOSChromeNTPSnippetsServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static IOSChromeNTPSnippetsServiceFactory* GetInstance();
  static ntp_snippets::NTPSnippetsService* GetForBrowserState(
      ios::ChromeBrowserState* browser_state);

 private:
  friend struct base::DefaultSingletonTraits<
      IOSChromeNTPSnippetsServiceFactory>;

  IOSChromeNTPSnippetsServiceFactory();
  ~IOSChromeNTPSnippetsServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;

  DISALLOW_COPY_AND_ASSIGN(IOSChromeNTPSnippetsServiceFactory);
};

#endif  // IOS_CHROME_BROWSER_NTP_SNIPPETS_IOS_CHROME_NTP_SNIPPETS_SERVICE_FACTORY_H_
