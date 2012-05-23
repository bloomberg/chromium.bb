// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_OBSERVER_H
#define ASH_SYSTEM_NETWORK_NETWORK_OBSERVER_H
#pragma once

#include "base/string16.h"

namespace ash {

struct NetworkIconInfo;
class NetworkTrayDelegate;

class NetworkTrayDelegate {
 public:
  virtual ~NetworkTrayDelegate() {}

  virtual void NotificationLinkClicked() = 0;
};

class NetworkObserver {
 public:
  enum ErrorType {
    // Priority order, highest to lowest.
    ERROR_CONNECT_FAILED,
    ERROR_DATA_NONE,
    ERROR_DATA_LOW
  };

  virtual ~NetworkObserver() {}

  virtual void OnNetworkRefresh(const NetworkIconInfo& info) = 0;

  // Sets a network error notification. |error_type| identifies the type of
  // error. |delegate|->NotificationLinkClicked() will be called if |link_text|
  // is clicked (if supplied, |link_text| may be empty).
  virtual void SetNetworkError(NetworkTrayDelegate* delegate,
                               ErrorType error_type,
                               const string16& title,
                               const string16& message,
                               const string16& link_text) = 0;
  // Clears the error notification for |error_type|.
  virtual void ClearNetworkError(ErrorType error_type) = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_OBSERVER_H
