// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SESSION_STATE_OBSERVER_H_
#define ASH_SESSION_STATE_OBSERVER_H_

#include <string>

#include "ash/ash_export.h"

namespace ash {

class ASH_EXPORT SessionStateObserver {
 public:
  // Called when active user has changed.
  virtual void ActiveUserChanged(const std::string& user_id) {}

 protected:
  virtual ~SessionStateObserver() {}
};

}  // namespace ash

#endif  // ASH_SESSION_STATE_OBSERVER_H_
