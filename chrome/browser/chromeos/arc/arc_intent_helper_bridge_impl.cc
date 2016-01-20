// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_intent_helper_bridge_impl.h"

#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "components/arc/common/intent_helper.mojom.h"

namespace arc {

ArcIntentHelperBridgeImpl::ArcIntentHelperBridgeImpl() : binding_(this) {}

ArcIntentHelperBridgeImpl::~ArcIntentHelperBridgeImpl() {
  ArcBridgeService* bridge_service = ArcBridgeService::Get();
  DCHECK(bridge_service);
  bridge_service->RemoveObserver(this);
}

void ArcIntentHelperBridgeImpl::StartObservingBridgeServiceChanges() {
  ArcBridgeService* bridge_service = ArcBridgeService::Get();
  DCHECK(bridge_service);
  bridge_service->AddObserver(this);
}

void ArcIntentHelperBridgeImpl::OnIntentHelperInstanceReady() {
  IntentHelperHostPtr host;
  binding_.Bind(mojo::GetProxy(&host));
  ArcBridgeService* bridge_service = ArcBridgeService::Get();
  bridge_service->intent_helper_instance()->Init(std::move(host));
}

void ArcIntentHelperBridgeImpl::OnOpenUrl(const mojo::String& url) {
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

}  // namespace arc
