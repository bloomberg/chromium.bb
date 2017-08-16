// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_SYSTEM_TRAY_DELEGATE_H_
#define ASH_SYSTEM_TRAY_SYSTEM_TRAY_DELEGATE_H_

#include "ash/ash_export.h"

namespace ash {

class NetworkingConfigDelegate;

// SystemTrayDelegate is intended for delegating tasks in the System Tray to the
// application (e.g. Chrome). These tasks should be limited to application
// (browser) specific tasks. For non application specific tasks, where possible,
// components/, chromeos/, device/, etc., code should be used directly.
//
// DEPRECATED: This class is being replaced with SystemTrayController and
// SessionController to support mash/mustash. Add new code to those classes.
class ASH_EXPORT SystemTrayDelegate {
 public:
  SystemTrayDelegate();
  virtual ~SystemTrayDelegate();

  // Returns NetworkingConfigDelegate. May return nullptr.
  virtual NetworkingConfigDelegate* GetNetworkingConfigDelegate() const;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_SYSTEM_TRAY_DELEGATE_H_
