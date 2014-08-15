// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_DECLARATIVE_USER_SCRIPT_MASTER_H_
#define CHROME_BROWSER_EXTENSIONS_DECLARATIVE_USER_SCRIPT_MASTER_H_

#include "base/scoped_observer.h"
#include "chrome/browser/extensions/user_script_loader.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension.h"

class Profile;

namespace extensions {

class ExtensionRegistry;
class UserScript;

// Manages declarative user scripts for a single extension. Owns a
// UserScriptLoader to which file loading and shared memory management
// operations are delegated, and provides an interface for adding, removing,
// and clearing scripts.
class DeclarativeUserScriptMaster : public ExtensionRegistryObserver {
 public:
  DeclarativeUserScriptMaster(Profile* profile,
                              const ExtensionId& extension_id);
  virtual ~DeclarativeUserScriptMaster();

  // Adds script to shared memory region. This may not happen right away if a
  // script load is in progress.
  void AddScript(const UserScript& script);

  // Removes script from shared memory region. This may not happen right away if
  // a script load is in progress.
  void RemoveScript(const UserScript& script);

  // Removes all scripts from shared memory region. This may not happen right
  // away if a script load is in progress.
  void ClearScripts();

  const ExtensionId& extension_id() const { return extension_id_; }

 private:
  // ExtensionRegistryObserver implementation.
  virtual void OnExtensionUnloaded(
      content::BrowserContext* browser_context,
      const Extension* extension,
      UnloadedExtensionInfo::Reason reason) OVERRIDE;

  // ID of extension that owns scripts that this component manages.
  ExtensionId extension_id_;

  // Script loader that handles loading contents of scripts into shared memory
  // and notifying renderers of script updates.
  UserScriptLoader loader_;

  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_;

  DISALLOW_COPY_AND_ASSIGN(DeclarativeUserScriptMaster);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_DECLARATIVE_USER_SCRIPT_MASTER_H_
