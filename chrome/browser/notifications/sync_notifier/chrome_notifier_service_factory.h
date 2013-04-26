// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SYNC_NOTIFIER_CHROME_NOTIFIER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_NOTIFICATIONS_SYNC_NOTIFIER_CHROME_NOTIFIER_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace notifier {

class ChromeNotifierService;

class ChromeNotifierServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static ChromeNotifierService* GetForProfile(
      Profile* profile, Profile::ServiceAccessType sat);

  static ChromeNotifierServiceFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<ChromeNotifierServiceFactory>;

  ChromeNotifierServiceFactory();
  virtual ~ChromeNotifierServiceFactory();

  // ProfileKeyedServiceFactory:
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;
};

}  // namespace notifier

#endif  // CHROME_BROWSER_NOTIFICATIONS_SYNC_NOTIFIER_CHROME_NOTIFIER_SERVICE_FACTORY_H_
