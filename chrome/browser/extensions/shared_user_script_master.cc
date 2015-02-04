// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/shared_user_script_master.h"

#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/manifest_handlers/content_scripts_handler.h"
#include "extensions/browser/extension_registry.h"

namespace extensions {

SharedUserScriptMaster::SharedUserScriptMaster(Profile* profile)
    : loader_(profile,
              std::string() /* owner_extension_id */,
              true /* listen_for_extension_system_loaded */),
      profile_(profile),
      extension_registry_observer_(this) {
  extension_registry_observer_.Add(ExtensionRegistry::Get(profile_));
}

SharedUserScriptMaster::~SharedUserScriptMaster() {
}

void SharedUserScriptMaster::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  loader_.AddScripts(GetScriptsMetadata(extension));
}

void SharedUserScriptMaster::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionInfo::Reason reason) {
  loader_.RemoveScripts(GetScriptsMetadata(extension));
}

const std::set<UserScript> SharedUserScriptMaster::GetScriptsMetadata(
    const Extension* extension) {
  bool incognito_enabled = util::IsIncognitoEnabled(extension->id(), profile_);
  const UserScriptList& script_list =
      ContentScriptsInfo::GetContentScripts(extension);
  std::set<UserScript> script_set;
  for (UserScriptList::const_iterator it = script_list.begin();
       it != script_list.end();
       ++it) {
    UserScript script = *it;
    script.set_incognito_enabled(incognito_enabled);
    script_set.insert(script);
  }

  return script_set;
}

}  // namespace extensions
