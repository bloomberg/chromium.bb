// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_sync_service.h"

#include "base/basictypes.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/bookmark_app_helper.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_sync_data.h"
#include "chrome/browser/extensions/extension_sync_service_factory.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/sync_start_util.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/sync_helper.h"
#include "chrome/common/web_application_info.h"
#include "extensions/browser/app_sorting.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/image_util.h"
#include "extensions/common/permissions/permission_message_provider.h"
#include "extensions/common/permissions/permissions_data.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_error_factory.h"

#if defined(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#endif

using extensions::AppSorting;
using extensions::Extension;
using extensions::ExtensionPrefs;
using extensions::ExtensionRegistry;
using extensions::ExtensionSet;
using extensions::ExtensionSyncData;
using extensions::ExtensionSystem;
using extensions::SyncBundle;

namespace {

void OnWebApplicationInfoLoaded(
    WebApplicationInfo synced_info,
    base::WeakPtr<ExtensionService> extension_service,
    const WebApplicationInfo& loaded_info) {
  DCHECK_EQ(synced_info.app_url, loaded_info.app_url);

  if (!extension_service)
    return;

  // Use the old icons if they exist.
  synced_info.icons = loaded_info.icons;
  CreateOrUpdateBookmarkApp(extension_service.get(), &synced_info);
}

// Returns the pref value for "all urls enabled" for the given extension id.
ExtensionSyncData::OptionalBoolean GetAllowedOnAllUrlsOptionalBoolean(
    const std::string& extension_id,
    content::BrowserContext* context) {
  bool allowed_on_all_urls =
      extensions::util::AllowedScriptingOnAllUrls(extension_id, context);
  // If the extension is not allowed on all urls (which is not the default),
  // then we have to sync the preference.
  if (!allowed_on_all_urls)
    return ExtensionSyncData::BOOLEAN_FALSE;

  // If the user has explicitly set a value, then we sync it.
  if (extensions::util::HasSetAllowedScriptingOnAllUrls(extension_id, context))
    return ExtensionSyncData::BOOLEAN_TRUE;

  // Otherwise, unset.
  return ExtensionSyncData::BOOLEAN_UNSET;
}

// Returns true if the sync type of |extension| matches |type|.
bool IsCorrectSyncType(const Extension& extension, syncer::ModelType type) {
  return (type == syncer::EXTENSIONS && extension.is_extension()) ||
         (type == syncer::APPS && extension.is_app());
}

syncer::SyncDataList ToSyncerSyncDataList(
    const std::vector<ExtensionSyncData>& data) {
  syncer::SyncDataList result;
  result.reserve(data.size());
  for (const ExtensionSyncData& item : data)
    result.push_back(item.GetSyncData());
  return result;
}

}  // namespace

struct ExtensionSyncService::PendingUpdate {
  PendingUpdate() : grant_permissions_and_reenable(false) {}
  PendingUpdate(const base::Version& version,
                bool grant_permissions_and_reenable)
    : version(version),
      grant_permissions_and_reenable(grant_permissions_and_reenable) {}

  base::Version version;
  bool grant_permissions_and_reenable;
};

ExtensionSyncService::ExtensionSyncService(Profile* profile)
    : profile_(profile),
      registry_observer_(this),
      prefs_observer_(this),
      flare_(sync_start_util::GetFlareForSyncableService(profile->GetPath())) {
  registry_observer_.Add(ExtensionRegistry::Get(profile_));
  prefs_observer_.Add(ExtensionPrefs::Get(profile_));
}

ExtensionSyncService::~ExtensionSyncService() {
}

// static
ExtensionSyncService* ExtensionSyncService::Get(
    content::BrowserContext* context) {
  return ExtensionSyncServiceFactory::GetForBrowserContext(context);
}

void ExtensionSyncService::SyncExtensionChangeIfNeeded(
    const Extension& extension) {
  if (!extensions::util::ShouldSync(&extension, profile_))
    return;

  syncer::ModelType type =
      extension.is_app() ? syncer::APPS : syncer::EXTENSIONS;
  SyncBundle* bundle = GetSyncBundle(type);
  if (bundle->IsSyncing()) {
    bundle->PushSyncAddOrUpdate(extension.id(),
                                CreateSyncData(extension).GetSyncData());
    DCHECK(!ExtensionPrefs::Get(profile_)->NeedsSync(extension.id()));
  } else {
    ExtensionPrefs::Get(profile_)->SetNeedsSync(extension.id(), true);
    if (extension_service()->is_ready() && !flare_.is_null())
      flare_.Run(type);  // Tell sync to start ASAP.
  }
}

bool ExtensionSyncService::HasPendingReenable(
    const std::string& id,
    const base::Version& version) const {
  auto it = pending_updates_.find(id);
  if (it == pending_updates_.end())
    return false;
  const PendingUpdate& pending = it->second;
  return pending.version.Equals(version) &&
         pending.grant_permissions_and_reenable;
}

syncer::SyncMergeResult ExtensionSyncService::MergeDataAndStartSyncing(
    syncer::ModelType type,
    const syncer::SyncDataList& initial_sync_data,
    scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
    scoped_ptr<syncer::SyncErrorFactory> sync_error_factory) {
  CHECK(sync_processor.get());
  LOG_IF(FATAL, type != syncer::EXTENSIONS && type != syncer::APPS)
      << "Got " << type << " ModelType";

  SyncBundle* bundle = GetSyncBundle(type);
  bundle->StartSyncing(sync_processor.Pass());

  // Apply the initial sync data, filtering out any items where we have more
  // recent local changes. Also tell the SyncBundle the extension IDs.
  for (const syncer::SyncData& sync_data : initial_sync_data) {
    scoped_ptr<ExtensionSyncData> extension_sync_data(
        ExtensionSyncData::CreateFromSyncData(sync_data));
    // If the extension has local state that needs to be synced, ignore this
    // change (we assume the local state is more recent).
    if (extension_sync_data &&
        !ExtensionPrefs::Get(profile_)->NeedsSync(extension_sync_data->id())) {
      ApplySyncData(*extension_sync_data);
    }
  }

  // Now push those local changes to sync.
  // TODO(treib,kalman): We should only have to send out changes for extensions
  // which have NeedsSync set (i.e. |GetLocalSyncDataList(type, false)|). That
  // makes some sync_integration_tests fail though - figure out why and fix it!
  std::vector<ExtensionSyncData> data_list = GetLocalSyncDataList(type, true);
  bundle->PushSyncDataList(ToSyncerSyncDataList(data_list));
  for (const ExtensionSyncData& data : data_list)
    ExtensionPrefs::Get(profile_)->SetNeedsSync(data.id(), false);

  if (type == syncer::APPS)
    ExtensionSystem::Get(profile_)->app_sorting()->FixNTPOrdinalCollisions();

  return syncer::SyncMergeResult(type);
}

void ExtensionSyncService::StopSyncing(syncer::ModelType type) {
  GetSyncBundle(type)->Reset();
}

syncer::SyncDataList ExtensionSyncService::GetAllSyncData(
    syncer::ModelType type) const {
  const SyncBundle* bundle = GetSyncBundle(type);
  if (!bundle->IsSyncing())
    return syncer::SyncDataList();

  std::vector<ExtensionSyncData> sync_data_list =
      GetLocalSyncDataList(type, true);

  // Add pending data (where the local extension is not installed yet).
  std::vector<ExtensionSyncData> pending_extensions =
      bundle->GetPendingExtensionData();
  sync_data_list.insert(sync_data_list.begin(),
                        pending_extensions.begin(),
                        pending_extensions.end());

  return ToSyncerSyncDataList(sync_data_list);
}

syncer::SyncError ExtensionSyncService::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  for (const syncer::SyncChange& sync_change : change_list) {
    scoped_ptr<ExtensionSyncData> extension_sync_data(
        ExtensionSyncData::CreateFromSyncChange(sync_change));
    if (extension_sync_data)
      ApplySyncData(*extension_sync_data);
  }

  ExtensionSystem::Get(profile_)->app_sorting()->FixNTPOrdinalCollisions();

  return syncer::SyncError();
}

ExtensionSyncData ExtensionSyncService::CreateSyncData(
    const Extension& extension) const {
  const ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(profile_);
  // Query the enabled state from ExtensionPrefs rather than
  // ExtensionService::IsExtensionEnabled - the latter uses ExtensionRegistry
  // which might not have been updated yet (ExtensionPrefs are updated first,
  // and we're listening for changes to these).
  bool enabled = !extension_prefs->IsExtensionDisabled(extension.id());
  enabled = enabled &&
      !extension_prefs->IsExternalExtensionUninstalled(extension.id());
  // Blacklisted extensions are not marked as disabled in ExtensionPrefs.
  enabled = enabled &&
      extension_prefs->GetExtensionBlacklistState(extension.id()) ==
          extensions::NOT_BLACKLISTED;
  int disable_reasons = extension_prefs->GetDisableReasons(extension.id());
  bool incognito_enabled = extensions::util::IsIncognitoEnabled(extension.id(),
                                                                profile_);
  bool remote_install =
      extension_prefs->HasDisableReason(extension.id(),
                                        Extension::DISABLE_REMOTE_INSTALL);
  ExtensionSyncData::OptionalBoolean allowed_on_all_url =
      GetAllowedOnAllUrlsOptionalBoolean(extension.id(), profile_);
  AppSorting* app_sorting = ExtensionSystem::Get(profile_)->app_sorting();

  ExtensionSyncData result = extension.is_app()
      ? ExtensionSyncData(
            extension, enabled, disable_reasons, incognito_enabled,
            remote_install, allowed_on_all_url,
            app_sorting->GetAppLaunchOrdinal(extension.id()),
            app_sorting->GetPageOrdinal(extension.id()),
            extensions::GetLaunchTypePrefValue(extension_prefs,
                                               extension.id()))
      : ExtensionSyncData(
            extension, enabled, disable_reasons, incognito_enabled,
            remote_install, allowed_on_all_url);

  // If there's a pending update, send the new version to sync instead of the
  // installed one.
  auto it = pending_updates_.find(extension.id());
  if (it != pending_updates_.end()) {
    const base::Version& version = it->second.version;
    // If we have a pending version, it should be newer than the installed one.
    DCHECK_EQ(-1, extension.version()->CompareTo(version));
    result.set_version(version);
    // If we'll re-enable the extension once it's updated, also send that back
    // to sync.
    if (it->second.grant_permissions_and_reenable)
      result.set_enabled(true);
  }
  return result;
}

void ExtensionSyncService::ApplySyncData(
    const ExtensionSyncData& extension_sync_data) {
  syncer::ModelType type = extension_sync_data.is_app() ? syncer::APPS
                                                        : syncer::EXTENSIONS;
  const std::string& id = extension_sync_data.id();
  // Note: |extension| may be null if it hasn't been installed yet.
  const Extension* extension =
      ExtensionRegistry::Get(profile_)->GetInstalledExtension(id);
  // TODO(bolms): we should really handle this better.  The particularly bad
  // case is where an app becomes an extension or vice versa, and we end up with
  // a zombie extension that won't go away.
  // TODO(treib): Is this still true?
  if (extension && !IsCorrectSyncType(*extension, type))
    return;

  SyncBundle* bundle = GetSyncBundle(type);
  // Forward to the bundle. This will just update the list of synced extensions.
  bundle->ApplySyncData(extension_sync_data);

  // Handle uninstalls first.
  if (extension_sync_data.uninstalled()) {
    if (!ExtensionService::UninstallExtensionHelper(
            extension_service(), id, extensions::UNINSTALL_REASON_SYNC)) {
      LOG(WARNING) << "Could not uninstall extension " << id << " for sync";
    }
    return;
  }

  // Extension from sync was uninstalled by the user as an external extension.
  // Honor user choice and skip installation/enabling.
  // TODO(treib): Should we still apply pref changes?
  ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(profile_);
  if (extension_prefs->IsExternalExtensionUninstalled(id)) {
    LOG(WARNING) << "Extension with id " << id
                 << " from sync was uninstalled as external extension";
    return;
  }

  int version_compare_result = extension ?
      extension->version()->CompareTo(extension_sync_data.version()) : 0;

  // Enable/disable the extension.
  bool reenable_after_update = false;
  if (extension_sync_data.enabled()) {
    DCHECK(!extension_sync_data.disable_reasons());

    if (extension) {
      // Only grant permissions if the sync data explicitly sets the disable
      // reasons to Extension::DISABLE_NONE (as opposed to the legacy (<M45)
      // case where they're not set at all), and if the version from sync
      // matches our local one.
      bool grant_permissions = extension_sync_data.supports_disable_reasons() &&
                               (version_compare_result == 0);
      if (grant_permissions) {
        extension_service()->GrantPermissionsAndEnableExtension(extension);
      } else if (!extension_service()->IsExtensionEnabled(extension->id())) {
        // Only enable if the extension already has all required permissions.
        scoped_ptr<const extensions::PermissionSet> granted_permissions =
            extension_prefs->GetGrantedPermissions(extension->id());
        DCHECK(granted_permissions.get());
        bool is_privilege_increase =
            extensions::PermissionMessageProvider::Get()->IsPrivilegeIncrease(
                *granted_permissions,
                extension->permissions_data()->active_permissions(),
                extension->GetType());
        if (!is_privilege_increase)
          extension_service()->EnableExtension(id);
        else if (extension_sync_data.supports_disable_reasons())
          reenable_after_update = true;

#if defined(ENABLE_SUPERVISED_USERS)
        if (is_privilege_increase && version_compare_result > 0 &&
            extensions::util::IsExtensionSupervised(extension, profile_)) {
          SupervisedUserServiceFactory::GetForProfile(profile_)
              ->AddExtensionUpdateRequest(id, *extension->version());
        }
#endif
      }
    } else {
      // The extension is not installed yet. Set it to enabled; we'll check for
      // permission increase when it's actually installed.
      extension_service()->EnableExtension(id);
    }
  } else {
    int disable_reasons = extension_sync_data.disable_reasons();
    if (extension_sync_data.remote_install()) {
      if (!(disable_reasons & Extension::DISABLE_REMOTE_INSTALL)) {
        // Since disabled reasons are synced since M45, DISABLE_REMOTE_INSTALL
        // should be among them already.
        DCHECK(!extension_sync_data.supports_disable_reasons());
        disable_reasons |= Extension::DISABLE_REMOTE_INSTALL;
      }
    } else if (!extension_sync_data.supports_disable_reasons()) {
      // Legacy case (<M45), from before we synced disable reasons (see
      // crbug.com/484214).
      disable_reasons = Extension::DISABLE_UNKNOWN_FROM_SYNC;
    }

    // In the non-legacy case (>=M45), clear any existing disable reasons first.
    // Otherwise sync can't remove just some of them.
    if (extension_sync_data.supports_disable_reasons())
      extension_prefs->ClearDisableReasons(id);

    extension_service()->DisableExtension(id, disable_reasons);
  }

  // If the target extension has already been installed ephemerally, it can
  // be promoted to a regular installed extension and downloading from the Web
  // Store is not necessary.
  if (extension && extensions::util::IsEphemeralApp(id, profile_))
    extension_service()->PromoteEphemeralApp(extension, true);

  // Cache whether the extension was already installed because setting the
  // incognito flag invalidates the |extension| pointer (it reloads the
  // extension).
  bool extension_installed = (extension != nullptr);

  // Update the incognito flag.
  extensions::util::SetIsIncognitoEnabled(
      id, profile_, extension_sync_data.incognito_enabled());
  extension = nullptr;  // No longer safe to use.

  // Update the all urls flag.
  if (extension_sync_data.all_urls_enabled() !=
          ExtensionSyncData::BOOLEAN_UNSET) {
    bool allowed = extension_sync_data.all_urls_enabled() ==
        ExtensionSyncData::BOOLEAN_TRUE;
    extensions::util::SetAllowedScriptingOnAllUrls(id, profile_, allowed);
  }

  // Set app-specific data.
  if (extension_sync_data.is_app()) {
    if (extension_sync_data.app_launch_ordinal().IsValid() &&
        extension_sync_data.page_ordinal().IsValid()) {
      AppSorting* app_sorting = ExtensionSystem::Get(profile_)->app_sorting();
      app_sorting->SetAppLaunchOrdinal(
          id,
          extension_sync_data.app_launch_ordinal());
      app_sorting->SetPageOrdinal(id, extension_sync_data.page_ordinal());
    }

    // The corresponding validation of this value during ExtensionSyncData
    // population is in ExtensionSyncData::ToAppSpecifics.
    if (extension_sync_data.launch_type() >= extensions::LAUNCH_TYPE_FIRST &&
        extension_sync_data.launch_type() < extensions::NUM_LAUNCH_TYPES) {
      extensions::SetLaunchType(
          profile_, id, extension_sync_data.launch_type());
    }

    if (!extension_sync_data.bookmark_app_url().empty())
      ApplyBookmarkAppSyncData(extension_sync_data);
  }

  // Finally, trigger installation/update as required.
  bool check_for_updates = false;
  if (extension_installed) {
    // If the extension is installed but outdated, store the new version.
    if (version_compare_result < 0) {
      pending_updates_[id] = PendingUpdate(extension_sync_data.version(),
                                           reenable_after_update);
      check_for_updates = true;
    }
  } else {
    if (!extension_service()->pending_extension_manager()->AddFromSync(
            id,
            extension_sync_data.update_url(),
            extension_sync_data.version(),
            extensions::sync_helper::IsSyncable,
            extension_sync_data.remote_install(),
            extension_sync_data.installed_by_custodian())) {
      LOG(WARNING) << "Could not add pending extension for " << id;
      // This means that the extension is already pending installation, with a
      // non-INTERNAL location.  Add to pending_sync_data, even though it will
      // never be removed (we'll never install a syncable version of the
      // extension), so that GetAllSyncData() continues to send it.
    }
    // Track pending extensions so that we can return them in GetAllSyncData().
    bundle->AddPendingExtensionData(id, extension_sync_data);
    check_for_updates = true;
  }

  if (check_for_updates)
    extension_service()->CheckForUpdatesSoon();
}

void ExtensionSyncService::ApplyBookmarkAppSyncData(
    const ExtensionSyncData& extension_sync_data) {
  DCHECK(extension_sync_data.is_app());

  // Process bookmark app sync if necessary.
  GURL bookmark_app_url(extension_sync_data.bookmark_app_url());
  if (!bookmark_app_url.is_valid() ||
      extension_sync_data.uninstalled()) {
    return;
  }

  const Extension* extension =
      extension_service()->GetInstalledExtension(extension_sync_data.id());

  // Return if there are no bookmark app details that need updating.
  if (extension &&
      extension->non_localized_name() == extension_sync_data.name() &&
      extension->description() ==
          extension_sync_data.bookmark_app_description()) {
    return;
  }

  WebApplicationInfo web_app_info;
  web_app_info.app_url = bookmark_app_url;
  web_app_info.title = base::UTF8ToUTF16(extension_sync_data.name());
  web_app_info.description =
      base::UTF8ToUTF16(extension_sync_data.bookmark_app_description());
  if (!extension_sync_data.bookmark_app_icon_color().empty()) {
    extensions::image_util::ParseCSSColorString(
        extension_sync_data.bookmark_app_icon_color(),
        &web_app_info.generated_icon_color);
  }
  for (const auto& icon : extension_sync_data.linked_icons()) {
    WebApplicationInfo::IconInfo icon_info;
    icon_info.url = icon.url;
    icon_info.width = icon.size;
    icon_info.height = icon.size;
    web_app_info.icons.push_back(icon_info);
  }

  // If the bookmark app already exists, keep the old icons.
  if (!extension) {
    CreateOrUpdateBookmarkApp(extension_service(), &web_app_info);
  } else {
    GetWebApplicationInfoFromApp(profile_,
                                 extension,
                                 base::Bind(&OnWebApplicationInfoLoaded,
                                            web_app_info,
                                            extension_service()->AsWeakPtr()));
  }
}

void ExtensionSyncService::SetSyncStartFlareForTesting(
    const syncer::SyncableService::StartSyncFlare& flare) {
  flare_ = flare;
}

ExtensionService* ExtensionSyncService::extension_service() const {
  return ExtensionSystem::Get(profile_)->extension_service();
}

void ExtensionSyncService::OnExtensionInstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    bool is_update) {
  DCHECK_EQ(profile_, browser_context);
  // Clear pending version if the installed one has caught up.
  auto it = pending_updates_.find(extension->id());
  if (it != pending_updates_.end()) {
    int compare_result = extension->version()->CompareTo(it->second.version);
    if (compare_result == 0 && it->second.grant_permissions_and_reenable)
      extension_service()->GrantPermissionsAndEnableExtension(extension);
    if (compare_result >= 0)
      pending_updates_.erase(it);
  }
  SyncExtensionChangeIfNeeded(*extension);
}

void ExtensionSyncService::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    extensions::UninstallReason reason) {
  DCHECK_EQ(profile_, browser_context);
  // Don't bother syncing if the extension will be re-installed momentarily.
  if (reason == extensions::UNINSTALL_REASON_REINSTALL ||
      !extensions::util::ShouldSync(extension, profile_))
    return;

  // TODO(tim): If we get here and IsSyncing is false, this will cause
  // "back from the dead" style bugs, because sync will add-back the extension
  // that was uninstalled here when MergeDataAndStartSyncing is called.
  // See crbug.com/256795.
  // Possible fix: Set NeedsSync here, then in MergeDataAndStartSyncing, if
  // NeedsSync is set but the extension isn't installed, send a sync deletion.
  syncer::ModelType type =
      extension->is_app() ? syncer::APPS : syncer::EXTENSIONS;
  SyncBundle* bundle = GetSyncBundle(type);
  if (bundle->IsSyncing()) {
    bundle->PushSyncDeletion(extension->id(),
                             CreateSyncData(*extension).GetSyncData());
  } else if (extension_service()->is_ready() && !flare_.is_null()) {
    flare_.Run(type);  // Tell sync to start ASAP.
  }

  pending_updates_.erase(extension->id());
}

void ExtensionSyncService::OnExtensionStateChanged(
    const std::string& extension_id,
    bool state) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile_);
  const Extension* extension = registry->GetInstalledExtension(extension_id);
  // We can get pref change notifications for extensions that aren't installed
  // (yet). In that case, we'll pick up the change later via ExtensionRegistry
  // observation (in OnExtensionInstalled).
  if (extension)
    SyncExtensionChangeIfNeeded(*extension);
}

void ExtensionSyncService::OnExtensionDisableReasonsChanged(
    const std::string& extension_id,
    int disabled_reasons) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile_);
  const Extension* extension = registry->GetInstalledExtension(extension_id);
  // We can get pref change notifications for extensions that aren't installed
  // (yet). In that case, we'll pick up the change later via ExtensionRegistry
  // observation (in OnExtensionInstalled).
  if (extension)
    SyncExtensionChangeIfNeeded(*extension);
}

SyncBundle* ExtensionSyncService::GetSyncBundle(syncer::ModelType type) {
  return const_cast<SyncBundle*>(
      const_cast<const ExtensionSyncService&>(*this).GetSyncBundle(type));
}

const SyncBundle* ExtensionSyncService::GetSyncBundle(
    syncer::ModelType type) const {
  return (type == syncer::APPS) ? &app_sync_bundle_ : &extension_sync_bundle_;
}

std::vector<ExtensionSyncData> ExtensionSyncService::GetLocalSyncDataList(
    syncer::ModelType type,
    bool include_everything) const {
  // Collect the local state.
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile_);
  std::vector<ExtensionSyncData> data;
  // TODO(treib, kalman): Should we be including blacklisted/blocked extensions
  // here? I.e. just calling registry->GeneratedInstalledExtensionsSet()?
  // It would be more consistent, but the danger is that the black/blocklist
  // hasn't been updated on all clients by the time sync has kicked in -
  // so it's safest not to. Take care to add any other extension lists here
  // in the future if they are added.
  FillSyncDataList(
      registry->enabled_extensions(), type, include_everything, &data);
  FillSyncDataList(
      registry->disabled_extensions(), type, include_everything, &data);
  FillSyncDataList(
      registry->terminated_extensions(), type, include_everything, &data);
  return data;
}

void ExtensionSyncService::FillSyncDataList(
    const ExtensionSet& extensions,
    syncer::ModelType type,
    bool include_everything,
    std::vector<ExtensionSyncData>* sync_data_list) const {
  for (const scoped_refptr<const Extension>& extension : extensions) {
    if (IsCorrectSyncType(*extension, type) &&
        extensions::util::ShouldSync(extension.get(), profile_) &&
        (include_everything ||
         ExtensionPrefs::Get(profile_)->NeedsSync(extension->id()))) {
      // We should never have pending data for an installed extension.
      DCHECK(!GetSyncBundle(type)->HasPendingExtensionData(extension->id()));
      sync_data_list->push_back(CreateSyncData(*extension));
    }
  }
}
