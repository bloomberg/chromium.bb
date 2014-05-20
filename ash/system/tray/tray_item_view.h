// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_ITEM_VIEW_H_
#define ASH_SYSTEM_TRAY_TRAY_ITEM_VIEW_H_

#include "ash/ash_export.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/views/view.h"

namespace gfx {
class SlideAnimation;
}

namespace views {
class ImageView;
class Label;
}

namespace ash {
class SystemTrayItem;

// Base-class for items in the tray. It makes sure the widget is updated
// correctly when the visibility/size of the tray item changes. It also adds
// animation when showing/hiding the item in the tray.
class ASH_EXPORT TrayItemView : public views::View,
                                public gfx::AnimationDelegate {
 public:
  explicit TrayItemView(SystemTrayItem* owner);
  virtual ~TrayItemView();

  static void DisableAnimationsForTest();

  // Convenience function for creating a child Label or ImageView.
  void CreateLabel();
  void CreateImageView();

  SystemTrayItem* owner() const { return owner_; }
  views::Label* label() const { return label_; }
  views::ImageView* image_view() const { return image_view_; }

  // Overridden from views::View.
  virtual void SetVisible(bool visible) OVERRIDE;
  virtual gfx::Size GetPreferredSize() const OVERRIDE;
  virtual int GetHeightForWidth(int width) const OVERRIDE;

 protected:
  // Makes sure the widget relayouts after the size/visibility of the view
  // changes.
  void ApplyChange();

  // This should return the desired size of the view. For most views, this
  // returns GetPreferredSize. But since this class overrides GetPreferredSize
  // for animation purposes, we allow a different way to get this size, and do
  // not allow GetPreferredSize to be overridden.
  virtual gfx::Size DesiredSize() const;

  // The default animation duration is 200ms. But each view can customize this.
  virtual int GetAnimationDurationMS();

 private:
  // Overridden from views::View.
  virtual void ChildPreferredSizeChanged(View* child) OVERRIDE;

  // Overridden from gfx::AnimationDelegate.
  virtual void AnimationProgressed(const gfx::Animation* animation) OVERRIDE;
  virtual void AnimationEnded(const gfx::Animation* animation) OVERRIDE;
  virtual void AnimationCanceled(const gfx::Animation* animation) OVERRIDE;

  SystemTrayItem* owner_;
  scoped_ptr<gfx::SlideAnimation> animation_;
  views::Label* label_;
  views::ImageView* image_view_;

  DISALLOW_COPY_AND_ASSIGN(TrayItemView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_ITEM_VIEW_H_
