// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_ITEM_VIEW_H_
#define ASH_SYSTEM_TRAY_TRAY_ITEM_VIEW_H_
#pragma once

#include "ui/base/animation/animation_delegate.h"
#include "ui/views/view.h"

namespace ui {
class SlideAnimation;
}

namespace views {
class ImageView;
class Label;
}

namespace ash {
namespace internal {

// Base-class for items in the tray. It makes sure the widget is updated
// correctly when the visibility/size of the tray item changes. It also adds
// animation when showing/hiding the item in the tray.
class TrayItemView : public views::View,
                     public ui::AnimationDelegate {
 public:
  TrayItemView();
  virtual ~TrayItemView();

  // Conveniece function for creating a child Label or ImageView.
  void CreateLabel();
  void CreateImageView();

  views::Label* label() { return label_; }
  views::ImageView* image_view() { return image_view_; }

  // Overridden from views::View.
  virtual void SetVisible(bool visible) OVERRIDE;

 protected:
  // Makes sure the widget relayouts after the size/visibility of the view
  // changes.
  void ApplyChange();

  // This should return the desired size of the view. For most views, this
  // returns GetPreferredSize. But since this class overrides GetPreferredSize
  // for animation purposes, we allow a different way to get this size, and do
  // not allow GetPreferredSize to be overridden.
  virtual gfx::Size DesiredSize();

  // The default animation duration is 200ms. But each view can customize this.
  virtual int GetAnimationDurationMS();

 private:
  // Overridden from views::View.
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void ChildPreferredSizeChanged(View* child) OVERRIDE;

  // Overridden from ui::AnimationDelegate.
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;
  virtual void AnimationEnded(const ui::Animation* animation) OVERRIDE;
  virtual void AnimationCanceled(const ui::Animation* animation) OVERRIDE;

  scoped_ptr<ui::SlideAnimation> animation_;
  views::Label* label_;
  views::ImageView* image_view_;

  DISALLOW_COPY_AND_ASSIGN(TrayItemView);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_ITEM_VIEW_H_
