// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_SESSION_SESSION_LENGTH_LIMIT_OBSERVER_H_
#define ASH_SYSTEM_CHROMEOS_SESSION_SESSION_LENGTH_LIMIT_OBSERVER_H_

#include "ash/ash_export.h"

namespace ash {

// Observer for the session length limit.
class ASH_EXPORT SessionLengthLimitObserver {
 public:
  virtual ~SessionLengthLimitObserver() {}

  // Called when the session start time is updated.
  virtual void OnSessionStartTimeChanged() = 0;

  // Called when the session length limit is updated.
  virtual void OnSessionLengthLimitChanged() = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_SESSION_SESSION_LENGTH_LIMIT_OBSERVER_H_
