// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_USER_USER_OBSERVER_H_
#define ASH_SYSTEM_USER_USER_OBSERVER_H_

namespace ash {

class UserObserver {
 public:
  virtual ~UserObserver() {}

  virtual void OnUserUpdate() = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_USER_USER_OBSERVER_H_
