// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_ACTION_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_ACTION_MANAGER_H_

#include <map>
#include <string>

#include "base/scoped_observer.h"
#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/extension_registry_observer.h"

class ExtensionAction;
class Profile;

namespace extensions {

class Extension;
class ExtensionRegistry;

// Owns the ExtensionActions associated with each extension.  These actions live
// while an extension is loaded and are destroyed on unload.
class ExtensionActionManager : public KeyedService,
                               public ExtensionRegistryObserver {
 public:
  explicit ExtensionActionManager(Profile* profile);
  virtual ~ExtensionActionManager();

  // Returns this profile's ExtensionActionManager.  One instance is
  // shared between a profile and its incognito version.
  static ExtensionActionManager* Get(Profile* profile);

  // Retrieves the page action, or browser action for |extension|.
  // If the result is not NULL, it remains valid until the extension is
  // unloaded.
  ExtensionAction* GetPageAction(const extensions::Extension& extension) const;
  ExtensionAction* GetBrowserAction(
      const extensions::Extension& extension) const;
  ExtensionAction* GetSystemIndicator(
      const extensions::Extension& extension) const;

  // Gets the best fit ExtensionAction for the given |extension|. This takes
  // into account |extension|'s browser or page actions, if any, along with its
  // name and any declared icons.
  scoped_ptr<ExtensionAction> GetBestFitAction(
      const extensions::Extension& extension,
      extensions::ActionInfo::Type type) const;

 private:
  // Implement ExtensionRegistryObserver.
  virtual void OnExtensionUnloaded(content::BrowserContext* browser_context,
                                   const Extension* extension,
                                   UnloadedExtensionInfo::Reason reason)
      OVERRIDE;

  Profile* profile_;

  // Listen to extension unloaded notifications.
  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_;

  // Keyed by Extension ID.  These maps are populated lazily when their
  // ExtensionAction is first requested, and the entries are removed when the
  // extension is unloaded.  Not every extension has a page action or browser
  // action.
  typedef std::map<std::string, linked_ptr<ExtensionAction> > ExtIdToActionMap;
  mutable ExtIdToActionMap page_actions_;
  mutable ExtIdToActionMap browser_actions_;
  mutable ExtIdToActionMap system_indicators_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_ACTION_MANAGER_H_
