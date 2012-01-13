// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LAUNCHER_TABBED_LAUNCHER_BUTTON_H_
#define ASH_LAUNCHER_TABBED_LAUNCHER_BUTTON_H_
#pragma once

#include "ash/launcher/launcher_types.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/glow_hover_controller.h"

namespace ui {
class MultiAnimation;
}

namespace ash {
namespace internal {

class LauncherButtonHost;

// Button used for items on the launcher corresponding to tabbed windows.
class TabbedLauncherButton : public views::ImageButton {
 public:
  TabbedLauncherButton(views::ButtonListener* listener,
                       LauncherButtonHost* host);
  virtual ~TabbedLauncherButton();

  // Notification that the images are about to change. Kicks off an animation.
  void PrepareForImageChange();

  // Sets the images to display for this entry.
  void SetImages(const LauncherTabbedImages& images);

 protected:
  // View overrides:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseCaptureLost() OVERRIDE;
  virtual bool OnMouseDragged(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseEntered(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseMoved(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const views::MouseEvent& event) OVERRIDE;

 private:
  // Used as the delegate for |animation_|. TabbedLauncherButton doesn't
  // directly implement AnimationDelegate as one of it's superclasses already
  // does.
  class AnimationDelegateImpl : public ui::AnimationDelegate {
   public:
    explicit AnimationDelegateImpl(TabbedLauncherButton* host);
    virtual ~AnimationDelegateImpl();

    // ui::AnimationDelegateImpl overrides:
    virtual void AnimationEnded(const ui::Animation* animation) OVERRIDE;
    virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;

   private:
    TabbedLauncherButton* host_;

    DISALLOW_COPY_AND_ASSIGN(AnimationDelegateImpl);
  };

  struct ImageSet {
    SkBitmap* normal_image;
    SkBitmap* pushed_image;
    SkBitmap* hot_image;
  };

  // Creates an ImageSet using the specified image ids. Caller owns the returned
  // value.
  static ImageSet* CreateImageSet(int normal_id, int pushed_id, int hot_id);

  LauncherTabbedImages images_;

  LauncherButtonHost* host_;

  // Delegate of |animation_|.
  AnimationDelegateImpl animation_delegate_;

  // Used to animate image.
  scoped_ptr<ui::MultiAnimation> animation_;

  // Should |images_| be shown? This is set to false soon after
  // PrepareForImageChange() is invoked without a following call to SetImages().
  bool show_image_;

  // Background images. Which one is chosen depends upon how many images are
  // provided.
  static ImageSet* bg_image_1_;
  static ImageSet* bg_image_2_;
  static ImageSet* bg_image_3_;

  views::GlowHoverController hover_controller_;

  DISALLOW_COPY_AND_ASSIGN(TabbedLauncherButton);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_LAUNCHER_TABBED_LAUNCHER_BUTTON_H_
