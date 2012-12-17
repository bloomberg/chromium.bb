// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_ACTION_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_ACTION_MANAGER_H_

#include <map>
#include <string>

#include "base/memory/linked_ptr.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class ExtensionAction;
class Profile;

namespace extensions {

class Extension;

// Owns the ExtensionActions associated with each extension.  These actions live
// while an extension is loaded and are destroyed on unload.
class ExtensionActionManager : public ProfileKeyedService,
                               public content::NotificationObserver {
 public:
  explicit ExtensionActionManager(Profile* profile);
  virtual ~ExtensionActionManager();

  // Returns this profile's ExtensionActionManager.  One instance is
  // shared between a profile and its incognito version.
  static ExtensionActionManager* Get(Profile* profile);

  // Retrieves the page action, browser action, or script badge for |extension|.
  // If the result is not NULL, it remains valid until the extension is
  // unloaded.
  ExtensionAction* GetPageAction(const extensions::Extension& extension) const;
  ExtensionAction* GetBrowserAction(
      const extensions::Extension& extension) const;
  ExtensionAction* GetScriptBadge(const extensions::Extension& extension) const;
  ExtensionAction* GetSystemIndicator(
      const extensions::Extension& extension) const;

 private:
  // Implement content::NotificationObserver.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  content::NotificationRegistrar registrar_;
  Profile* profile_;

  // Keyed by Extension ID.  These maps are populated lazily when their
  // ExtensionAction is first requested, and the entries are removed when the
  // extension is unloaded.  Not every extension has a page action or browser
  // action, but all have a script badge.
  typedef std::map<std::string, linked_ptr<ExtensionAction> > ExtIdToActionMap;
  mutable ExtIdToActionMap page_actions_;
  mutable ExtIdToActionMap browser_actions_;
  mutable ExtIdToActionMap script_badges_;
  mutable ExtIdToActionMap system_indicators_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_ACTION_MANAGER_H_
