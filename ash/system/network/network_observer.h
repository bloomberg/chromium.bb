// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_OBSERVER_H
#define ASH_SYSTEM_NETWORK_NETWORK_OBSERVER_H

#include <vector>

#include "base/string16.h"

namespace ash {

struct NetworkIconInfo;

class NetworkTrayDelegate {
 public:
  virtual ~NetworkTrayDelegate() {}

  // Notifies that the |index|-th link on the notification is clicked.
  virtual void NotificationLinkClicked(size_t index) = 0;
};

class NetworkObserver {
 public:
  enum MessageType {
    // Priority order, highest to lowest.
    ERROR_CONNECT_FAILED,

    MESSAGE_DATA_NONE,
    MESSAGE_DATA_LOW,
    MESSAGE_DATA_PROMO,
  };

  virtual ~NetworkObserver() {}

  virtual void OnNetworkRefresh(const NetworkIconInfo& info) = 0;

  // Sets a network message notification. |message_type| identifies the type of
  // message. |delegate|->NotificationLinkClicked() will be called if any of the
  // |links| are clicked (if supplied, |links| may be empty).
  virtual void SetNetworkMessage(NetworkTrayDelegate* delegate,
                                MessageType message_type,
                                const string16& title,
                                const string16& message,
                                const std::vector<string16>& links) = 0;
  // Clears the message notification for |message_type|.
  virtual void ClearNetworkMessage(MessageType message_type) = 0;

  // Called when the user attempted to toggle Wi-Fi enable/disable.
  // NOTE: Toggling is asynchronous and subsequent calls to query the current
  // state may return the old value.
  virtual void OnWillToggleWifi() = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_OBSERVER_H
