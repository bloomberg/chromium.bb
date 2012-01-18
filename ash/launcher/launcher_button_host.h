// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LAUNCHER_LAUNCHER_BUTTON_HOST_H_
#define ASH_LAUNCHER_LAUNCHER_BUTTON_HOST_H_
#pragma once

namespace views {
class MouseEvent;
class View;
}

namespace ash {
namespace internal {

// The launcher buttons communicate back to the host by way of this interface.
// This interface is used to enable reordering the items on the launcher.
class LauncherButtonHost {
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

 protected:
  virtual ~LauncherButtonHost() {}
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_LAUNCHER_LAUNCHER_BUTTON_HOST_H_
