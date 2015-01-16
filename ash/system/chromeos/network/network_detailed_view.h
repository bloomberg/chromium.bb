// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_NETWORK_NETWORK_DETAILED_VIEW_H
#define ASH_SYSTEM_CHROMEOS_NETWORK_NETWORK_DETAILED_VIEW_H

#include "ash/system/tray/tray_details_view.h"
#include "chromeos/network/network_state_handler.h"

namespace ash {
namespace tray {

// Abstract base class for all NetworkDetailedView derived subclasses,
// which includes NetworkWifiDetailedView and NetworkStateListDetailedView.
class NetworkDetailedView : public TrayDetailsView {
 public:
  enum DetailedViewType {
    LIST_VIEW,
    STATE_LIST_VIEW,
    WIFI_VIEW,
  };

  explicit NetworkDetailedView(SystemTrayItem* owner)
      : TrayDetailsView(owner) {
  }

  virtual void Init() = 0;

  virtual DetailedViewType GetViewType() const = 0;

  // Called when the contents of the network list have changed or when any
  // Manager properties (e.g. technology state) have changed.
  // (Called only from TrayNetworkStateObserver).
  virtual void Update() = 0;

 protected:
  ~NetworkDetailedView() override {}
};

}  // namespace tray
}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_NETWORK_NETWORK_DETAILED_VIEW_H
