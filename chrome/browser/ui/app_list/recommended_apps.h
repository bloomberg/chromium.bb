// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_RECOMMENDED_APPS_H_
#define CHROME_BROWSER_UI_APP_LIST_RECOMMENDED_APPS_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/scoped_observer.h"
#include "extensions/browser/extension_registry_observer.h"

class Profile;

namespace extensions {
class ExtensionRegistry;
}

namespace app_list {

class RecommendedAppsObserver;

// A class that maintains a list of recommended apps by watching changes
// to app state.
class RecommendedApps : public extensions::ExtensionRegistryObserver {
 public:
  typedef std::vector<scoped_refptr<const extensions::Extension> > Apps;

  explicit RecommendedApps(Profile* profile);
  virtual ~RecommendedApps();

  void AddObserver(RecommendedAppsObserver* observer);
  void RemoveObserver(RecommendedAppsObserver* observer);

  const Apps& apps() const { return apps_; }

 private:
  void Update();

  // extensions::ExtensionRegistryObserver overrides:
  virtual void OnExtensionWillBeInstalled(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension,
      bool is_update,
      bool from_ephemeral,
      const std::string& old_name) OVERRIDE;
  virtual void OnExtensionLoaded(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension) OVERRIDE;
  virtual void OnExtensionUnloaded(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension,
      extensions::UnloadedExtensionInfo::Reason reason) OVERRIDE;
  virtual void OnExtensionUninstalled(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension,
      extensions::UninstallReason reason) OVERRIDE;

  Profile* profile_;
  PrefChangeRegistrar pref_change_registrar_;

  Apps apps_;
  ObserverList<RecommendedAppsObserver, true> observers_;

  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver>
      extension_registry_observer_;

  DISALLOW_COPY_AND_ASSIGN(RecommendedApps);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_RECOMMENDED_APPS_H_
