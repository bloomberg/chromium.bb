// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LAUNCHER_TABBED_LAUNCHER_BUTTON_H_
#define ASH_LAUNCHER_TABBED_LAUNCHER_BUTTON_H_
#pragma once

#include "ash/launcher/launcher_button.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/glow_hover_controller.h"

namespace ui {
class MultiAnimation;
}

namespace ash {

struct LauncherItem;

namespace internal {

// Button used for items on the launcher corresponding to tabbed windows.
class TabbedLauncherButton : public LauncherButton {
 public:
  // Indicates if this button is incognito or not.
  enum IncognitoState {
    STATE_INCOGNITO,
    STATE_NOT_INCOGNITO,
  };

  static TabbedLauncherButton* Create(views::ButtonListener* listener,
                                      LauncherButtonHost* host,
                                      IncognitoState is_incognito);
  virtual ~TabbedLauncherButton();

  // Sets the images to display for this entry.
  void SetTabImage(const SkBitmap& image);

  // This only defines how the icon is drawn. Do not use it for other purposes.
  IncognitoState is_incognito() const { return is_incognito_; }

 protected:
  TabbedLauncherButton(views::ButtonListener* listener,
                       LauncherButtonHost* host,
                       IncognitoState is_incognito);
  // View override.
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;

  // LauncherButton override.
  virtual IconView* CreateIconView() OVERRIDE;

 private:
  // Used as the delegate for |animation_|.
  class IconView : public LauncherButton::IconView,
                   public ui::AnimationDelegate {
   public:
    explicit IconView(TabbedLauncherButton* host);
    virtual ~IconView();

    // ui::AnimationDelegateImpl overrides:
    virtual void AnimationEnded(const ui::Animation* animation) OVERRIDE;
    virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;

    // Sets the image to display for this entry.
    void SetTabImage(const SkBitmap& image);

   protected:
    // View override.
    virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

   private:
    TabbedLauncherButton* host_;
    SkBitmap image_;
    SkBitmap animating_image_;

    // Used to animate image.
    scoped_ptr<ui::MultiAnimation> animation_;

    // Background images. Which one is chosen depends on the type of the window.
    static SkBitmap* browser_image_;
    static SkBitmap* incognito_browser_image_;
    // TODO[dave] implement panel specific image.
    static SkBitmap* browser_panel_image_;
    static SkBitmap* incognito_browser_panel_image_;

    DISALLOW_COPY_AND_ASSIGN(IconView);
  };

  IconView* tabbed_icon_view() {
    return static_cast<IconView*>(icon_view());
  }

  // Indicates how the icon is drawn. If true an Incognito symbol will be
  // drawn. It does not necessarily indicate if the window is 'incognito'.
  const IncognitoState is_incognito_;

  DISALLOW_COPY_AND_ASSIGN(TabbedLauncherButton);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_LAUNCHER_TABBED_LAUNCHER_BUTTON_H_
