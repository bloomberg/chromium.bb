// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_COMMON_WM_DISPLAY_OBSERVER_H_
#define ASH_WM_COMMON_WM_DISPLAY_OBSERVER_H_

#include "ash/wm/common/ash_wm_common_export.h"

namespace ash {
namespace wm {

class WmWindow;

// Used to track changes in display configuration.
class ASH_WM_COMMON_EXPORT WmDisplayObserver {
 public:
  virtual void OnDisplayConfigurationChanged() {}

 protected:
  virtual ~WmDisplayObserver() {}
};

}  // namespace wm
}  // namespace ash

#endif  // ASH_WM_COMMON_WM_DISPLAY_OBSERVER_H_
