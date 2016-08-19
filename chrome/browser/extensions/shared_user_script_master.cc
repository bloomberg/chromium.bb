// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/shared_user_script_master.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/manifest_handlers/content_scripts_handler.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/host_id.h"

namespace extensions {

SharedUserScriptMaster::SharedUserScriptMaster(Profile* profile)
    : loader_(profile,
              HostID(),
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
  const UserScriptList& script_list =
      ContentScriptsInfo::GetContentScripts(extension);
  std::set<UserScriptIDPair> scripts_to_remove;
  for (const std::unique_ptr<UserScript>& script : script_list)
    scripts_to_remove.insert(UserScriptIDPair(script->id(), script->host_id()));
  loader_.RemoveScripts(scripts_to_remove);
}

std::unique_ptr<UserScriptList> SharedUserScriptMaster::GetScriptsMetadata(
    const Extension* extension) {
  bool incognito_enabled = util::IsIncognitoEnabled(extension->id(), profile_);
  const UserScriptList& script_list =
      ContentScriptsInfo::GetContentScripts(extension);
  std::unique_ptr<UserScriptList> script_vector(new UserScriptList());
  script_vector->reserve(script_list.size());
  for (const std::unique_ptr<UserScript>& script : script_list) {
    std::unique_ptr<UserScript> script_copy =
        UserScript::CopyMetadataFrom(*script);
    script_copy->set_incognito_enabled(incognito_enabled);
    script_vector->push_back(std::move(script_copy));
  }
  return script_vector;
}

}  // namespace extensions
