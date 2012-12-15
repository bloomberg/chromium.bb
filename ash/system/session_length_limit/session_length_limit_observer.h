// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_SESSION_LENGTH_LIMIT_SESSION_LENGTH_LIMIT_OBSERVER_H_
#define ASH_SYSTEM_SESSION_LENGTH_LIMIT_SESSION_LENGTH_LIMIT_OBSERVER_H_

#include "base/time.h"

namespace ash {

// Observer for the session length limit.
class SessionLengthLimitObserver {
 public:
  virtual ~SessionLengthLimitObserver() {}

  // Called when the session start time is updated.
  virtual void OnSessionStartTimeChanged(
      const base::Time& session_start_time) = 0;

  // Called when the session length limit is updated.
  virtual void OnSessionLengthLimitChanged(const base::TimeDelta& limit) = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_SESSION_LENGTH_LIMIT_SESSION_LENGTH_LIMIT_OBSERVER_H_
