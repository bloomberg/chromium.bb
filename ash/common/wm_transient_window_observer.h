// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_WM_TRANSIENT_WINDOW_OBSERVER_H_
#define ASH_COMMON_WM_TRANSIENT_WINDOW_OBSERVER_H_

#include "ash/ash_export.h"

namespace ash {

class WmWindow;

class ASH_EXPORT WmTransientWindowObserver {
 public:
  virtual void OnTransientChildAdded(WmWindow* window, WmWindow* transient) {}
  virtual void OnTransientChildRemoved(WmWindow* window, WmWindow* transient) {}

 protected:
  virtual ~WmTransientWindowObserver() {}
};

}  // namespace ash

#endif  // ASH_COMMON_WM_TRANSIENT_WINDOW_OBSERVER_H_
