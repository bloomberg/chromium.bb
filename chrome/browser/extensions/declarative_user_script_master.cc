// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/declarative_user_script_master.h"

#include <set>

#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_registry.h"

namespace extensions {

DeclarativeUserScriptMaster::DeclarativeUserScriptMaster(
    Profile* profile,
    const ExtensionId& extension_id)
    : extension_id_(extension_id),
      loader_(profile,
              extension_id,
              false /* listen_for_extension_system_loaded */),
      extension_registry_observer_(this) {
  extension_registry_observer_.Add(ExtensionRegistry::Get(profile));
}

DeclarativeUserScriptMaster::~DeclarativeUserScriptMaster() {
}

void DeclarativeUserScriptMaster::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionInfo::Reason reason) {
  if (extension_id_ == extension->id())
    ClearScripts();
}

void DeclarativeUserScriptMaster::AddScript(const UserScript& script) {
  std::set<UserScript> set;
  set.insert(script);
  loader_.AddScripts(set);
}

void DeclarativeUserScriptMaster::RemoveScript(const UserScript& script) {
  std::set<UserScript> set;
  set.insert(script);
  loader_.RemoveScripts(set);
}

void DeclarativeUserScriptMaster::ClearScripts() {
  loader_.ClearScripts();
}

}  // namespace extensions
