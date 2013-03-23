// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/installed_loader.h"

#include "base/files/file_path.h"
#include "base/metrics/histogram.h"
#include "base/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/runtime/runtime_api.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/management_policy.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "chrome/common/extensions/extension_l10n_util.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/manifest.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/user_metrics.h"

using content::BrowserThread;
using content::UserMetricsAction;
using extensions::Extension;
using extensions::ExtensionInfo;
using extensions::Manifest;

namespace errors = extension_manifest_errors;

namespace {

// The following enumeration is used in histograms matching
// Extensions.ManifestReload* .  Values may be added, as long as existing
// values are not changed.
enum ManifestReloadReason {
  NOT_NEEDED = 0,  // Reload not needed.
  UNPACKED_DIR,  // Unpacked directory.
  NEEDS_RELOCALIZATION,  // The locale has changed since we read this extension.
  NUM_MANIFEST_RELOAD_REASONS
};

ManifestReloadReason ShouldReloadExtensionManifest(const ExtensionInfo& info) {
  // Always reload manifests of unpacked extensions, because they can change
  // on disk independent of the manifest in our prefs.
  if (Manifest::IsUnpackedLocation(info.extension_location))
    return UNPACKED_DIR;

  // Reload the manifest if it needs to be relocalized.
  if (extension_l10n_util::ShouldRelocalizeManifest(
          info.extension_manifest.get()))
    return NEEDS_RELOCALIZATION;

  return NOT_NEEDED;
}

void DispatchOnInstalledEvent(
    Profile* profile,
    const std::string& extension_id,
    const Version& old_version,
    bool chrome_updated) {
  // profile manager can be NULL in unit tests.
  if (!g_browser_process->profile_manager())
    return;
  if (!g_browser_process->profile_manager()->IsValidProfile(profile))
    return;

  extensions::RuntimeEventRouter::DispatchOnInstalledEvent(
      profile, extension_id, old_version, chrome_updated);
}

}  // namespace

namespace extensions {

InstalledLoader::InstalledLoader(ExtensionService* extension_service)
    : extension_service_(extension_service),
      extension_prefs_(extension_service->extension_prefs()) {
}

InstalledLoader::~InstalledLoader() {
}

void InstalledLoader::Load(const ExtensionInfo& info, bool write_to_prefs) {
  std::string error;
  scoped_refptr<const Extension> extension(NULL);
  if (info.extension_manifest.get()) {
    extension = Extension::Create(
        info.extension_path,
        info.extension_location,
        *info.extension_manifest,
        GetCreationFlags(&info),
        &error);
  } else {
    error = errors::kManifestUnreadable;
  }

  // Once installed, non-unpacked extensions cannot change their IDs (e.g., by
  // updating the 'key' field in their manifest).
  // TODO(jstritar): migrate preferences when unpacked extensions change IDs.
  if (extension &&
      !Manifest::IsUnpackedLocation(extension->location()) &&
      info.extension_id != extension->id()) {
    error = errors::kCannotChangeExtensionID;
    extension = NULL;
    content::RecordAction(UserMetricsAction("Extensions.IDChangedError"));
  }

  // Check policy on every load in case an extension was blacklisted while
  // Chrome was not running.
  const ManagementPolicy* policy = extensions::ExtensionSystem::Get(
      extension_service_->profile())->management_policy();
  if (extension &&
      !policy->UserMayLoad(extension, NULL)) {
    // The error message from UserMayInstall() often contains the extension ID
    // and is therefore not well suited to this UI.
    error = errors::kDisabledByPolicy;
    extension = NULL;
  }

  if (!extension) {
    extension_service_->
        ReportExtensionLoadError(info.extension_path, error, false);
    return;
  }

  if (write_to_prefs)
    extension_prefs_->UpdateManifest(extension);

  extension_service_->AddExtension(extension);
}

void InstalledLoader::LoadAllExtensions() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  base::TimeTicks start_time = base::TimeTicks::Now();

  scoped_ptr<ExtensionPrefs::ExtensionsInfo> extensions_info(
      extension_prefs_->GetInstalledExtensionsInfo());

  std::vector<int> reload_reason_counts(NUM_MANIFEST_RELOAD_REASONS, 0);
  bool should_write_prefs = false;
  int update_count = 0;

  for (size_t i = 0; i < extensions_info->size(); ++i) {
    ExtensionInfo* info = extensions_info->at(i).get();

    // Skip extensions that were loaded from the command-line because we don't
    // want those to persist across browser restart.
    if (info->extension_location == Manifest::COMMAND_LINE)
      continue;

    scoped_ptr<ExtensionInfo> pending_update(
        extension_prefs_->GetDelayedInstallInfo(info->extension_id));
    if (pending_update) {
      if (!extension_prefs_->FinishDelayedInstallInfo(info->extension_id))
        NOTREACHED();

      Version old_version;
      if (info->extension_manifest) {
        std::string version_str;
        if (info->extension_manifest->GetString(
            extension_manifest_keys::kVersion, &version_str)) {
          old_version = Version(version_str);
        }
      }
      MessageLoop::current()->PostTask(FROM_HERE,
          base::Bind(&DispatchOnInstalledEvent, extension_service_->profile(),
                     info->extension_id, old_version, false));

      info = extension_prefs_->GetInstalledExtensionInfo(
          info->extension_id).release();
      extensions_info->at(i).reset(info);

      update_count++;
    }

    ManifestReloadReason reload_reason = ShouldReloadExtensionManifest(*info);
    ++reload_reason_counts[reload_reason];
    UMA_HISTOGRAM_ENUMERATION("Extensions.ManifestReloadEnumValue",
                              reload_reason, 100);

    if (reload_reason != NOT_NEEDED) {
      // Reloading an extension reads files from disk.  We do this on the
      // UI thread because reloads should be very rare, and the complexity
      // added by delaying the time when the extensions service knows about
      // all extensions is significant.  See crbug.com/37548 for details.
      // |allow_io| disables tests that file operations run on the file
      // thread.
      base::ThreadRestrictions::ScopedAllowIO allow_io;

      std::string error;
      scoped_refptr<const Extension> extension(
          extension_file_util::LoadExtension(
              info->extension_path,
              info->extension_location,
              GetCreationFlags(info),
              &error));

      if (!extension.get()) {
        extension_service_->
            ReportExtensionLoadError(info->extension_path, error, false);
        continue;
      }

      extensions_info->at(i)->extension_manifest.reset(
          static_cast<DictionaryValue*>(
              extension->manifest()->value()->DeepCopy()));
      should_write_prefs = true;
    }
  }

  for (size_t i = 0; i < extensions_info->size(); ++i) {
    if (extensions_info->at(i)->extension_location == Manifest::COMMAND_LINE)
      continue;
    Load(*extensions_info->at(i), should_write_prefs);
  }

  extension_service_->OnLoadedInstalledExtensions();

  // The histograms Extensions.ManifestReload* allow us to validate
  // the assumption that reloading manifest is a rare event.
  UMA_HISTOGRAM_COUNTS_100("Extensions.ManifestReloadNotNeeded",
                           reload_reason_counts[NOT_NEEDED]);
  UMA_HISTOGRAM_COUNTS_100("Extensions.ManifestReloadUnpackedDir",
                           reload_reason_counts[UNPACKED_DIR]);
  UMA_HISTOGRAM_COUNTS_100("Extensions.ManifestReloadNeedsRelocalization",
                           reload_reason_counts[NEEDS_RELOCALIZATION]);

  UMA_HISTOGRAM_COUNTS_100("Extensions.LoadAll",
                           extension_service_->extensions()->size());
  UMA_HISTOGRAM_COUNTS_100("Extensions.Disabled",
                           extension_service_->disabled_extensions()->size());
  UMA_HISTOGRAM_COUNTS_100("Extensions.UpdateOnLoad",
                           update_count);

  UMA_HISTOGRAM_TIMES("Extensions.LoadAllTime",
                      base::TimeTicks::Now() - start_time);

  int app_user_count = 0;
  int app_external_count = 0;
  int hosted_app_count = 0;
  int legacy_packaged_app_count = 0;
  int platform_app_count = 0;
  int user_script_count = 0;
  int content_pack_count = 0;
  int extension_user_count = 0;
  int extension_external_count = 0;
  int theme_count = 0;
  int page_action_count = 0;
  int browser_action_count = 0;
  int disabled_for_permissions_count = 0;
  int item_user_count = 0;
  const ExtensionSet* extensions = extension_service_->extensions();
  ExtensionSet::const_iterator ex;
  for (ex = extensions->begin(); ex != extensions->end(); ++ex) {
    Manifest::Location location = (*ex)->location();
    Manifest::Type type = (*ex)->GetType();
    if ((*ex)->is_app()) {
      UMA_HISTOGRAM_ENUMERATION("Extensions.AppLocation",
                                location, 100);
    } else if (type == Manifest::TYPE_EXTENSION) {
      UMA_HISTOGRAM_ENUMERATION("Extensions.ExtensionLocation",
                                location, 100);
    }

    // Don't count component extensions, since they are only extensions as an
    // implementation detail.
    if (location == Manifest::COMPONENT)
      continue;

    // Don't count unpacked extensions, since they're a developer-specific
    // feature.
    if (Manifest::IsUnpackedLocation(location))
      continue;

    // Using an enumeration shows us the total installed ratio across all users.
    // Using the totals per user at each startup tells us the distribution of
    // usage for each user (e.g. 40% of users have at least one app installed).
    UMA_HISTOGRAM_ENUMERATION("Extensions.LoadType", type, 100);
    switch (type) {
      case Manifest::TYPE_THEME:
        ++theme_count;
        break;
      case Manifest::TYPE_USER_SCRIPT:
        ++user_script_count;
        break;
      case Manifest::TYPE_HOSTED_APP:
        ++hosted_app_count;
        if (Manifest::IsExternalLocation(location)) {
          ++app_external_count;
        } else {
          ++app_user_count;
        }
        break;
      case Manifest::TYPE_LEGACY_PACKAGED_APP:
        ++legacy_packaged_app_count;
        if (Manifest::IsExternalLocation(location)) {
          ++app_external_count;
        } else {
          ++app_user_count;
        }
        break;
      case Manifest::TYPE_PLATFORM_APP:
        ++platform_app_count;
        if (Manifest::IsExternalLocation(location)) {
          ++app_external_count;
        } else {
          ++app_user_count;
        }
        break;
      case Manifest::TYPE_EXTENSION:
      default:
        if (Manifest::IsExternalLocation(location)) {
          ++extension_external_count;
        } else {
          ++extension_user_count;
        }
        break;
    }
    if (!Manifest::IsExternalLocation((*ex)->location()))
      ++item_user_count;
    ExtensionActionManager* extension_action_manager =
        ExtensionActionManager::Get(extension_service_->profile());
    if (extension_action_manager->GetPageAction(**ex))
      ++page_action_count;
    if (extension_action_manager->GetBrowserAction(**ex))
      ++browser_action_count;

    if ((*ex)->is_content_pack())
      ++content_pack_count;

    extension_service_->RecordPermissionMessagesHistogram(
        *ex, "Extensions.Permissions_Load");
  }
  const ExtensionSet* disabled_extensions =
      extension_service_->disabled_extensions();
  for (ex = disabled_extensions->begin();
       ex != disabled_extensions->end(); ++ex) {
    if (extension_service_->extension_prefs()->
        DidExtensionEscalatePermissions((*ex)->id())) {
      ++disabled_for_permissions_count;
    }
  }

  UMA_HISTOGRAM_COUNTS_100("Extensions.LoadAllUser", item_user_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.LoadApp",
                           app_user_count + app_external_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.LoadAppUser", app_user_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.LoadAppExternal", app_external_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.LoadHostedApp", hosted_app_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.LoadPackagedApp",
                           legacy_packaged_app_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.LoadPlatformApp", platform_app_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.LoadExtension",
                           extension_user_count + extension_external_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.LoadExtensionUser",
                           extension_user_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.LoadExtensionExternal",
                           extension_external_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.LoadUserScript", user_script_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.LoadTheme", theme_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.LoadPageAction", page_action_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.LoadBrowserAction",
                           browser_action_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.LoadContentPack", content_pack_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.DisabledForPermissions",
                           disabled_for_permissions_count);
}

int InstalledLoader::GetCreationFlags(const ExtensionInfo* info) {
  int flags = extension_prefs_->GetCreationFlags(info->extension_id);
  if (!Manifest::IsUnpackedLocation(info->extension_location))
    flags |= Extension::REQUIRE_KEY;
  if (extension_prefs_->AllowFileAccess(info->extension_id))
    flags |= Extension::ALLOW_FILE_ACCESS;
  return flags;
}

}  // namespace extensions
