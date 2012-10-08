// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_ACTION_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_ACTION_MANAGER_H_

#include <map>
#include <string>

#include "base/memory/linked_ptr.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class ExtensionAction;
class Profile;

namespace extensions {

class Extension;

// Owns the ExtensionActions associated with each extension.  These actions live
// while an extension is loaded and are destroyed on unload.
class ExtensionActionManager : public content::NotificationObserver {
 public:
  explicit ExtensionActionManager(Profile* profile);
  virtual ~ExtensionActionManager();

  // Called during ExtensionSystem::Shutdown(), when the associated
  // Profile is going to be destroyed.
  void Shutdown();

 private:
  // Implement content::NotificationObserver.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  Profile* const profile_;
  content::NotificationRegistrar registrar_;

  // Keyed by Extension ID.  These maps are populated when the extension is
  // loaded, and the entries are removed when the extension is unloaded.  Not
  // every extension has a page action or browser action, but all have a script
  // badge.
  typedef std::map<std::string, linked_ptr<ExtensionAction> > ExtIdToActionMap;
  ExtIdToActionMap page_actions_;
  ExtIdToActionMap browser_actions_;
  ExtIdToActionMap script_badges_;
};

}

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_ACTION_MANAGER_H_
