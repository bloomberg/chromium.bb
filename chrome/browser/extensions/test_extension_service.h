// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_TEST_EXTENSION_SERVICE_H_
#define CHROME_BROWSER_EXTENSIONS_TEST_EXTENSION_SERVICE_H_

#include <string>
#include <vector>

#include "chrome/browser/extensions/extension_service.h"

namespace syncer {
class SyncErrorFactory;
}

namespace extensions {
class CrxInstaller;
class Extension;
}

// Implemention of ExtensionServiceInterface with default
// implementations for methods that add failures.  You should subclass
// this and override the methods you care about.
class TestExtensionService : public ExtensionServiceInterface {
 public:
  virtual ~TestExtensionService();

  // ExtensionServiceInterface implementation.
  virtual const ExtensionSet* extensions() const OVERRIDE;
  virtual const ExtensionSet* disabled_extensions() const OVERRIDE;
  virtual extensions::PendingExtensionManager*
      pending_extension_manager() OVERRIDE;

  virtual bool UpdateExtension(
      const std::string& id,
      const base::FilePath& path,
      const GURL& download_url,
      extensions::CrxInstaller** out_crx_installer) OVERRIDE;
  virtual const extensions::Extension* GetExtensionById(
      const std::string& id, bool include_disabled) const OVERRIDE;
  virtual const extensions::Extension* GetInstalledExtension(
      const std::string& id) const OVERRIDE;
  virtual const extensions::Extension* GetPendingExtensionUpdate(
      const std::string& extension_id) const OVERRIDE;
  virtual void FinishDelayedInstallation(
      const std::string& extension_id) OVERRIDE;
  virtual bool IsExtensionEnabled(
      const std::string& extension_id) const OVERRIDE;
  virtual bool IsExternalExtensionUninstalled(
      const std::string& extension_id) const OVERRIDE;

  virtual void CheckManagementPolicy() OVERRIDE;
  virtual void CheckForUpdatesSoon() OVERRIDE;

  virtual syncer::SyncMergeResult MergeDataAndStartSyncing(
      syncer::ModelType type,
      const syncer::SyncDataList& initial_sync_data,
      scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
      scoped_ptr<syncer::SyncErrorFactory> sync_error_factory) OVERRIDE;
  virtual void StopSyncing(syncer::ModelType type) OVERRIDE;
  virtual syncer::SyncDataList GetAllSyncData(
      syncer::ModelType type) const OVERRIDE;
  virtual syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) OVERRIDE;

  virtual bool is_ready() OVERRIDE;

  virtual base::SequencedTaskRunner* GetFileTaskRunner() OVERRIDE;

  virtual void AddExtension(const extensions::Extension* extension) OVERRIDE;
  virtual void AddComponentExtension(
      const extensions::Extension* extension) OVERRIDE;

  virtual void UnloadExtension(
      const std::string& extension_id,
      extension_misc::UnloadedExtensionReason reason) OVERRIDE;

  virtual void SyncExtensionChangeIfNeeded(
      const extensions::Extension& extension) OVERRIDE;
};

#endif  // CHROME_BROWSER_EXTENSIONS_TEST_EXTENSION_SERVICE_H_
