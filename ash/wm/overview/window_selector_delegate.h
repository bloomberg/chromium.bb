// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_WINDOW_SELECTOR_DELEGATE_H_
#define ASH_WM_OVERVIEW_WINDOW_SELECTOR_DELEGATE_H_

#include "ash/ash_export.h"
#include "base/compiler_specific.h"

namespace aura {
class Window;
}

namespace ash {

// Implement this class to handle the selection event from WindowSelector.
class ASH_EXPORT WindowSelectorDelegate {
 public:
  // Invoked if selection is ended.
  virtual void OnSelectionEnded() = 0;

 protected:
  virtual ~WindowSelectorDelegate() {}
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_WINDOW_SELECTOR_DELEGATE_H_
