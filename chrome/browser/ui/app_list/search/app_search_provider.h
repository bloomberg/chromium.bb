// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_APP_SEARCH_PROVIDER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_APP_SEARCH_PROVIDER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_vector.h"
#include "base/scoped_observer.h"
#include "extensions/browser/extension_registry_observer.h"
#include "ui/app_list/search_provider.h"

class AppListControllerDelegate;
class Profile;

namespace extensions {
class ExtensionRegistry;
class ExtensionSet;
}

namespace app_list {

namespace test {
class AppSearchProviderTest;
}

class AppSearchProvider : public SearchProvider,
                          public extensions::ExtensionRegistryObserver {
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

  friend test::AppSearchProviderTest;

  void StartImpl(const base::Time& current_time, const base::string16& query);

  // Adds extensions to apps container if they should be displayed.
  void AddApps(const extensions::ExtensionSet& extensions);
  void RefreshApps();

  // extensions::ExtensionRegistryObserver overrides:
  virtual void OnExtensionLoaded(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension) OVERRIDE;
  virtual void OnExtensionUninstalled(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension,
      extensions::UninstallReason reason) OVERRIDE;

  Profile* profile_;
  AppListControllerDelegate* list_controller_;

  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver>
      extension_registry_observer_;

  Apps apps_;

  DISALLOW_COPY_AND_ASSIGN(AppSearchProvider);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_APP_SEARCH_PROVIDER_H_
