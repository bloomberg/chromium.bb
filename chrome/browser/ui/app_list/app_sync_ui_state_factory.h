// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_SYNC_UI_STATE_FACTORY_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_SYNC_UI_STATE_FACTORY_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class AppSyncUIState;
class Profile;

// Singleton that owns all AppSyncUIStates and associates them with profiles.
class AppSyncUIStateFactory : public BrowserContextKeyedServiceFactory {
 public:
  static AppSyncUIState* GetForProfile(Profile* profile);

  static AppSyncUIStateFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<AppSyncUIStateFactory>;

  AppSyncUIStateFactory();
  ~AppSyncUIStateFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(AppSyncUIStateFactory);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_SYNC_UI_STATE_FACTORY_H_
