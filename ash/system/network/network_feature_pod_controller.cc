// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_feature_pod_controller.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/network/network_feature_pod_button.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

NetworkFeaturePodController::NetworkFeaturePodController(
    UnifiedSystemTrayController* tray_controller)
    : tray_controller_(tray_controller) {}

NetworkFeaturePodController::~NetworkFeaturePodController() = default;

FeaturePodButton* NetworkFeaturePodController::CreateButton() {
  DCHECK(!button_);
  button_ = new NetworkFeaturePodButton(this);
  return button_;
}

void NetworkFeaturePodController::OnPressed() {
  tray_controller_->ShowNetworkDetailedView();
}

}  // namespace ash
