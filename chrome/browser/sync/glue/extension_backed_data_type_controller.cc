// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/extension_backed_data_type_controller.h"

#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/chrome_report_unrecoverable_error.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_factory.h"

using content::BrowserThread;

namespace browser_sync {

namespace {

// Helper method to generate a hash from an extension id.
std::string IdToHash(const std::string extension_id) {
  std::string hash = base::SHA1HashString(extension_id);
  hash = base::HexEncode(hash.c_str(), hash.length());
  return hash;
}

}  // namespace

ExtensionBackedDataTypeController::ExtensionBackedDataTypeController(
    syncer::ModelType type,
    const std::string& extension_hash,
    sync_driver::SyncApiComponentFactory* sync_factory,
    Profile* profile)
    : UIDataTypeController(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
          base::Bind(&ChromeReportUnrecoverableError),
          type,
          sync_factory),
      extension_hash_(extension_hash),
      profile_(profile) {
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistryFactory::GetForBrowserContext(profile_);
  registry->AddObserver(this);
}

ExtensionBackedDataTypeController::~ExtensionBackedDataTypeController() {
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistryFactory::GetForBrowserContext(profile_);
  registry->RemoveObserver(this);
}

bool ExtensionBackedDataTypeController::ReadyForStart() const {
  // TODO(zea): consider checking if the extension was uninstalled without
  // sync noticing, in which case the datatype should be purged.
  return IsSyncingExtensionEnabled();
}

void ExtensionBackedDataTypeController::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension) {
  if (DoesExtensionMatch(*extension)) {
    ProfileSyncService* sync_service =
        ProfileSyncServiceFactory::GetForProfile(profile_);
    sync_service->ReenableDatatype(type());
  }
}

void ExtensionBackedDataTypeController::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionInfo::Reason reason) {
  if (DoesExtensionMatch(*extension)) {
    // This will trigger purging the datatype from the sync directory. This
    // may not always be the right choice, especially in the face of transient
    // unloads (e.g. for permission changes). If that becomes a large enough
    // issue, we should consider using the extension unload reason to just
    // trigger a reconfiguration without disabling (type will be unready).
    syncer::SyncError error(FROM_HERE,
                            syncer::SyncError::DATATYPE_POLICY_ERROR,
                            "Extension unloaded",
                            type());
    OnSingleDataTypeUnrecoverableError(error);
  }
}

bool ExtensionBackedDataTypeController::IsSyncingExtensionEnabled() const {
  if (extension_hash_.empty())
    return true;  // For use while extension is in development.

  // TODO(synced notifications): rather than rely on a hash of the extension
  // id, look through the manifests to see if the extension has permissions
  // for this model type.
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistryFactory::GetForBrowserContext(profile_);
  const extensions::ExtensionSet& enabled_extensions(
      registry->enabled_extensions());
  for (extensions::ExtensionSet::const_iterator iter =
           enabled_extensions.begin();
       iter != enabled_extensions.end(); ++iter) {
    if (DoesExtensionMatch(*iter->get()))
      return true;
  }
  return false;
}

bool ExtensionBackedDataTypeController::DoesExtensionMatch(
    const extensions::Extension& extension) const {
  return IdToHash(extension.id()) == extension_hash_;
}

bool ExtensionBackedDataTypeController::StartModels() {
  if (IsSyncingExtensionEnabled())
    return true;

  // If the extension is not currently enabled, it means that it has been
  // disabled since the call to ReadyForStart(), and the notification
  // observer should have already posted a call to disable the type.
  return false;
}

}  // namespace browser_sync
