// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SHARED_USER_SCRIPT_MASTER_H_
#define CHROME_BROWSER_EXTENSIONS_SHARED_USER_SCRIPT_MASTER_H_

#include <set>

#include "base/scoped_observer.h"
#include "chrome/browser/extensions/user_script_loader.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension.h"
#include "extensions/common/user_script.h"

namespace content {
class BrowserContext;
}

class Profile;

namespace extensions {

class ExtensionRegistry;

// Manages statically-defined user scripts for all extensions. Owns a
// UserScriptLoader to which file loading and shared memory management
// operations are delegated.
class SharedUserScriptMaster : public ExtensionRegistryObserver {
 public:
  explicit SharedUserScriptMaster(Profile* profile);
  virtual ~SharedUserScriptMaster();

  // Provides access to loader state method: scripts_ready().
  bool scripts_ready() const { return loader_.scripts_ready(); }

 private:
  // ExtensionRegistryObserver implementation.
  virtual void OnExtensionLoaded(content::BrowserContext* browser_context,
                                 const Extension* extension) OVERRIDE;
  virtual void OnExtensionUnloaded(
      content::BrowserContext* browser_context,
      const Extension* extension,
      UnloadedExtensionInfo::Reason reason) OVERRIDE;

  // Gets an extension's scripts' metadata; i.e., gets a list of UserScript
  // objects that contains script info, but not the contents of the scripts.
  const std::set<UserScript> GetScriptsMetadata(const Extension* extension);

  // Script loader that handles loading contents of scripts into shared memory
  // and notifying renderers of scripts in shared memory.
  UserScriptLoader loader_;

  // The profile for which the scripts managed here are installed.
  Profile* profile_;

  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_;

  DISALLOW_COPY_AND_ASSIGN(SharedUserScriptMaster);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SHARED_USER_SCRIPT_MASTER_H_
