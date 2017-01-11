// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_WM_LOOKUP_H_
#define ASH_COMMON_WM_LOOKUP_H_

#include <stdint.h>

#include "ash/ash_export.h"

namespace views {
class Widget;
}

namespace ash {

class RootWindowController;
class WmWindow;

// WmLookup is used to lookup various wm types.
class ASH_EXPORT WmLookup {
 public:
  static void Set(WmLookup* lookup);
  static WmLookup* Get();

  // Returns the RootWindowController with the specified display id, or null if
  // there isn't one.
  virtual RootWindowController* GetRootWindowControllerWithDisplayId(
      int64_t id) = 0;

  // Returns the WmWindow for the specified widget.
  virtual WmWindow* GetWindowForWidget(views::Widget* widget) = 0;

 protected:
  virtual ~WmLookup() {}

 private:
  static WmLookup* instance_;
};

}  // namespace ash

#endif  // ASH_COMMON_WM_LOOKUP_H_
