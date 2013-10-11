// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_USER_USER_OBSERVER_H_
#define ASH_SYSTEM_USER_USER_OBSERVER_H_

#include "ash/ash_export.h"

namespace ash {

class ASH_EXPORT UserObserver {
 public:
  virtual ~UserObserver() {}

  // A user got updated / changed.
  virtual void OnUserUpdate() = 0;

  // A user was added to the existing session.
  virtual void OnUserAddedToSession() = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_USER_USER_OBSERVER_H_
