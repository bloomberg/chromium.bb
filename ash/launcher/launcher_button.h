// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LAUNCHER_LAUNCHER_BUTTON_H_
#define ASH_LAUNCHER_LAUNCHER_BUTTON_H_
#pragma once

#include "ui/views/controls/button/custom_button.h"
#include "ui/views/controls/image_view.h"

namespace ash {
namespace internal {

class LauncherButtonHost;

// Button used for items on the launcher, except for the AppList.
class LauncherButton : public views::CustomButton {
 public:
  // Used to indicate the current state of the button.
  enum State {
    STATE_NORMAL    = 0,
    STATE_HOVERED   = 1 << 0,
    STATE_RUNNING   = 1 << 1,
    STATE_ACTIVE    = 1 << 2,
    STATE_ATTENTION = 1 << 3,
  };

  virtual ~LauncherButton();

  // Called to create an instance of a LauncherButton.
  static LauncherButton* Create(views::ButtonListener* listener,
                                LauncherButtonHost* host);

  // Sets the image to display for this entry.
  void SetImage(const SkBitmap& image);

  // |state| is or'd into the current state.
  void AddState(State state);
  void ClearState(State state);
  int state() const { return state_; }

  // Returns the bounds of the icon.
  gfx::Rect GetIconBounds() const;

 protected:
  LauncherButton(views::ButtonListener* listener, LauncherButtonHost* host);

  // Class that draws the icon part of a button, so it can be animated
  // independently of the rest. This can be subclassed to provide a custom
  // implementation, by overriding CreateIconView().
  class IconView : public views::ImageView {
   public:
    IconView();
    virtual ~IconView();
    virtual bool HitTest(const gfx::Point& l) const OVERRIDE;
    void set_icon_size(int icon_size) { icon_size_ = icon_size; }
    int icon_size() const { return icon_size_; }

   private:
    // Set to non-zero to force icons to be resized to fit within a square,
    // while maintaining original proportions.
    int icon_size_;

    DISALLOW_COPY_AND_ASSIGN(IconView);
  };

  // View overrides:
  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseCaptureLost() OVERRIDE;
  virtual bool OnMouseDragged(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseEntered(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const views::MouseEvent& event) OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual bool GetTooltipText(const gfx::Point& p,
                              string16* tooltip) const OVERRIDE;

  // Sets the icon image with a shadow.
  void SetShadowedImage(const SkBitmap& bitmap);
  // Override for custom initialization.
  virtual void Init();
  // Override to subclass IconView.
  virtual IconView* CreateIconView();
  IconView* icon_view() const { return icon_view_; }
  LauncherButtonHost* host() const { return host_; }

 private:
  class BarView;
  // Updates the parts of the button to reflect the current state_. This may
  // add or remove views, layout and paint.
  void UpdateState();

  LauncherButtonHost* host_;
  IconView* icon_view_;
  // Draws a bar underneath the image to represent the state of the application.
  BarView* bar_;
  // The current state of the application, multiple values of AppState are or'd
  // together.
  int state_;

  DISALLOW_COPY_AND_ASSIGN(LauncherButton);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_LAUNCHER_APP_LAUNCHER_BUTTON_H_
