// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_USER_EVENT_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SYNC_USER_EVENT_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace syncer {
class UserEventService;
}  // namespace syncer

namespace browser_sync {

class UserEventServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static syncer::UserEventService* GetForProfile(Profile* profile);
  static UserEventServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<UserEventServiceFactory>;

  UserEventServiceFactory();
  ~UserEventServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(UserEventServiceFactory);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_USER_EVENT_SERVICE_FACTORY_H_
