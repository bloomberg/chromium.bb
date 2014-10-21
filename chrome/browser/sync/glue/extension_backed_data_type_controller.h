// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_EXTENSION_BACKED_DATA_TYPE_CONTROLLER_H_
#define CHROME_BROWSER_SYNC_GLUE_EXTENSION_BACKED_DATA_TYPE_CONTROLLER_H_

#include <string>

#include "base/compiler_specific.h"
#include "components/sync_driver/ui_data_type_controller.h"
#include "extensions/browser/extension_registry_observer.h"

class Profile;

namespace browser_sync {

// A data type controller for types backed by an extension. Manages waiting for
// extension enable/disable and triggering reconfigurations as necessary.
class ExtensionBackedDataTypeController
    : public sync_driver::UIDataTypeController,
      public extensions::ExtensionRegistryObserver {
 public:
  ExtensionBackedDataTypeController(
      syncer::ModelType type,
      const std::string& extension_hash,
      sync_driver::SyncApiComponentFactory* sync_factory,
      Profile* profile);

  // UIDataTypeController overrides.
  bool ReadyForStart() const override;
  bool StartModels() override;

  // ExtensionRegistryObserver implementation.
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override;
  void OnExtensionUnloaded(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension,
      extensions::UnloadedExtensionInfo::Reason reason) override;

 private:
  // Refcounted via DataTypeController.
  ~ExtensionBackedDataTypeController() override;

  // Returns whether the extension syncing this type is enabled.
  bool IsSyncingExtensionEnabled() const;

  // Returns whether |extension|'s id hash matches |extension_hash_|.
  bool DoesExtensionMatch(const extensions::Extension& extension) const;

  // A hash of the extension id this datatype is dependent on.
  const std::string extension_hash_;

  // The owning profile.
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionBackedDataTypeController);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_EXTENSION_BACKED_DATA_TYPE_CONTROLLER_H_
