// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_STORAGE_MANAGER_ARC_STORAGE_MANAGER_H
#define COMPONENTS_ARC_STORAGE_MANAGER_ARC_STORAGE_MANAGER_H

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service.h"
#include "components/arc/common/storage_manager.mojom.h"

namespace arc {

// This class represents as a simple proxy of StorageManager to Chrome OS.
class ArcStorageManager : public ArcService,
                          public ArcBridgeService::Observer {
 public:
  explicit ArcStorageManager(ArcBridgeService* bridge_service);
  ~ArcStorageManager() override;

  // Gets the singleton instance of the ARC Storage Manager.
  // MUST be called while ArcStorageManager is alive, otherwise it returns
  // nullptr (or aborts on Debug build).
  static ArcStorageManager* Get();

  // ArcBridgeService::Observer
  void OnStorageManagerInstanceReady() override;

  // Opens detailed preference screen of private volume on ARC.
  // Returns false when an instance of ARC-side isn't ready yet.
  bool OpenPrivateVolumeSettings();

  // Gets storage usage of all application's APK, data, and cache size.
  using GetApplicationsSizeCallback = base::Callback<
      void(bool succeeded,
           uint64_t total_code_bytes,
           uint64_t total_data_bytes,
           uint64_t total_cache_bytes)>;
  bool GetApplicationsSize(const GetApplicationsSizeCallback& callback);

 private:
  mojom::StorageManagerInstance* GetStorageManagerInstance();

  DISALLOW_COPY_AND_ASSIGN(ArcStorageManager);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_STORAGE_MANAGER_ARC_STORAGE_MANAGER_H
