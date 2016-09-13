// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AUTOCLICK_COMMON_AUTOCLICK_CONTROLLER_COMMON_DELEGATE_H
#define ASH_AUTOCLICK_COMMON_AUTOCLICK_CONTROLLER_COMMON_DELEGATE_H

#include "base/macros.h"

namespace views {
class Widget;
}

namespace gfx {
class Point;
}

namespace ash {

class AutoclickControllerCommonDelegate {
 public:
  AutoclickControllerCommonDelegate() {}
  virtual ~AutoclickControllerCommonDelegate() {}

  // Creates a ring widget at |event_location|.
  // AutoclickControllerCommonDelegate still has ownership of the widget they
  // created.
  virtual views::Widget* CreateAutoclickRingWidget(
      const gfx::Point& event_location) = 0;

  // Moves |widget| to |event_location|.
  virtual void UpdateAutoclickRingWidget(views::Widget* widget,
                                         const gfx::Point& event_location) = 0;

  // Generates a click with |mouse_event_flags| at |event_location|.
  virtual void DoAutoclick(const gfx::Point& event_location,
                           const int mouse_event_flags) = 0;

  virtual void OnAutoclickCanceled() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutoclickControllerCommonDelegate);
};

}  // namespace ash

#endif  // ASH_AUTOCLICK_COMMON_AUTOCLICK_CONTROLLER_COMMON_DELEGATE_H
