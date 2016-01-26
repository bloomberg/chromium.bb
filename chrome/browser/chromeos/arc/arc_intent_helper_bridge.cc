// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_intent_helper_bridge.h"

#include "base/json/json_writer.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "components/arc/common/intent_helper.mojom.h"

namespace arc {

ArcIntentHelperBridge::ArcIntentHelperBridge(ArcBridgeService* bridge_service)
    : ArcService(bridge_service), binding_(this) {
  arc_bridge_service()->AddObserver(this);
}

ArcIntentHelperBridge::~ArcIntentHelperBridge() {
  arc_bridge_service()->RemoveObserver(this);
}

void ArcIntentHelperBridge::OnIntentHelperInstanceReady() {
  arc_bridge_service()->intent_helper_instance()->Init(
      binding_.CreateInterfacePtrAndBind());
  settings_bridge_.reset(new SettingsBridge(this));
}

void ArcIntentHelperBridge::OnIntentHelperInstanceClosed() {
  settings_bridge_.reset();
}

void ArcIntentHelperBridge::OnOpenUrl(const mojo::String& url) {
  GURL gurl(url.get());
  if (!gurl.is_valid())
    return;

  chrome::ScopedTabbedBrowserDisplayer displayer(
      ProfileManager::GetActiveUserProfile(), chrome::HOST_DESKTOP_TYPE_ASH);
  chrome::AddSelectedTabWithURL(displayer.browser(), gurl,
                                ui::PAGE_TRANSITION_LINK);

  // Since the ScopedTabbedBrowserDisplayer does not guarantee that the
  // browser will be shown on the active desktop, we ensure the visibility.
  multi_user_util::MoveWindowToCurrentDesktop(
      displayer.browser()->window()->GetNativeWindow());
}

void ArcIntentHelperBridge::OnBroadcastNeeded(
    const std::string& action,
    const base::DictionaryValue& extras) {
  if (arc_bridge_service()->state() != ArcBridgeService::State::READY) {
    LOG(ERROR) << "Bridge service is not ready.";
    return;
  }

  std::string extras_json;
  bool write_success = base::JSONWriter::Write(extras, &extras_json);
  DCHECK(write_success);

  if (arc_bridge_service()->intent_helper_version() >= 1) {
    arc_bridge_service()->intent_helper_instance()->SendBroadcast(
        action, "org.chromium.arc.intent_helper",
        "org.chromium.arc.intent_helper.SettingsReceiver", extras_json);
  }
}

}  // namespace arc
