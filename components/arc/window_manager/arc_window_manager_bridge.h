// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_WINDOW_MANAGER_ARC_WINDOW_MANAGER_BRIDGE_H_
#define COMPONENTS_ARC_WINDOW_MANAGER_ARC_WINDOW_MANAGER_BRIDGE_H_

#include <string>

#include "base/macros.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service.h"
#include "components/arc/instance_holder.h"

namespace arc {

class ArcWindowManagerBridge
    : public ArcService,
      public InstanceHolder<mojom::WindowManagerInstance>::Observer {
 public:
  explicit ArcWindowManagerBridge(ArcBridgeService* bridge_service);
  ~ArcWindowManagerBridge() override;

  // InstanceHolder<mojom::WindowManagerInstance>::Observer
  void OnInstanceReady() override;

 private:

  DISALLOW_COPY_AND_ASSIGN(ArcWindowManagerBridge);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_WINDOW_MANAGER_ARC_WINDOW_MANAGER_BRIDGE_H_
