// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LAUNCHER_APP_LIST_BUTTON_H_
#define ASH_LAUNCHER_APP_LIST_BUTTON_H_
#pragma once

#include "ui/views/controls/button/image_button.h"

namespace ash {
namespace internal {

class LauncherButtonHost;

// Button used for the AppList icon on the launcher.
class AppListButton : public views::ImageButton {
 public:
  AppListButton(views::ButtonListener* listener,
                LauncherButtonHost* host);
  virtual ~AppListButton();

 protected:
  // View overrides:
  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseCaptureLost() OVERRIDE;
  virtual bool OnMouseDragged(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseMoved(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseEntered(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const views::MouseEvent& event) OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;

 private:
  LauncherButtonHost* host_;

  DISALLOW_COPY_AND_ASSIGN(AppListButton);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_LAUNCHER_LAUNCHER_BUTTON_H_
