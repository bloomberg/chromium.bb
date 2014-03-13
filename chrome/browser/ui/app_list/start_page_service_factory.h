// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_START_PAGE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UI_APP_LIST_START_PAGE_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace app_list {
class StartPageService;

// Singleton factory to create StartPageService.
class StartPageServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Gets or creates the instance of StartPageService for |profile|.
  static StartPageService* GetForProfile(Profile* profile);

  // Gets the singleton instance of this factory.
  static StartPageServiceFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<StartPageServiceFactory>;

  StartPageServiceFactory();
  virtual ~StartPageServiceFactory();

  // BrowserContextKeyedServiceFactory overrides:
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(StartPageServiceFactory);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_START_PAGE_SERVICE_FACTORY_H_
