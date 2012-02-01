// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LAUNCHER_APP_LAUNCHER_BUTTON_H_
#define ASH_LAUNCHER_APP_LAUNCHER_BUTTON_H_
#pragma once

#include "ui/views/controls/button/image_button.h"

namespace ash {
namespace internal {

class LauncherButtonHost;

// Button used for items on the launcher corresponding to an app window as
// well as one of the preset types (TYPE_APP_LIST or TYPE_BROWSER_SHORTCUT).
class AppLauncherButton : public views::ImageButton {
 public:
  AppLauncherButton(views::ButtonListener* listener,
                    LauncherButtonHost* host);
  virtual ~AppLauncherButton();

  // Sets the image to display for this entry.
  void SetAppImage(const SkBitmap& image);

 protected:
  // View overrides:
  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseCaptureLost() OVERRIDE;
  virtual bool OnMouseDragged(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const views::MouseEvent& event) OVERRIDE;

 private:
  LauncherButtonHost* host_;

  DISALLOW_COPY_AND_ASSIGN(AppLauncherButton);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_LAUNCHER_APP_LAUNCHER_BUTTON_H_
