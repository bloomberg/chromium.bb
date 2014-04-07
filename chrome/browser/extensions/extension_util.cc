// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_util.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_sync_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/manifest_handlers/app_isolation_info.h"
#include "chrome/common/extensions/sync_helper.h"
#include "content/public/browser/site_instance.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/incognito_info.h"

namespace extensions {
namespace util {

bool IsIncognitoEnabled(const std::string& extension_id,
                        content::BrowserContext* context) {
  const Extension* extension = ExtensionRegistry::Get(context)->
      GetExtensionById(extension_id, ExtensionRegistry::ENABLED);
  if (extension) {
    if (!extension->can_be_incognito_enabled())
      return false;
    // If this is an existing component extension we always allow it to
    // work in incognito mode.
    if (extension->location() == Manifest::COMPONENT)
      return true;
  }

  return ExtensionPrefs::Get(context)->IsIncognitoEnabled(extension_id);
}

void SetIsIncognitoEnabled(const std::string& extension_id,
                           content::BrowserContext* context,
                           bool enabled) {
  ExtensionService* service =
      ExtensionSystem::Get(context)->extension_service();
  CHECK(service);
  const Extension* extension = service->GetInstalledExtension(extension_id);

  if (extension) {
    if (!extension->can_be_incognito_enabled())
      return;

    if (extension->location() == Manifest::COMPONENT) {
      // This shouldn't be called for component extensions unless it is called
      // by sync, for syncable component extensions.
      // See http://crbug.com/112290 and associated CLs for the sordid history.
      DCHECK(sync_helper::IsSyncable(extension));

      // If we are here, make sure the we aren't trying to change the value.
      DCHECK_EQ(enabled, IsIncognitoEnabled(extension_id, service->profile()));
      return;
    }
  }

  ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(service->profile());
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
  if (extension) {
    ExtensionSyncService::Get(service->profile())->
        SyncExtensionChangeIfNeeded(*extension);
  }
}

bool CanCrossIncognito(const Extension* extension,
                       content::BrowserContext* context) {
  // We allow the extension to see events and data from another profile iff it
  // uses "spanning" behavior and it has incognito access. "split" mode
  // extensions only see events for a matching profile.
  CHECK(extension);
  return IsIncognitoEnabled(extension->id(), context) &&
         !IncognitoInfo::IsSplitMode(extension);
}

bool CanLoadInIncognito(const Extension* extension,
                        content::BrowserContext* context) {
  CHECK(extension);
  if (extension->is_hosted_app())
    return true;
  // Packaged apps and regular extensions need to be enabled specifically for
  // incognito (and split mode should be set).
  return IncognitoInfo::IsSplitMode(extension) &&
         IsIncognitoEnabled(extension->id(), context);
}

bool AllowFileAccess(const std::string& extension_id,
                     content::BrowserContext* context) {
  return CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kDisableExtensionsFileAccessCheck) ||
         ExtensionPrefs::Get(context)->AllowFileAccess(extension_id);
}

void SetAllowFileAccess(const std::string& extension_id,
                        content::BrowserContext* context,
                        bool allow) {
  ExtensionService* service =
      ExtensionSystem::Get(context)->extension_service();
  CHECK(service);

  // Reload to update browser state. Only bother if the value changed and the
  // extension is actually enabled, since there is no UI otherwise.
  if (allow == AllowFileAccess(extension_id, context))
    return;

  ExtensionPrefs::Get(context)->SetAllowFileAccess(extension_id, allow);

  bool extension_is_enabled = service->extensions()->Contains(extension_id);
  if (extension_is_enabled)
    service->ReloadExtension(extension_id);
}

bool IsAppLaunchable(const std::string& extension_id,
                     content::BrowserContext* context) {
  return !(ExtensionPrefs::Get(context)->GetDisableReasons(extension_id) &
           Extension::DISABLE_UNSUPPORTED_REQUIREMENT);
}

bool IsAppLaunchableWithoutEnabling(const std::string& extension_id,
                                    content::BrowserContext* context) {
  return ExtensionRegistry::Get(context)->GetExtensionById(
      extension_id, ExtensionRegistry::ENABLED) != NULL;
}

bool IsExtensionIdle(const std::string& extension_id,
                     content::BrowserContext* context) {
  ProcessManager* process_manager =
      ExtensionSystem::Get(context)->process_manager();
  DCHECK(process_manager);
  ExtensionHost* host =
      process_manager->GetBackgroundHostForExtension(extension_id);
  if (host)
    return false;

  content::SiteInstance* site_instance = process_manager->GetSiteInstanceForURL(
      Extension::GetBaseURLFromExtensionId(extension_id));
  if (site_instance && site_instance->HasProcess())
    return false;

  return process_manager->GetRenderViewHostsForExtension(extension_id).empty();
}

bool IsExtensionInstalledPermanently(const std::string& extension_id,
                                     content::BrowserContext* context) {
  const Extension* extension = ExtensionRegistry::Get(context)->
      GetExtensionById(extension_id, ExtensionRegistry::EVERYTHING);
  return extension && !extension->is_ephemeral();
}

GURL GetSiteForExtensionId(const std::string& extension_id,
                           content::BrowserContext* context) {
  return content::SiteInstance::GetSiteForURL(
      context, Extension::GetBaseURLFromExtensionId(extension_id));
}

scoped_ptr<base::DictionaryValue> GetExtensionInfo(const Extension* extension) {
  DCHECK(extension);
  scoped_ptr<base::DictionaryValue> dict(new base::DictionaryValue);

  dict->SetString("id", extension->id());
  dict->SetString("name", extension->name());

  GURL icon = extensions::ExtensionIconSource::GetIconURL(
      extension,
      extension_misc::EXTENSION_ICON_SMALLISH,
      ExtensionIconSet::MATCH_BIGGER,
      false,  // Not grayscale.
      NULL);  // Don't set bool if exists.
  dict->SetString("icon", icon.spec());

  return dict.Pass();
}

bool HasIsolatedStorage(const ExtensionInfo& info) {
  if (!info.extension_manifest.get())
    return false;

  std::string error;
  scoped_refptr<const Extension> extension(Extension::Create(
      info.extension_path,
      info.extension_location,
      *info.extension_manifest,
      Extension::NO_FLAGS,
      info.extension_id,
      &error));
  if (!extension.get())
    return false;

  return AppIsolationInfo::HasIsolatedStorage(extension.get());
}

bool SiteHasIsolatedStorage(const GURL& extension_site_url,
                            content::BrowserContext* context) {
  const Extension* extension = ExtensionRegistry::Get(context)->
      enabled_extensions().GetExtensionOrAppByURL(extension_site_url);
  if (extension)
    return AppIsolationInfo::HasIsolatedStorage(extension);

  if (extension_site_url.SchemeIs(kExtensionScheme)) {
    // The site URL may also be from an evicted ephemeral app. We do not
    // immediately delete their data when they are removed from extension
    // system.
    ExtensionPrefs* prefs = ExtensionPrefs::Get(context);
    DCHECK(prefs);
    scoped_ptr<ExtensionInfo> info = prefs->GetEvictedEphemeralAppInfo(
        extension_site_url.host());
    if (info.get())
      return HasIsolatedStorage(*info);
  }

  return false;
}

}  // namespace util
}  // namespace extensions
