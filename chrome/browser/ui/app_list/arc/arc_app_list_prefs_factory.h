// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_LIST_PREFS_FACTORY_H_
#define CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_LIST_PREFS_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class ArcAppListPrefs;

class ArcAppListPrefsFactory : public BrowserContextKeyedServiceFactory {
 public:
  static ArcAppListPrefs* GetForBrowserContext(
      content::BrowserContext* context);

  static ArcAppListPrefsFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<ArcAppListPrefsFactory>;

  ArcAppListPrefsFactory();
  ~ArcAppListPrefsFactory() override;

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(ArcAppListPrefsFactory);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_LIST_PREFS_FACTORY_H_
