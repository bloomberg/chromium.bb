// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_COMMON_WM_LOOKUP_H_
#define ASH_WM_COMMON_WM_LOOKUP_H_

#include <stdint.h>

#include "ash/wm/common/ash_wm_common_export.h"

namespace views {
class Widget;
}

namespace ash {
namespace wm {

class WmRootWindowController;
class WmWindow;

// WmLookup is used to lookup various wm types.
class ASH_WM_COMMON_EXPORT WmLookup {
 public:
  static void Set(WmLookup* lookup);
  static WmLookup* Get();

  // Returns the WmRootWindowController with the specified display id, or null
  // if there isn't one.
  virtual WmRootWindowController* GetRootWindowControllerWithDisplayId(
      int64_t id) = 0;

  // Returns the WmWindow for the specified widget.
  virtual WmWindow* GetWindowForWidget(views::Widget* widget) = 0;

 protected:
  virtual ~WmLookup() {}

 private:
  static WmLookup* instance_;
};

}  // namespace wm
}  // namespace ash

#endif  // ASH_WM_COMMON_WM_LOOKUP_H_
