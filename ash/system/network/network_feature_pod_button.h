// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_FEATURE_POD_BUTTON_H_
#define ASH_SYSTEM_NETWORK_NETWORK_FEATURE_POD_BUTTON_H_

#include "ash/system/network/network_icon_animation_observer.h"
#include "ash/system/network/tray_network_state_model.h"
#include "ash/system/unified/feature_pod_button.h"

namespace ash {

// Button view class for network feature pod button. It uses network_icon
// animation to implement network connecting animation on feature pod button.
class NetworkFeaturePodButton : public FeaturePodButton,
                                public network_icon::AnimationObserver,
                                public TrayNetworkStateModel::Observer {
 public:
  explicit NetworkFeaturePodButton(FeaturePodControllerBase* controller);
  ~NetworkFeaturePodButton() override;

  // network_icon::AnimationObserver:
  void NetworkIconChanged() override;

  // TrayNetworkStateModel::Observer:
  void ActiveNetworkStateChanged() override;

  // views::Button:
  const char* GetClassName() const override;

 private:
  void Update();
  void SetTooltipState(const base::string16& tooltip_state);

  DISALLOW_COPY_AND_ASSIGN(NetworkFeaturePodButton);
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_FEATURE_POD_BUTTON_H_
