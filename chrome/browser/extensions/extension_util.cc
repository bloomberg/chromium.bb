// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_util.h"

#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/banners/app_banner_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_sync_service.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/extensions/permissions_updater.h"
#include "chrome/browser/extensions/scripting_permissions_modifier.h"
#include "chrome/browser/extensions/shared_module_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/url_handlers/url_handlers_parser.h"
#include "chrome/common/extensions/sync_helper.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/site_instance.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/app_isolation_info.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/file_manager/app_id.h"
#endif

namespace extensions {
namespace util {

namespace {
// The entry into the prefs used to flag an extension as installed by custodian.
// It is relevant only for supervised users.
const char kWasInstalledByCustodianPrefName[] = "was_installed_by_custodian";

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
      bool syncable = sync_helper::IsSyncableComponentExtension(extension);
#if defined(OS_CHROMEOS)
      // For some users, the file manager app somehow ended up being synced even
      // though it's supposed to be unsyncable; see crbug.com/576964. If the bad
      // data ever gets cleaned up, this hack should be removed.
      syncable = syncable || extension->id() == file_manager::kFileManagerAppId;
#endif
      DCHECK(syncable);

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
             ::switches::kDisableExtensionsFileAccessCheck) ||
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

void SetWasInstalledByCustodian(const std::string& extension_id,
                                content::BrowserContext* context,
                                bool installed_by_custodian) {
  if (installed_by_custodian == WasInstalledByCustodian(extension_id, context))
    return;

  ExtensionPrefs* prefs = ExtensionPrefs::Get(context);

  prefs->UpdateExtensionPref(
      extension_id, kWasInstalledByCustodianPrefName,
      installed_by_custodian ? std::make_unique<base::Value>(true) : nullptr);
  ExtensionService* service =
      ExtensionSystem::Get(context)->extension_service();

  if (!installed_by_custodian) {
    // If installed_by_custodian changes to false, the extension may need to
    // be unloaded now.
    service->ReloadExtension(extension_id);
    return;
  }

  ExtensionRegistry* registry = ExtensionRegistry::Get(context);
  // If it is already enabled, do nothing.
  if (registry->enabled_extensions().Contains(extension_id))
    return;

  // If the extension was disabled due to management policy, try to re-enable
  // it. Example is a pre-installed extension that was disabled when a
  // supervised user flag has been received.
  // Note: EnableExtension will fail if the extension still needs to be disabled
  // due to manangement policy.
  if (registry->disabled_extensions().Contains(extension_id) &&
      prefs->GetDisableReasons(extension_id) ==
          disable_reason::DISABLE_BLOCKED_BY_POLICY) {
    service->EnableExtension(extension_id);
  }
}

bool WasInstalledByCustodian(const std::string& extension_id,
                             content::BrowserContext* context) {
  bool installed_by_custodian = false;
  ExtensionPrefs* prefs = ExtensionPrefs::Get(context);
  prefs->ReadPrefAsBoolean(extension_id, kWasInstalledByCustodianPrefName,
                           &installed_by_custodian);
  return installed_by_custodian;
}

bool IsAppLaunchable(const std::string& extension_id,
                     content::BrowserContext* context) {
  int reason = ExtensionPrefs::Get(context)->GetDisableReasons(extension_id);
  return !((reason & disable_reason::DISABLE_UNSUPPORTED_REQUIREMENT) ||
           (reason & disable_reason::DISABLE_CORRUPTED));
}

bool IsAppLaunchableWithoutEnabling(const std::string& extension_id,
                                    content::BrowserContext* context) {
  return ExtensionRegistry::Get(context)->GetExtensionById(
      extension_id, ExtensionRegistry::ENABLED) != NULL;
}

bool ShouldSync(const Extension* extension,
                content::BrowserContext* context) {
  return sync_helper::IsSyncable(extension) &&
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
    std::unique_ptr<ExtensionSet> dependents =
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

std::unique_ptr<base::DictionaryValue> GetExtensionInfo(
    const Extension* extension) {
  DCHECK(extension);
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue);

  dict->SetString("id", extension->id());
  dict->SetString("name", extension->name());

  GURL icon = extensions::ExtensionIconSource::GetIconURL(
      extension, extension_misc::EXTENSION_ICON_SMALLISH,
      ExtensionIconSet::MATCH_BIGGER,
      false);  // Not grayscale.
  dict->SetString("icon", icon.spec());

  return dict;
}

const gfx::ImageSkia& GetDefaultAppIcon() {
  return *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      IDR_APP_DEFAULT_ICON);
}

const gfx::ImageSkia& GetDefaultExtensionIcon() {
  return *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      IDR_EXTENSION_DEFAULT_ICON);
}

bool IsExtensionSupervised(const Extension* extension, Profile* profile) {
  return WasInstalledByCustodian(extension->id(), profile) &&
         profile->IsSupervised();
}

const Extension* GetInstalledPwaForUrl(
    content::BrowserContext* context,
    const GURL& url,
    base::Optional<LaunchContainer> launch_container_filter) {
  DCHECK(base::FeatureList::IsEnabled(::features::kDesktopPWAWindowing));
  const ExtensionPrefs* prefs = ExtensionPrefs::Get(context);
  for (scoped_refptr<const Extension> app :
       ExtensionRegistry::Get(context)->enabled_extensions()) {
    if (!app->from_bookmark())
      continue;
    if (!BookmarkAppIsLocallyInstalled(prefs, app.get()))
      continue;
    if (launch_container_filter &&
        GetLaunchContainer(prefs, app.get()) != *launch_container_filter) {
      continue;
    }
    if (UrlHandlers::CanBookmarkAppHandleUrl(app.get(), url))
      return app.get();
  }
  return nullptr;
}

const Extension* GetPwaForSecureActiveTab(Browser* browser) {
  switch (browser->location_bar_model()->GetSecurityLevel()) {
    case security_state::SECURITY_LEVEL_COUNT:
      NOTREACHED();
      FALLTHROUGH;
    case security_state::NONE:
    case security_state::HTTP_SHOW_WARNING:
    case security_state::DANGEROUS:
      return nullptr;
    case security_state::EV_SECURE:
    case security_state::SECURE:
    case security_state::SECURE_WITH_POLICY_INSTALLED_CERT:
      break;
  }
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  return GetInstalledPwaForUrl(
      web_contents->GetBrowserContext(),
      web_contents->GetMainFrame()->GetLastCommittedURL());
}

bool IsWebContentsInAppWindow(content::WebContents* web_contents) {
  // TODO(loyso): Unify this check as a util (including
  // MaybeCreateHostedAppController).
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  return browser && browser->web_app_controller();
}

}  // namespace util
}  // namespace extensions
