// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_CLIENT_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_CLIENT_SERVICE_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace send_tab_to_self {
class SendTabToSelfClientService;

// Singleton that owns the SendTabToSelfClientService and associates them with
// Profile.
class SendTabToSelfClientServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static send_tab_to_self::SendTabToSelfClientService* GetForProfile(
      Profile* profile);
  static SendTabToSelfClientServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<SendTabToSelfClientServiceFactory>;

  SendTabToSelfClientServiceFactory();
  ~SendTabToSelfClientServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  bool ServiceIsCreatedWithBrowserContext() const override;

  bool ServiceIsNULLWhileTesting() const override;

  DISALLOW_COPY_AND_ASSIGN(SendTabToSelfClientServiceFactory);
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_CLIENT_SERVICE_FACTORY_H_
