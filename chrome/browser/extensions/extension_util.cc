// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_util.h"

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/incognito_handler.h"
#include "chrome/common/extensions/sync_helper.h"
#include "extensions/common/manifest.h"

using extensions::Extension;
using extensions::ExtensionPrefs;

namespace extension_util {

bool IsIncognitoEnabled(const std::string& extension_id,
                        const ExtensionService* service) {
  if (!service)
    return false;

  const Extension* extension = service->GetInstalledExtension(extension_id);
  if (extension && !extension->can_be_incognito_enabled())
    return false;
  // If this is an existing component extension we always allow it to
  // work in incognito mode.
  if (extension && extension->location() == extensions::Manifest::COMPONENT)
    return true;
  if (extension && extension->force_incognito_enabled())
    return true;

  // Check the prefs.
  return service->extension_prefs()->IsIncognitoEnabled(extension_id);
}

void SetIsIncognitoEnabled(const std::string& extension_id,
                           ExtensionService* service,
                           bool enabled) {
  const Extension* extension = service->GetInstalledExtension(extension_id);
  if (extension && !extension->can_be_incognito_enabled())
    return;
  if (extension && extension->location() == extensions::Manifest::COMPONENT) {
    // This shouldn't be called for component extensions unless it is called
    // by sync, for syncable component extensions.
    // See http://crbug.com/112290 and associated CLs for the sordid history.
    DCHECK(extensions::sync_helper::IsSyncable(extension));

    // If we are here, make sure the we aren't trying to change the value.
    DCHECK_EQ(enabled, IsIncognitoEnabled(extension_id, service));
    return;
  }

  ExtensionPrefs* extension_prefs = service->extension_prefs();
  // Broadcast unloaded and loaded events to update browser state. Only bother
  // if the value changed and the extension is actually enabled, since there is
  // no UI otherwise.
  bool old_enabled = extension_prefs->IsIncognitoEnabled(extension_id);
  if (enabled == old_enabled)
    return;

  extension_prefs->SetIsIncognitoEnabled(extension_id, enabled);

  bool extension_is_enabled = service->extensions()->Contains(extension_id);

  // When we reload the extension the ID may be invalidated if we've passed it
  // by const ref everywhere. Make a copy to be safe.
  std::string id = extension_id;
  if (extension_is_enabled)
    service->ReloadExtension(id);

  // Reloading the extension invalidates the |extension| pointer.
  extension = service->GetInstalledExtension(id);
  if (extension)
    service->SyncExtensionChangeIfNeeded(*extension);
}

bool CanCrossIncognito(const Extension* extension,
                       const ExtensionService* service) {
  // We allow the extension to see events and data from another profile iff it
  // uses "spanning" behavior and it has incognito access. "split" mode
  // extensions only see events for a matching profile.
  CHECK(extension);
  return extension_util::IsIncognitoEnabled(extension->id(), service) &&
         !extensions::IncognitoInfo::IsSplitMode(extension);
}

bool CanLoadInIncognito(const Extension* extension,
                        const ExtensionService* service) {
  if (extension->is_hosted_app())
    return true;
  // Packaged apps and regular extensions need to be enabled specifically for
  // incognito (and split mode should be set).
  return extensions::IncognitoInfo::IsSplitMode(extension) &&
         extension_util::IsIncognitoEnabled(extension->id(), service);
}

bool AllowFileAccess(const Extension* extension,
                     const ExtensionService* service) {
  return (CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kDisableExtensionsFileAccessCheck) ||
          service->extension_prefs()->AllowFileAccess(extension->id()));
}

void SetAllowFileAccess(const Extension* extension,
                        ExtensionService* service,
                        bool allow) {
  // Reload to update browser state. Only bother if the value changed and the
  // extension is actually enabled, since there is no UI otherwise.
  bool old_allow = AllowFileAccess(extension, service);
  if (allow == old_allow)
    return;

  service->extension_prefs()->SetAllowFileAccess(extension->id(), allow);

  bool extension_is_enabled = service->extensions()->Contains(extension->id());
  if (extension_is_enabled)
    service->ReloadExtension(extension->id());
}

}  // namespace extension_util
