// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_util.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_sync_service.h"
#include "chrome/browser/extensions/permissions_updater.h"
#include "chrome/browser/extensions/scripting_permissions_modifier.h"
#include "chrome/browser/extensions/shared_module_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/sync_helper.h"
#include "content/public/browser/site_instance.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/features/behavior_feature.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/app_isolation_info.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace extensions {
namespace util {

namespace {

const char kSupervisedUserExtensionPermissionIncreaseFieldTrialName[] =
    "SupervisedUserExtensionPermissionIncrease";

// The entry into the ExtensionPrefs for allowing an extension to script on
// all urls without explicit permission.
const char kExtensionAllowedOnAllUrlsPrefName[] =
    "extension_can_script_all_urls";

// The entry into the prefs for when a user has explicitly set the "extension
// allowed on all urls" pref.
const char kHasSetScriptOnAllUrlsPrefName[] = "has_set_script_all_urls";

// Returns true if |extension| should always be enabled in incognito mode.
bool IsWhitelistedForIncognito(const Extension* extension) {
  return FeatureProvider::GetBehaviorFeature(
             BehaviorFeature::kWhitelistedForIncognito)
      ->IsAvailableToExtension(extension)
      .is_available();
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

// Sets the preference for scripting on all urls to |allowed|, optionally
// updating the extension's active permissions (based on |update_permissions|).
void SetAllowedScriptingOnAllUrlsHelper(
    content::BrowserContext* context,
    const std::string& extension_id,
    bool allowed,
    bool update_permissions) {
  // TODO(devlin): Right now, we always need to have a value for this pref.
  // Once the scripts-require-action feature launches, we can change the set
  // to be null if false.
  ExtensionPrefs::Get(context)->UpdateExtensionPref(
      extension_id,
      kExtensionAllowedOnAllUrlsPrefName,
      new base::FundamentalValue(allowed));

  if (update_permissions) {
    const Extension* extension =
        ExtensionRegistry::Get(context)->enabled_extensions().GetByID(
            extension_id);
    if (extension) {
      ScriptingPermissionsModifier modifier(context, extension);
      if (allowed)
        modifier.GrantWithheldImpliedAllHosts();
      else
        modifier.WithholdImpliedAllHosts();

      // If this was an update to permissions, we also need to sync the change.
      ExtensionSyncService* sync_service = ExtensionSyncService::Get(context);
      if (sync_service)  // sync_service can be null in unittests.
        sync_service->SyncExtensionChangeIfNeeded(*extension);
    }
  }
}

}  // namespace

bool IsIncognitoEnabled(const std::string& extension_id,
                        content::BrowserContext* context) {
  const Extension* extension = ExtensionRegistry::Get(context)->
      GetExtensionById(extension_id, ExtensionRegistry::ENABLED);
  if (extension) {
    if (!util::CanBeIncognitoEnabled(extension))
      return false;
    // If this is an existing component extension we always allow it to
    // work in incognito mode.
    if (extension->location() == Manifest::COMPONENT)
      return true;
    if (IsWhitelistedForIncognito(extension))
      return true;
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
    if (!util::CanBeIncognitoEnabled(extension))
      return;

    // TODO(treib,kalman): Should this be Manifest::IsComponentLocation(..)?
    // (which also checks for EXTERNAL_COMPONENT).
    if (extension->location() == Manifest::COMPONENT) {
      // This shouldn't be called for component extensions unless it is called
      // by sync, for syncable component extensions.
      // See http://crbug.com/112290 and associated CLs for the sordid history.
      DCHECK(sync_helper::IsSyncableComponentExtension(extension));

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
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
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
  ExtensionPrefs* prefs = ExtensionPrefs::Get(context);
  if (!prefs->ReadPrefAsBoolean(extension_id,
                                kExtensionAllowedOnAllUrlsPrefName,
                                &allowed)) {
    // If there is no value present, we make one, defaulting it to the value of
    // the 'scripts require action' flag. If the flag is on, then the extension
    // does not have permission to script on all urls by default.
    allowed = DefaultAllowedScriptingOnAllUrls();
    SetAllowedScriptingOnAllUrlsHelper(context, extension_id, allowed, false);
  }
  return allowed;
}

void SetAllowedScriptingOnAllUrls(const std::string& extension_id,
                                  content::BrowserContext* context,
                                  bool allowed) {
  if (allowed != AllowedScriptingOnAllUrls(extension_id, context)) {
    SetAllowedScriptingOnAllUrlsHelper(context, extension_id, allowed, true);
    ExtensionPrefs::Get(context)->UpdateExtensionPref(
        extension_id,
        kHasSetScriptOnAllUrlsPrefName,
        new base::FundamentalValue(true));
  }
}

bool HasSetAllowedScriptingOnAllUrls(const std::string& extension_id,
                                     content::BrowserContext* context) {
  bool did_set = false;
  return ExtensionPrefs::Get(context)->ReadPrefAsBoolean(
      extension_id,
      kHasSetScriptOnAllUrlsPrefName,
      &did_set) && did_set;
}

bool DefaultAllowedScriptingOnAllUrls() {
  return !FeatureSwitch::scripts_require_action()->IsEnabled();
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

bool ShouldSync(const Extension* extension,
                content::BrowserContext* context) {
  return sync_helper::IsSyncable(extension) &&
         !util::IsEphemeralApp(extension->id(), context) &&
         !ExtensionPrefs::Get(context)->DoNotSync(extension->id());
}

bool IsExtensionIdle(const std::string& extension_id,
                     content::BrowserContext* context) {
  std::vector<std::string> ids_to_check;
  ids_to_check.push_back(extension_id);

  const Extension* extension =
      ExtensionRegistry::Get(context)
          ->GetExtensionById(extension_id, ExtensionRegistry::ENABLED);
  if (extension && extension->is_shared_module()) {
    // We have to check all the extensions that use this shared module for idle
    // to tell whether it is really 'idle'.
    SharedModuleService* service = ExtensionSystem::Get(context)
                                       ->extension_service()
                                       ->shared_module_service();
    scoped_ptr<ExtensionSet> dependents =
        service->GetDependentExtensions(extension);
    for (ExtensionSet::const_iterator i = dependents->begin();
         i != dependents->end();
         i++) {
      ids_to_check.push_back((*i)->id());
    }
  }

  ProcessManager* process_manager = ProcessManager::Get(context);
  for (std::vector<std::string>::const_iterator i = ids_to_check.begin();
       i != ids_to_check.end();
       i++) {
    const std::string id = (*i);
    ExtensionHost* host = process_manager->GetBackgroundHostForExtension(id);
    if (host)
      return false;

    scoped_refptr<content::SiteInstance> site_instance =
        process_manager->GetSiteInstanceForURL(
            Extension::GetBaseURLFromExtensionId(id));
    if (site_instance && site_instance->HasProcess())
      return false;

    if (!process_manager->GetRenderFrameHostsForExtension(id).empty())
      return false;
  }
  return true;
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

const gfx::ImageSkia& GetDefaultAppIcon() {
  return *ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      IDR_APP_DEFAULT_ICON);
}

const gfx::ImageSkia& GetDefaultExtensionIcon() {
  return *ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      IDR_EXTENSION_DEFAULT_ICON);
}

bool IsNewBookmarkAppsEnabled() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableNewBookmarkApps);
}

bool CanHostedAppsOpenInWindows() {
#if defined(OS_MACOSX)
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableHostedAppsInWindows);
#else
  return true;
#endif
}

bool IsExtensionSupervised(const Extension* extension, Profile* profile) {
  return extension->was_installed_by_custodian() && profile->IsSupervised();
}

bool NeedCustodianApprovalForPermissionIncrease() {
  const std::string group_name = base::FieldTrialList::FindFullName(
      kSupervisedUserExtensionPermissionIncreaseFieldTrialName);
  return group_name == "NeedCustodianApproval";
}

}  // namespace util
}  // namespace extensions
