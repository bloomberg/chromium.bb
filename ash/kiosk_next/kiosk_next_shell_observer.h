// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_KIOSK_NEXT_KIOSK_NEXT_SHELL_OBSERVER_H_
#define ASH_KIOSK_NEXT_KIOSK_NEXT_SHELL_OBSERVER_H_

#include "ash/ash_export.h"
#include "base/observer_list_types.h"

namespace ash {

class ASH_EXPORT KioskNextShellObserver : public base::CheckedObserver {
 public:
  // This method is only called once when KioskNextShell is enabled.
  virtual void OnKioskNextEnabled() {}
};

}  // namespace ash

#endif  // ASH_KIOSK_NEXT_KIOSK_NEXT_SHELL_OBSERVER_H_
