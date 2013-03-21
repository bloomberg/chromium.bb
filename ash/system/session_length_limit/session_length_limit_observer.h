// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_SESSION_LENGTH_LIMIT_SESSION_LENGTH_LIMIT_OBSERVER_H_
#define ASH_SYSTEM_SESSION_LENGTH_LIMIT_SESSION_LENGTH_LIMIT_OBSERVER_H_

namespace ash {

// Observer for the session length limit.
class SessionLengthLimitObserver {
 public:
  virtual ~SessionLengthLimitObserver() {}

  // Called when the session start time is updated.
  virtual void OnSessionStartTimeChanged() = 0;

  // Called when the session length limit is updated.
  virtual void OnSessionLengthLimitChanged() = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_SESSION_LENGTH_LIMIT_SESSION_LENGTH_LIMIT_OBSERVER_H_
