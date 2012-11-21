// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_NETWORK_NETWORK_DETAILED_VIEW_H
#define ASH_SYSTEM_CHROMEOS_NETWORK_NETWORK_DETAILED_VIEW_H

#include "ash/system/tray/tray_details_view.h"

namespace ash {
namespace internal {

namespace tray {

// Abstract base class for all NetworkDetailedView derived subclasses,
// which includes NetworkWifiDetailedView and NetworkListDetailedViewBase.
class NetworkDetailedView : public TrayDetailsView {
 public:
  explicit NetworkDetailedView(SystemTrayItem* owner)
      : TrayDetailsView(owner) {
  }

  enum DetailedViewType {
    LIST_VIEW,
    WIFI_VIEW,
  };

  virtual void Init() = 0;
  virtual DetailedViewType GetViewType() const = 0;
  // Updates network data and UI of the detailed view.
  virtual void Update() = 0;

 protected:
  virtual ~NetworkDetailedView() {}
};

}  // namespace tray

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_NETWORK_NETWORK_DETAILED_VIEW_H
