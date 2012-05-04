// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_keybinding_registry.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/extension_set.h"
#include "chrome/browser/extensions/extension_service.h"

namespace extensions {

ExtensionKeybindingRegistry::ExtensionKeybindingRegistry(Profile* profile)
    : profile_(profile) {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                 content::Source<Profile>(profile->GetOriginalProfile()));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile->GetOriginalProfile()));
}

ExtensionKeybindingRegistry::~ExtensionKeybindingRegistry() {
}

void ExtensionKeybindingRegistry::Init() {
  ExtensionService* service = profile_->GetExtensionService();
  if (!service)
    return;  // ExtensionService can be null during testing.

  const ExtensionSet* extensions = service->extensions();
  ExtensionSet::const_iterator iter = extensions->begin();
  for (; iter != extensions->end(); ++iter)
    AddExtensionKeybinding(*iter);
}

bool ExtensionKeybindingRegistry::ShouldIgnoreCommand(
    const std::string& command) const {
  return command == extension_manifest_values::kPageActionKeybindingEvent ||
         command == extension_manifest_values::kBrowserActionKeybindingEvent;
}

void ExtensionKeybindingRegistry::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_LOADED:
      AddExtensionKeybinding(
          content::Details<const Extension>(details).ptr());
      break;
    case chrome::NOTIFICATION_EXTENSION_UNLOADED:
      RemoveExtensionKeybinding(
          content::Details<UnloadedExtensionInfo>(details)->extension);
      break;
    default:
      NOTREACHED();
      break;
  }
}

}  // namespace extensions
