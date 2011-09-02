// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AURA_FOCUS_MANAGER_H_
#define AURA_FOCUS_MANAGER_H_
#pragma once

#include "base/basictypes.h"

namespace aura {
class Window;

namespace internal {

// The FocusManager, when attached to a Window, tracks changes to keyboard input
// focus within that Window's hierarchy.
class FocusManager {
 public:
  explicit FocusManager(Window* owner);
  ~FocusManager();

  void SetFocusedWindow(Window* window);

  Window* focused_window() { return focused_window_; }

 private:
  Window* owner_;
  Window* focused_window_;

  DISALLOW_COPY_AND_ASSIGN(FocusManager);
};

}  // namespace internal
}  // namespace aura

#endif  // AURA_FOCUS_MANAGER_H_
