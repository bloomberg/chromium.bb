// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_CHROMEOS_ARC_ARC_INTENT_HELPER_BRIDGE_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ARC_INTENT_HELPER_BRIDGE_IMPL_H_

#include "base/macros.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/common/intent_helper.mojom.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace arc {

// Receives intents from ARC.
class ArcIntentHelperBridgeImpl : public ArcIntentHelperBridge,
                                  public ArcBridgeService::Observer,
                                  public IntentHelperHost {
 public:
  ArcIntentHelperBridgeImpl();
  ~ArcIntentHelperBridgeImpl() override;

  // Starts listening to state changes of the ArcBridgeService.
  // This must be called before the bridge service starts bootstrapping.
  void StartObservingBridgeServiceChanges() override;

  // ArcBridgeService::Observer
  void OnIntentHelperInstanceReady() override;

  // arc::IntentHelperHost
  void OnOpenUrl(const mojo::String& url) override;

 private:
  mojo::Binding<IntentHelperHost> binding_;

  DISALLOW_COPY_AND_ASSIGN(ArcIntentHelperBridgeImpl);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ARC_INTENT_HELPER_BRIDGE_IMPL_H_
