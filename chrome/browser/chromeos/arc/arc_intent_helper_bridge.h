// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_CHROMEOS_ARC_ARC_INTENT_HELPER_BRIDGE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ARC_INTENT_HELPER_BRIDGE_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/arc/settings_bridge.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service.h"
#include "components/arc/common/intent_helper.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace arc {

// Receives intents from ARC.
class ArcIntentHelperBridge : public ArcService,
                              public ArcBridgeService::Observer,
                              public IntentHelperHost,
                              public SettingsBridge::Delegate {
 public:
  explicit ArcIntentHelperBridge(ArcBridgeService* bridge_service);
  ~ArcIntentHelperBridge() override;

  // ArcBridgeService::Observer
  void OnIntentHelperInstanceReady() override;
  void OnIntentHelperInstanceClosed() override;

  // SettingsBridge::Delegate. Sends a broadcast to the ArcIntentHelper app
  // in Android.
  void OnBroadcastNeeded(const std::string& action,
                         const base::DictionaryValue& extras) override;

  // arc::IntentHelperHost
  void OnOpenUrl(const mojo::String& url) override;

 private:
  mojo::Binding<IntentHelperHost> binding_;
  scoped_ptr<SettingsBridge> settings_bridge_;

  DISALLOW_COPY_AND_ASSIGN(ArcIntentHelperBridge);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ARC_INTENT_HELPER_BRIDGE_H_
