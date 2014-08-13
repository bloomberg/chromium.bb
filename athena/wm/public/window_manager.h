// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_SCREEN_PUBLIC_WINDOW_MANAGER_H_
#define ATHENA_SCREEN_PUBLIC_WINDOW_MANAGER_H_

#include "athena/athena_export.h"

namespace athena {

class WindowManagerObserver;

// Manages the application, web windows.
class ATHENA_EXPORT WindowManager {
 public:
  // Creates and deletes the singleton object of the WindowManager
  // implementation.
  static WindowManager* Create();
  static void Shutdown();
  static WindowManager* GetInstance();

  virtual ~WindowManager() {}

  virtual void ToggleOverview() = 0;

  virtual bool IsOverviewModeActive() = 0;

  virtual void AddObserver(WindowManagerObserver* observer) = 0;
  virtual void RemoveObserver(WindowManagerObserver* observer) = 0;
};

}  // namespace athena

#endif  // ATHENA_SCREEN_PUBLIC_WINDOW_MANAGER_H_
