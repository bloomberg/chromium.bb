// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_CRASH_COLLECTOR_ARC_CRASH_COLLECTOR_BRIDGE_H_
#define COMPONENTS_ARC_CRASH_COLLECTOR_ARC_CRASH_COLLECTOR_BRIDGE_H_

#include "base/macros.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace arc {

// Relays dumps for non-native ARC crashes to the crash reporter in Chrome OS.
class ArcCrashCollectorBridge : public ArcService,
                                public ArcBridgeService::Observer,
                                public mojom::CrashCollectorHost {
 public:
  explicit ArcCrashCollectorBridge(ArcBridgeService* bridge);
  ~ArcCrashCollectorBridge() override;

  // ArcBridgeService::Observer overrides.
  void OnCrashCollectorInstanceReady() override;

  // mojom::CrashCollectorHost overrides.
  void DumpCrash(const mojo::String& type, mojo::ScopedHandle pipe) override;

 private:
  mojo::Binding<mojom::CrashCollectorHost> binding_;

  DISALLOW_COPY_AND_ASSIGN(ArcCrashCollectorBridge);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_CRASH_COLLECTOR_ARC_CRASH_COLLECTOR_BRIDGE_H_
