// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_util.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_sync_service.h"
#include "chrome/browser/extensions/permissions_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/manifest_handlers/app_isolation_info.h"
#include "chrome/common/extensions/sync_helper.h"
#include "content/public/browser/site_instance.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/features/simple_feature.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace extensions {
namespace util {

namespace {

// The entry into the ExtensionPrefs for allowing an extension to script on
// all urls without explicit permission.
const char kExtensionAllowedOnAllUrlsPrefName[] =
    "extension_can_script_all_urls";

// Returns true if |extension_id| for an external component extension should
// always be enabled in incognito windows.
bool IsWhitelistedForIncognito(const std::string& extension_id) {
  static const char* kExtensionWhitelist[] = {
    "D5736E4B5CF695CB93A2FB57E4FDC6E5AFAB6FE2",  // http://crbug.com/312900
    "D57DE394F36DC1C3220E7604C575D29C51A6C495",  // http://crbug.com/319444
    "3F65507A3B39259B38C8173C6FFA3D12DF64CCE9"   // http://crbug.com/371562
  };

  return extensions::SimpleFeature::IsIdInList(
      extension_id,
      std::set<std::string>(
          kExtensionWhitelist,
          kExtensionWhitelist + arraysize(kExtensionWhitelist)));
}

// Returns |extension_id|. See note below.
std::string ReloadExtensionIfEnabled(const std::string& extension_id,
                                     content::BrowserContext* context) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(context);
  bool extension_is_enabled =
      registry->enabled_extensions().Contains(extension_id);

  if (!extension_is_enabled)
    return extension_id;

  // When we reload the extension the ID may be invalidated if we've passed it
  // by const ref everywhere. Make a copy to be safe. http://crbug.com/103762
  std::string id = extension_id;
  ExtensionService* service =
      ExtensionSystem::Get(context)->extension_service();
  CHECK(service);
  service->ReloadExtension(id);
  return id;
}

}  // namespace

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
    if (extension->location() == Manifest::EXTERNAL_COMPONENT &&
        IsWhitelistedForIncognito(extension_id)) {
      return true;
    }
  }

  return ExtensionPrefs::Get(context)->IsIncognitoEnabled(extension_id);
}

void SetIsIncognitoEnabled(const std::string& extension_id,
                           content::BrowserContext* context,
                           bool enabled) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(context);
  const Extension* extension =
      registry->GetExtensionById(extension_id, ExtensionRegistry::EVERYTHING);

  if (extension) {
    if (!extension->can_be_incognito_enabled())
      return;

    if (extension->location() == Manifest::COMPONENT) {
      // This shouldn't be called for component extensions unless it is called
      // by sync, for syncable component extensions.
      // See http://crbug.com/112290 and associated CLs for the sordid history.
      DCHECK(sync_helper::IsSyncable(extension));

      // If we are here, make sure the we aren't trying to change the value.
      DCHECK_EQ(enabled, IsIncognitoEnabled(extension_id, context));
      return;
    }
  }

  ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(context);
  // Broadcast unloaded and loaded events to update browser state. Only bother
  // if the value changed and the extension is actually enabled, since there is
  // no UI otherwise.
  bool old_enabled = extension_prefs->IsIncognitoEnabled(extension_id);
  if (enabled == old_enabled)
    return;

  extension_prefs->SetIsIncognitoEnabled(extension_id, enabled);

  std::string id = ReloadExtensionIfEnabled(extension_id, context);

  // Reloading the extension invalidates the |extension| pointer.
  extension = registry->GetExtensionById(id, ExtensionRegistry::EVERYTHING);
  if (extension) {
    Profile* profile = Profile::FromBrowserContext(context);
    ExtensionSyncService::Get(profile)->SyncExtensionChangeIfNeeded(*extension);
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
  // Reload to update browser state. Only bother if the value changed and the
  // extension is actually enabled, since there is no UI otherwise.
  if (allow == AllowFileAccess(extension_id, context))
    return;

  ExtensionPrefs::Get(context)->SetAllowFileAccess(extension_id, allow);

  ReloadExtensionIfEnabled(extension_id, context);
}

bool AllowedScriptingOnAllUrls(const std::string& extension_id,
                               content::BrowserContext* context) {
  bool allowed = false;
  return ExtensionPrefs::Get(context)->ReadPrefAsBoolean(
             extension_id,
             kExtensionAllowedOnAllUrlsPrefName,
             &allowed) &&
         allowed;
}

void SetAllowedScriptingOnAllUrls(const std::string& extension_id,
                                  content::BrowserContext* context,
                                  bool allowed) {
  if (allowed == AllowedScriptingOnAllUrls(extension_id, context))
    return;  // Nothing to do here.

  ExtensionPrefs::Get(context)->UpdateExtensionPref(
      extension_id,
      kExtensionAllowedOnAllUrlsPrefName,
      allowed ? new base::FundamentalValue(true) : NULL);

  const Extension* extension =
      ExtensionRegistry::Get(context)->enabled_extensions().GetByID(
          extension_id);
  if (extension) {
    PermissionsUpdater updater(context);
    if (allowed)
      updater.GrantWithheldImpliedAllHosts(extension);
    else
      updater.WithholdImpliedAllHosts(extension);
  }
}

bool ScriptsMayRequireActionForExtension(const Extension* extension) {
  // An extension requires user action to execute scripts iff the switch to do
  // so is enabled, the extension shows up in chrome:extensions (so the user can
  // grant withheld permissions), the extension is not part of chrome or
  // corporate policy, and also not on the scripting whitelist.
  return FeatureSwitch::scripts_require_action()->IsEnabled() &&
      extension->ShouldDisplayInExtensionSettings() &&
      !Manifest::IsPolicyLocation(extension->location()) &&
      !Manifest::IsComponentLocation(extension->location()) &&
      !PermissionsData::CanExecuteScriptEverywhere(extension);
}

bool IsAppLaunchable(const std::string& extension_id,
                     content::BrowserContext* context) {
  int reason = ExtensionPrefs::Get(context)->GetDisableReasons(extension_id);
  return !((reason & Extension::DISABLE_UNSUPPORTED_REQUIREMENT) ||
           (reason & Extension::DISABLE_CORRUPTED));
}

bool IsAppLaunchableWithoutEnabling(const std::string& extension_id,
                                    content::BrowserContext* context) {
  return ExtensionRegistry::Get(context)->GetExtensionById(
      extension_id, ExtensionRegistry::ENABLED) != NULL;
}

bool ShouldSyncExtension(const Extension* extension,
                         content::BrowserContext* context) {
  return sync_helper::IsSyncableExtension(extension) &&
         !ExtensionPrefs::Get(context)->DoNotSync(extension->id());
}

bool ShouldSyncApp(const Extension* app, content::BrowserContext* context) {
  return sync_helper::IsSyncableApp(app) &&
         !util::IsEphemeralApp(app->id(), context) &&
         !ExtensionPrefs::Get(context)->DoNotSync(app->id());
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
  if (!extension)
    return false;

  return AppIsolationInfo::HasIsolatedStorage(extension);
}

const gfx::ImageSkia& GetDefaultAppIcon() {
  return *ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      IDR_APP_DEFAULT_ICON);
}

const gfx::ImageSkia& GetDefaultExtensionIcon() {
  return *ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      IDR_EXTENSION_DEFAULT_ICON);
}

bool IsStreamlinedHostedAppsEnabled() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableStreamlinedHostedApps);
}

}  // namespace util
}  // namespace extensions
