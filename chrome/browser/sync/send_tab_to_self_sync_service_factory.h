// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SEND_TAB_TO_SELF_SYNC_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SYNC_SEND_TAB_TO_SELF_SYNC_SERVICE_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace send_tab_to_self {
class SendTabToSelfSyncService;
}  // namespace send_tab_to_self

class SendTabToSelfSyncServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static send_tab_to_self::SendTabToSelfSyncService* GetForProfile(
      Profile* profile);
  static SendTabToSelfSyncServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<SendTabToSelfSyncServiceFactory>;

  SendTabToSelfSyncServiceFactory();
  ~SendTabToSelfSyncServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(SendTabToSelfSyncServiceFactory);
};

#endif  // CHROME_BROWSER_SYNC_SEND_TAB_TO_SELF_SYNC_SERVICE_FACTORY_H_
