// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_OBB_MOUNTER_ARC_OBB_MOUNTER_BRIDGE_H_
#define COMPONENTS_ARC_OBB_MOUNTER_ARC_OBB_MOUNTER_BRIDGE_H_

#include <string>

#include "base/macros.h"
#include "components/arc/arc_service.h"
#include "components/arc/common/obb_mounter.mojom.h"
#include "components/arc/instance_holder.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// This class handles OBB mount/unmount requests from Android.
class ArcObbMounterBridge
    : public KeyedService,
      public InstanceHolder<mojom::ObbMounterInstance>::Observer,
      public mojom::ObbMounterHost {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcObbMounterBridge* GetForBrowserContext(
      content::BrowserContext* context);

  ArcObbMounterBridge(content::BrowserContext* context,
                      ArcBridgeService* bridge_service);
  ~ArcObbMounterBridge() override;

  // InstanceHolder<mojom::ObbMounterInstance>::Observer overrides:
  void OnInstanceReady() override;

  // mojom::ObbMounterHost overrides:
  void MountObb(const std::string& obb_file,
                const std::string& target_path,
                int32_t owner_gid,
                const MountObbCallback& callback) override;
  void UnmountObb(const std::string& target_path,
                  const UnmountObbCallback& callback) override;

 private:
  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.

  mojo::Binding<mojom::ObbMounterHost> binding_;

  DISALLOW_COPY_AND_ASSIGN(ArcObbMounterBridge);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_OBB_MOUNTER_ARC_OBB_MOUNTER_BRIDGE_H_
