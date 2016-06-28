// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_CHROMEOS_SESSION_LAST_WINDOW_CLOSED_OBSERVER_H_
#define ASH_COMMON_SYSTEM_CHROMEOS_SESSION_LAST_WINDOW_CLOSED_OBSERVER_H_

#include "ash/ash_export.h"

namespace ash {

class ASH_EXPORT LastWindowClosedObserver {
 public:
  virtual void OnLastWindowClosed() = 0;

 protected:
  virtual ~LastWindowClosedObserver() {}
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_CHROMEOS_SESSION_LAST_WINDOW_CLOSED_OBSERVER_H_
