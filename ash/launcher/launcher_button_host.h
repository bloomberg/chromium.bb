// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LAUNCHER_LAUNCHER_BUTTON_HOST_H_
#define ASH_LAUNCHER_LAUNCHER_BUTTON_HOST_H_
#pragma once

#include "ash/ash_export.h"
#include "ash/wm/shelf_auto_hide_behavior.h"
#include "base/string16.h"

namespace views {
class MouseEvent;
class View;
}

namespace ash {
namespace internal {

// The launcher buttons communicate back to the host by way of this interface.
// This interface is used to enable reordering the items on the launcher.
class ASH_EXPORT LauncherButtonHost {
 public:
  // Invoked when the mose is pressed on a view.
  virtual void MousePressedOnButton(views::View* view,
                                    const views::MouseEvent& event) = 0;

  // Invoked when the mouse is dragged over a view.
  virtual void MouseDraggedOnButton(views::View* view,
                                    const views::MouseEvent& event) = 0;

  // Invoked either if the mouse is released or mouse capture canceled.
  virtual void MouseReleasedOnButton(views::View* view,
                                     bool canceled) = 0;

  // Invoked when the mouse exits the item.
  virtual void MouseExitedButton(views::View* view) = 0;

  virtual ShelfAlignment GetShelfAlignment() const = 0;

  // Invoked to get the accessible name of the item.
  virtual string16 GetAccessibleName(const views::View* view) = 0;

 protected:
  virtual ~LauncherButtonHost() {}
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_LAUNCHER_LAUNCHER_BUTTON_HOST_H_
