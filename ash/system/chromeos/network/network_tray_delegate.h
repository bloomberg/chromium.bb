// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_NETWORK_NETWORK_TRAY_DELEGATE_H
#define ASH_SYSTEM_CHROMEOS_NETWORK_NETWORK_TRAY_DELEGATE_H

#include "ash/system/chromeos/network/network_observer.h"

namespace ash {

class NetworkTrayDelegate {
 public:
  virtual ~NetworkTrayDelegate() {}

  // Notifies that the |index|-th link on the notification is clicked.
  virtual void NotificationLinkClicked(
      NetworkObserver::MessageType message_type,
      size_t link_index) = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_NETWORK_NETWORK_TRAY_DELEGATE_H
