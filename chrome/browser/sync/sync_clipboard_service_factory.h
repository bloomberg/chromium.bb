// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNC_CLIPBOARD_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SYNC_SYNC_CLIPBOARD_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_clipboard_service.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/pref_registry/pref_registry_syncable.h"

class SyncClipboardServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static SyncClipboardServiceFactory* GetInstance();

  static SyncClipboardService* GetForProfile(Profile* profile);

 private:
  friend base::NoDestructor<SyncClipboardServiceFactory>;

  SyncClipboardServiceFactory();

  // BrowserContextKeyedServiceFactory:
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

#endif  // CHROME_BROWSER_SYNC_SYNC_CLIPBOARD_SERVICE_FACTORY_H_
