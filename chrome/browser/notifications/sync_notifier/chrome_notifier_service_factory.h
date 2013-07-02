// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SYNC_NOTIFIER_CHROME_NOTIFIER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_NOTIFICATIONS_SYNC_NOTIFIER_CHROME_NOTIFIER_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service_factory.h"

class CommandLine;

namespace notifier {

class ChromeNotifierService;

class ChromeNotifierServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static ChromeNotifierService* GetForProfile(
      Profile* profile, Profile::ServiceAccessType sat);

  static ChromeNotifierServiceFactory* GetInstance();

  // Based on command line switches, make the call to use SyncedNotifications or
  // not.
  // TODO(petewil): Remove this when the SyncedNotifications feature is ready
  // to be turned on by default, and just use a disable switch instead then.
  static bool UseSyncedNotifications(CommandLine* command_line);

 private:
  friend struct DefaultSingletonTraits<ChromeNotifierServiceFactory>;

  ChromeNotifierServiceFactory();
  virtual ~ChromeNotifierServiceFactory();

  // BrowserContextKeyedServiceFactory:
  virtual BrowserContextKeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;
};

}  // namespace notifier

#endif  // CHROME_BROWSER_NOTIFICATIONS_SYNC_NOTIFIER_CHROME_NOTIFIER_SERVICE_FACTORY_H_
