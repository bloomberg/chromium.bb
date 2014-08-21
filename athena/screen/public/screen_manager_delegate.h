// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_SCREEN_PUBLIC_SCREEN_MANAGER_DELEGATE_H_
#define ATHENA_SCREEN_PUBLIC_SCREEN_MANAGER_DELEGATE_H_

#include "athena/athena_export.h"

namespace gfx {
class Insets;
}

namespace athena {

// Delegate of the ScreenManager.
class ATHENA_EXPORT ScreenManagerDelegate {
 public:
  virtual ~ScreenManagerDelegate() {}

  // Sets the screen's work area insets.
  virtual void SetWorkAreaInsets(const gfx::Insets& insets) = 0;
};

}  // namespace athena

#endif  // ATHENA_SCREEN_PUBLIC_SCREEN_MANAGER_DELEGATE_H_
