// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/obb_mounter/arc_obb_mounter_bridge.h"

#include "base/logging.h"
#include "components/arc/arc_bridge_service.h"

namespace arc {

ArcObbMounterBridge::ArcObbMounterBridge(ArcBridgeService* bridge_service)
    : ArcService(bridge_service),
      binding_(this) {
  arc_bridge_service()->AddObserver(this);
}

ArcObbMounterBridge::~ArcObbMounterBridge() {
  arc_bridge_service()->RemoveObserver(this);
}

void ArcObbMounterBridge::OnObbMounterInstanceReady() {
  mojom::ObbMounterInstance* obb_mounter_instance =
      arc_bridge_service()->obb_mounter_instance();
  obb_mounter_instance->Init(binding_.CreateInterfacePtrAndBind());
}

void ArcObbMounterBridge::MountObb(const mojo::String& obb_file,
                                   const mojo::String& target_path,
                                   int32_t owner_gid,
                                   const MountObbCallback& callback) {
  // TODO(hashimoto): Implement this. crbug.com/613480
  NOTIMPLEMENTED();
  callback.Run(false);
}

void ArcObbMounterBridge::UnmountObb(const mojo::String& target_path,
                                     const UnmountObbCallback& callback) {
  // TODO(hashimoto): Implement this. crbug.com/613480
  NOTIMPLEMENTED();
  callback.Run(false);
}

}  // namespace arc
