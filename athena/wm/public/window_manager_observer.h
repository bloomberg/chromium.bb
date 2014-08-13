// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_SCREEN_PUBLIC_WINDOW_MANAGER_OBSERVER_H_
#define ATHENA_SCREEN_PUBLIC_WINDOW_MANAGER_OBSERVER_H_

#include "athena/athena_export.h"

namespace athena {

class ATHENA_EXPORT WindowManagerObserver {
 public:
  virtual ~WindowManagerObserver() {}

  // Called when the overview mode is displayed.
  virtual void OnOverviewModeEnter() = 0;

  // Called when going out of overview mode.
  virtual void OnOverviewModeExit() = 0;
};

}  // namespace athena

#endif  // ATHENA_SCREEN_PUBLIC_WINDOW_MANAGER_OBSERVER_H_
