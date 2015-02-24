// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_DECLARATIVE_USER_SCRIPT_MASTER_H_
#define EXTENSIONS_BROWSER_DECLARATIVE_USER_SCRIPT_MASTER_H_

#include "base/scoped_observer.h"
#include "extensions/browser/extension_user_script_loader.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class UserScript;

// Manages declarative user scripts for a single extension. Owns a
// UserScriptLoader to which file loading and shared memory management
// operations are delegated, and provides an interface for adding, removing,
// and clearing scripts.
class DeclarativeUserScriptMaster {
 public:
  DeclarativeUserScriptMaster(content::BrowserContext* browser_context,
                              const HostID& host_id);
  ~DeclarativeUserScriptMaster();

  // Adds script to shared memory region. This may not happen right away if a
  // script load is in progress.
  void AddScript(const UserScript& script);

  // Removes script from shared memory region. This may not happen right away if
  // a script load is in progress.
  void RemoveScript(const UserScript& script);

  // Removes all scripts from shared memory region. This may not happen right
  // away if a script load is in progress.
  void ClearScripts();

  const HostID& host_id() const { return host_id_; }

 private:
  // ID of host that owns scripts that this component manages.
  HostID host_id_;

  // Script loader that handles loading contents of scripts into shared memory
  // and notifying renderers of script updates.
  ExtensionUserScriptLoader loader_;

  DISALLOW_COPY_AND_ASSIGN(DeclarativeUserScriptMaster);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_DECLARATIVE_USER_SCRIPT_MASTER_H_
