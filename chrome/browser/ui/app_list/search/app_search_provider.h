// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_APP_SEARCH_PROVIDER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_APP_SEARCH_PROVIDER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_vector.h"
#include "chrome/browser/ui/app_list/search/search_provider.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class AppListControllerDelegate;
class ExtensionService;
class ExtensionSet;
class Profile;

namespace app_list {

class AppSearchProvider : public SearchProvider,
                          public content::NotificationObserver {
 public:
  AppSearchProvider(Profile* profile,
                    AppListControllerDelegate* list_controller);
  virtual ~AppSearchProvider();

  // SearchProvider overrides:
  virtual void Start(const base::string16& query) OVERRIDE;
  virtual void Stop() OVERRIDE;

 private:
  class App;
  typedef ScopedVector<App> Apps;

  // Adds extensions to apps container if they should be displayed.
  void AddApps(const ExtensionSet* extensions, ExtensionService* service);
  void RefreshApps();

  // content::NotificationObserver overrides:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  Profile* profile_;
  AppListControllerDelegate* list_controller_;
  content::NotificationRegistrar registrar_;

  Apps apps_;

  DISALLOW_COPY_AND_ASSIGN(AppSearchProvider);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_APP_SEARCH_PROVIDER_H_
