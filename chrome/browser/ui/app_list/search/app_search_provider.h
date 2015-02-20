// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_APP_SEARCH_PROVIDER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_APP_SEARCH_PROVIDER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "extensions/browser/extension_registry_observer.h"
#include "ui/app_list/search_provider.h"

class AppListControllerDelegate;
class Profile;

namespace base {
class Clock;
}

namespace extensions {
class ExtensionRegistry;
class ExtensionSet;
}

namespace app_list {

class AppListItemList;

namespace test {
class AppSearchProviderTest;
}

class AppSearchProvider : public SearchProvider,
                          public extensions::ExtensionRegistryObserver {
 public:
  AppSearchProvider(Profile* profile,
                    AppListControllerDelegate* list_controller,
                    scoped_ptr<base::Clock> clock,
                    AppListItemList* top_level_item_list);
  ~AppSearchProvider() override;

  // SearchProvider overrides:
  void Start(bool is_voice_query, const base::string16& query) override;
  void Stop() override;

 private:
  class App;
  typedef ScopedVector<App> Apps;

  void UpdateResults();

  // Adds extensions to apps container if they should be displayed.
  void AddApps(const extensions::ExtensionSet& extensions);
  void RefreshApps();

  // extensions::ExtensionRegistryObserver overrides:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const extensions::Extension* extension,
                              extensions::UninstallReason reason) override;

  Profile* profile_;
  AppListControllerDelegate* list_controller_;

  base::string16 query_;

  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver>
      extension_registry_observer_;

  Apps apps_;

  AppListItemList* top_level_item_list_;

  scoped_ptr<base::Clock> clock_;
  base::WeakPtrFactory<AppSearchProvider> update_results_factory_;

  DISALLOW_COPY_AND_ASSIGN(AppSearchProvider);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_APP_SEARCH_PROVIDER_H_
