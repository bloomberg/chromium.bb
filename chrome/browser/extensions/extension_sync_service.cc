// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_sync_service.h"

#include "base/basictypes.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
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
#include "components/sync_driver/sync_prefs.h"
#include "extensions/browser/app_sorting.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/image_util.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_error_factory.h"

using extensions::Extension;
using extensions::ExtensionPrefs;
using extensions::ExtensionRegistry;
using extensions::ExtensionSet;
using extensions::ExtensionSyncData;
using extensions::PendingEnables;
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
  if (type == syncer::EXTENSIONS &&
      extensions::sync_helper::IsSyncableExtension(&extension)) {
    return true;
  }

  if (type == syncer::APPS &&
      extensions::sync_helper::IsSyncableApp(&extension)) {
    return true;
  }

  return false;
}

// Returns whether the given app or extension should be synced.
bool ShouldSync(const Extension& extension, Profile* profile) {
  if (extension.is_app())
    return extensions::util::ShouldSyncApp(&extension, profile);
  return extensions::util::ShouldSyncExtension(&extension, profile);
}

}  // namespace

ExtensionSyncService::ExtensionSyncService(Profile* profile,
                                           ExtensionPrefs* extension_prefs,
                                           ExtensionService* extension_service)
    : profile_(profile),
      extension_prefs_(extension_prefs),
      extension_service_(extension_service),
      app_sync_bundle_(this),
      extension_sync_bundle_(this),
      pending_app_enables_(make_scoped_ptr(new sync_driver::SyncPrefs(
                               extension_prefs_->pref_service())),
                           &app_sync_bundle_,
                           syncer::APPS),
      pending_extension_enables_(make_scoped_ptr(new sync_driver::SyncPrefs(
                                     extension_prefs_->pref_service())),
                                 &extension_sync_bundle_,
                                 syncer::EXTENSIONS) {
  SetSyncStartFlare(sync_start_util::GetFlareForSyncableService(
      profile_->GetPath()));

  extension_service_->set_extension_sync_service(this);
  extension_prefs_->app_sorting()->SetExtensionSyncService(this);
}

ExtensionSyncService::~ExtensionSyncService() {}

// static
ExtensionSyncService* ExtensionSyncService::Get(
    content::BrowserContext* context) {
  return ExtensionSyncServiceFactory::GetForBrowserContext(context);
}

syncer::SyncData ExtensionSyncService::PrepareToSyncUninstallExtension(
    const Extension& extension) {
  // Extract the data we need for sync now, but don't actually sync until we've
  // completed the uninstallation.
  // TODO(tim): If we get here and IsSyncing is false, this will cause
  // "back from the dead" style bugs, because sync will add-back the extension
  // that was uninstalled here when MergeDataAndStartSyncing is called.
  // See crbug.com/256795.
  syncer::ModelType type =
      extension.is_app() ? syncer::APPS : syncer::EXTENSIONS;
  const SyncBundle* bundle = GetSyncBundle(type);
  if (ShouldSync(extension, profile_)) {
    if (bundle->IsSyncing())
      return CreateSyncData(extension).GetSyncData();
    if (extension_service_->is_ready() && !flare_.is_null())
      flare_.Run(type);  // Tell sync to start ASAP.
  }

  return syncer::SyncData();
}

void ExtensionSyncService::ProcessSyncUninstallExtension(
    const std::string& extension_id,
    const syncer::SyncData& sync_data) {
  SyncBundle* bundle = GetSyncBundle(sync_data.GetDataType());
  if (bundle->HasExtensionId(extension_id))
    bundle->PushSyncDeletion(extension_id, sync_data);
}

void ExtensionSyncService::SyncEnableExtension(const Extension& extension) {
  // Syncing may not have started yet, so handle pending enables.
  if (extensions::util::ShouldSyncApp(&extension, profile_))
    pending_app_enables_.Add(extension.id());

  if (extensions::util::ShouldSyncExtension(&extension, profile_))
    pending_extension_enables_.Add(extension.id());

  SyncExtensionChangeIfNeeded(extension);
}

void ExtensionSyncService::SyncDisableExtension(const Extension& extension) {
  // Syncing may not have started yet, so handle pending enables.
  if (extensions::util::ShouldSyncApp(&extension, profile_))
    pending_app_enables_.Remove(extension.id());

  if (extensions::util::ShouldSyncExtension(&extension, profile_))
    pending_extension_enables_.Remove(extension.id());

  SyncExtensionChangeIfNeeded(extension);
}

void ExtensionSyncService::SyncExtensionChangeIfNeeded(
    const Extension& extension) {
  if (!ShouldSync(extension, profile_))
    return;

  syncer::ModelType type =
      extension.is_app() ? syncer::APPS : syncer::EXTENSIONS;
  SyncBundle* bundle = GetSyncBundle(type);
  if (bundle->IsSyncing())
    bundle->PushSyncChangeIfNeeded(extension);
  else if (extension_service_->is_ready() && !flare_.is_null())
    flare_.Run(type);
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
  bool is_apps = (type == syncer::APPS);
  PendingEnables* pending_enables =
      is_apps ? &pending_app_enables_ : &pending_extension_enables_;

  bundle->MergeDataAndStartSyncing(initial_sync_data, sync_processor.Pass());
  pending_enables->OnSyncStarted(extension_service_);

  // Process local extensions.
  // TODO(yoz): Determine whether pending extensions should be considered too.
  //            See crbug.com/104399.
  bundle->PushSyncDataList(GetAllSyncData(type));

  if (is_apps)
    extension_prefs_->app_sorting()->FixNTPOrdinalCollisions();

  return syncer::SyncMergeResult(type);
}

void ExtensionSyncService::StopSyncing(syncer::ModelType type) {
  GetSyncBundle(type)->Reset();
}

syncer::SyncDataList ExtensionSyncService::GetAllSyncData(
    syncer::ModelType type) const {
  std::vector<ExtensionSyncData> data = GetSyncDataList(type);
  syncer::SyncDataList result;
  result.reserve(data.size());
  for (const ExtensionSyncData& item : data)
    result.push_back(item.GetSyncData());
  return result;
}

syncer::SyncError ExtensionSyncService::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  for (const syncer::SyncChange& sync_change : change_list) {
    syncer::ModelType type = sync_change.sync_data().GetDataType();
    GetSyncBundle(type)->ApplySyncChange(sync_change);
  }

  extension_prefs_->app_sorting()->FixNTPOrdinalCollisions();

  return syncer::SyncError();
}

ExtensionSyncData ExtensionSyncService::CreateSyncData(
    const extensions::Extension& extension) const {
  bool enabled = extension_service_->IsExtensionEnabled(extension.id());
  int disable_reasons = extension_prefs_->GetDisableReasons(extension.id());
  bool incognito_enabled = extensions::util::IsIncognitoEnabled(extension.id(),
                                                                profile_);
  bool remote_install =
      extension_prefs_->HasDisableReason(extension.id(),
                                         Extension::DISABLE_REMOTE_INSTALL);
  ExtensionSyncData::OptionalBoolean allowed_on_all_url =
      GetAllowedOnAllUrlsOptionalBoolean(extension.id(), profile_);
  if (extension.is_app()) {
    return ExtensionSyncData(
        extension, enabled, disable_reasons, incognito_enabled, remote_install,
        allowed_on_all_url,
        extension_prefs_->app_sorting()->GetAppLaunchOrdinal(extension.id()),
        extension_prefs_->app_sorting()->GetPageOrdinal(extension.id()),
        extensions::GetLaunchTypePrefValue(extension_prefs_, extension.id()));
  }
  return ExtensionSyncData(
      extension, enabled, disable_reasons, incognito_enabled, remote_install,
      allowed_on_all_url);
}

bool ExtensionSyncService::ApplySyncData(
    const ExtensionSyncData& extension_sync_data) {
  const std::string& id = extension_sync_data.id();

  if (extension_sync_data.is_app()) {
    if (extension_sync_data.app_launch_ordinal().IsValid() &&
        extension_sync_data.page_ordinal().IsValid()) {
      extension_prefs_->app_sorting()->SetAppLaunchOrdinal(
          id,
          extension_sync_data.app_launch_ordinal());
      extension_prefs_->app_sorting()->SetPageOrdinal(
          id,
          extension_sync_data.page_ordinal());
    }

    // The corresponding validation of this value during AppSyncData population
    // is in AppSyncData::PopulateAppSpecifics.
    if (extension_sync_data.launch_type() >= extensions::LAUNCH_TYPE_FIRST &&
        extension_sync_data.launch_type() < extensions::NUM_LAUNCH_TYPES) {
      extensions::SetLaunchType(
          profile_, id, extension_sync_data.launch_type());
    }

    if (!extension_sync_data.bookmark_app_url().empty())
      ApplyBookmarkAppSyncData(extension_sync_data);
  }

  syncer::ModelType type = extension_sync_data.is_app() ? syncer::APPS
                                                        : syncer::EXTENSIONS;

  if (!ApplyExtensionSyncDataHelper(extension_sync_data, type)) {
    GetSyncBundle(type)->AddPendingExtension(id, extension_sync_data);
    extension_service_->CheckForUpdatesSoon();
    return false;
  }

  return true;
}

void ExtensionSyncService::ApplyBookmarkAppSyncData(
    const extensions::ExtensionSyncData& extension_sync_data) {
  DCHECK(extension_sync_data.is_app());

  // Process bookmark app sync if necessary.
  GURL bookmark_app_url(extension_sync_data.bookmark_app_url());
  if (!bookmark_app_url.is_valid() ||
      extension_sync_data.uninstalled()) {
    return;
  }

  const Extension* extension =
      extension_service_->GetInstalledExtension(extension_sync_data.id());

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
    CreateOrUpdateBookmarkApp(extension_service_, &web_app_info);
  } else {
    GetWebApplicationInfoFromApp(profile_,
                                 extension,
                                 base::Bind(&OnWebApplicationInfoLoaded,
                                            web_app_info,
                                            extension_service_->AsWeakPtr()));
  }
}

void ExtensionSyncService::SyncOrderingChange(const std::string& extension_id) {
  const Extension* ext =
      extension_service_->GetInstalledExtension(extension_id);

  if (ext)
    SyncExtensionChangeIfNeeded(*ext);
}

void ExtensionSyncService::SetSyncStartFlare(
    const syncer::SyncableService::StartSyncFlare& flare) {
  flare_ = flare;
}

bool ExtensionSyncService::IsPendingEnable(
    const std::string& extension_id) const {
  return pending_app_enables_.Contains(extension_id) ||
      pending_extension_enables_.Contains(extension_id);
}

SyncBundle* ExtensionSyncService::GetSyncBundle(syncer::ModelType type) {
  return const_cast<SyncBundle*>(
      const_cast<const ExtensionSyncService&>(*this).GetSyncBundle(type));
}

const SyncBundle* ExtensionSyncService::GetSyncBundle(
    syncer::ModelType type) const {
  if (type == syncer::APPS)
    return &app_sync_bundle_;
  return &extension_sync_bundle_;
}

std::vector<ExtensionSyncData> ExtensionSyncService::GetSyncDataList(
    syncer::ModelType type) const {
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile_);
  std::vector<ExtensionSyncData> extension_sync_list;
  FillSyncDataList(registry->enabled_extensions(), type, &extension_sync_list);
  FillSyncDataList(registry->disabled_extensions(), type, &extension_sync_list);
  FillSyncDataList(
      registry->terminated_extensions(), type, &extension_sync_list);

  std::vector<ExtensionSyncData> pending_extensions =
      GetSyncBundle(type)->GetPendingData();
  extension_sync_list.insert(extension_sync_list.begin(),
                             pending_extensions.begin(),
                             pending_extensions.end());

  return extension_sync_list;
}

void ExtensionSyncService::FillSyncDataList(
    const ExtensionSet& extensions,
    syncer::ModelType type,
    std::vector<ExtensionSyncData>* sync_data_list) const {
  const SyncBundle* bundle = GetSyncBundle(type);
  for (const scoped_refptr<const Extension>& extension : extensions) {
    if (IsCorrectSyncType(*extension, type) &&
        ShouldSync(*extension, profile_) &&
        bundle->ShouldIncludeInLocalSyncDataList(*extension)) {
      sync_data_list->push_back(CreateSyncData(*extension));
    }
  }
}

bool ExtensionSyncService::ApplyExtensionSyncDataHelper(
    const ExtensionSyncData& extension_sync_data,
    syncer::ModelType type) {
  const std::string& id = extension_sync_data.id();
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile_);
  const Extension* extension = registry->GetInstalledExtension(id);

  // TODO(bolms): we should really handle this better.  The particularly bad
  // case is where an app becomes an extension or vice versa, and we end up with
  // a zombie extension that won't go away.
  if (extension && !IsCorrectSyncType(*extension, type))
    return true;

  // Handle uninstalls first.
  if (extension_sync_data.uninstalled()) {
    if (!extension_service_->UninstallExtensionHelper(
            extension_service_, id, extensions::UNINSTALL_REASON_SYNC)) {
      LOG(WARNING) << "Could not uninstall extension " << id << " for sync";
    }
    return true;
  }

  // Extension from sync was uninstalled by the user as external extensions.
  // Honor user choice and skip installation/enabling.
  if (ExtensionPrefs::Get(profile_)->IsExternalExtensionUninstalled(id)) {
    LOG(WARNING) << "Extension with id " << id
                 << " from sync was uninstalled as external extension";
    return true;
  }

  int version_compare_result = extension ?
      extension->version()->CompareTo(extension_sync_data.version()) : 0;

  // Set user settings.
  if (extension_sync_data.enabled()) {
    DCHECK(!extension_sync_data.disable_reasons());

    // Only grant permissions if the sync data explicitly sets the disable
    // reasons to Extension::DISABLE_NONE (as opposed to the legacy (<M45) case
    // where they're not set at all), and if the version from sync matches our
    // local one. Otherwise we just enable it without granting permissions. If
    // any permissions are missing, CheckPermissionsIncrease will soon disable
    // it again.
    bool grant_permissions =
        extension_sync_data.supports_disable_reasons() &&
        extension && (version_compare_result == 0);
    if (grant_permissions)
      extension_service_->GrantPermissionsAndEnableExtension(extension);
    else
      extension_service_->EnableExtension(id);
  } else if (!IsPendingEnable(id)) {
    int disable_reasons = extension_sync_data.disable_reasons();
    if (extension_sync_data.remote_install()) {
      if (!(disable_reasons & Extension::DISABLE_REMOTE_INSTALL)) {
        // In the non-legacy case (>=M45) where disable reasons are synced at
        // all, DISABLE_REMOTE_INSTALL should be among them already.
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
      ExtensionPrefs::Get(profile_)->ClearDisableReasons(id);

    extension_service_->DisableExtension(id, disable_reasons);
  }

  // We need to cache some information here because setting the incognito flag
  // invalidates the |extension| pointer (it reloads the extension).
  bool extension_installed = (extension != NULL);

  // If the target extension has already been installed ephemerally, it can
  // be promoted to a regular installed extension and downloading from the Web
  // Store is not necessary.
  if (extension && extensions::util::IsEphemeralApp(id, profile_))
    extension_service_->PromoteEphemeralApp(extension, true);

  // Update the incognito flag.
  extensions::util::SetIsIncognitoEnabled(
      id, profile_, extension_sync_data.incognito_enabled());
  extension = NULL;  // No longer safe to use.

  // Update the all urls flag.
  if (extension_sync_data.all_urls_enabled() !=
          ExtensionSyncData::BOOLEAN_UNSET) {
    bool allowed = extension_sync_data.all_urls_enabled() ==
        ExtensionSyncData::BOOLEAN_TRUE;
    extensions::util::SetAllowedScriptingOnAllUrls(id, profile_, allowed);
  }

  if (extension_installed) {
    // If the extension is already installed, check if it's outdated.
    if (version_compare_result < 0) {
      // Extension is outdated.
      return false;
    }
  } else {
    CHECK(type == syncer::EXTENSIONS || type == syncer::APPS);
    extensions::PendingExtensionInfo::ShouldAllowInstallPredicate filter =
        (type == syncer::APPS) ? extensions::sync_helper::IsSyncableApp :
                                 extensions::sync_helper::IsSyncableExtension;

    if (!extension_service_->pending_extension_manager()->AddFromSync(
            id,
            extension_sync_data.update_url(),
            filter,
            extension_sync_data.remote_install(),
            extension_sync_data.installed_by_custodian())) {
      LOG(WARNING) << "Could not add pending extension for " << id;
      // This means that the extension is already pending installation, with a
      // non-INTERNAL location.  Add to pending_sync_data, even though it will
      // never be removed (we'll never install a syncable version of the
      // extension), so that GetAllSyncData() continues to send it.
    }
    // Track pending extensions so that we can return them in GetAllSyncData().
    return false;
  }

  return true;
}
