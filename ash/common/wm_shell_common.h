// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_WM_SHELL_COMMON_H_
#define ASH_COMMON_WM_SHELL_COMMON_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/macros.h"

namespace ash {

class MruWindowTracker;

// Contains code used by both shell implementations.
class ASH_EXPORT WmShellCommon {
 public:
  WmShellCommon();
  ~WmShellCommon();

  MruWindowTracker* mru_window_tracker() { return mru_window_tracker_.get(); }

  void CreateMruWindowTracker();
  void DeleteMruWindowTracker();

 private:
  std::unique_ptr<MruWindowTracker> mru_window_tracker_;

  DISALLOW_COPY_AND_ASSIGN(WmShellCommon);
};

}  // namespace ash

#endif  // ASH_COMMON_WM_SHELL_COMMON_H_
