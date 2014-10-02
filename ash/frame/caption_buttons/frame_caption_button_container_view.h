// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FRAME_CAPTION_BUTTONS_FRAME_CAPTION_BUTTON_CONTAINER_VIEW_H_
#define ASH_FRAME_CAPTION_BUTTONS_FRAME_CAPTION_BUTTON_CONTAINER_VIEW_H_

#include <map>

#include "ash/ash_export.h"
#include "ash/frame/caption_buttons/frame_size_button_delegate.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace gfx {
class SlideAnimation;
}

namespace views {
class Widget;
}

namespace ash {

// Container view for the frame caption buttons. It performs the appropriate
// action when a caption button is clicked.
class ASH_EXPORT FrameCaptionButtonContainerView
    : public views::View,
      public views::ButtonListener,
      public FrameSizeButtonDelegate,
      public gfx::AnimationDelegate {
 public:
  static const char kViewClassName[];

  // Whether the frame can be minimized (either via the maximize/restore button
  // or via a dedicated button).
  enum MinimizeAllowed {
    MINIMIZE_ALLOWED,
    MINIMIZE_DISALLOWED
  };

  // |frame| is the views::Widget that the caption buttons act on.
  // |minimize_allowed| indicates whether the frame can be minimized (either via
  // the maximize/restore button or via a dedicated button).
  FrameCaptionButtonContainerView(views::Widget* frame,
                                  MinimizeAllowed minimize_allowed);
  virtual ~FrameCaptionButtonContainerView();

  // For testing.
  class ASH_EXPORT TestApi {
   public:
    explicit TestApi(FrameCaptionButtonContainerView* container_view)
        : container_view_(container_view) {
    }

    void EndAnimations();

    FrameCaptionButton* minimize_button() const {
      return container_view_->minimize_button_;
    }

    FrameCaptionButton* size_button() const {
      return container_view_->size_button_;
    }

    FrameCaptionButton* close_button() const {
      return container_view_->close_button_;
    }

   private:
    FrameCaptionButtonContainerView* container_view_;

    DISALLOW_COPY_AND_ASSIGN(TestApi);
  };

  // Sets the resource ids of the images to paint the button for |icon|. The
  // FrameCaptionButtonContainerView will keep track of the images to use for
  // |icon| even if none of the buttons currently use |icon|.
  void SetButtonImages(CaptionButtonIcon icon,
                       int icon_image_id,
                       int inactive_icon_image_id,
                       int hovered_background_image_id,
                       int pressed_background_image_id);

  // Sets whether the buttons should be painted as active. Does not schedule
  // a repaint.
  void SetPaintAsActive(bool paint_as_active);

  // Tell the window controls to reset themselves to the normal state.
  void ResetWindowControls();

  // Determines the window HT* code for the caption button at |point|. Returns
  // HTNOWHERE if |point| is not over any of the caption buttons. |point| must
  // be in the coordinates of the FrameCaptionButtonContainerView.
  int NonClientHitTest(const gfx::Point& point) const;

  // Updates the size button's visibility based on whether |frame_| can be
  // maximized and if maximize mode is enabled. A parent view should relayout
  // to reflect the change in visibility.
  void UpdateSizeButtonVisibility();

  // views::View:
  virtual gfx::Size GetPreferredSize() const OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual const char* GetClassName() const OVERRIDE;

  // Overridden from gfx::AnimationDelegate:
  virtual void AnimationEnded(const gfx::Animation* animation) OVERRIDE;
  virtual void AnimationProgressed(const gfx::Animation* animation) OVERRIDE;

 private:
  friend class FrameCaptionButtonContainerViewTest;

  struct ButtonIconIds {
    ButtonIconIds();
    ButtonIconIds(int icon_id,
                  int inactive_icon_id,
                  int hovered_background_id,
                  int pressed_background_id);
    ~ButtonIconIds();

    int icon_image_id;
    int inactive_icon_image_id;
    int hovered_background_image_id;
    int pressed_background_image_id;
  };

  // Sets |button|'s icon to |icon|. If |animate| is ANIMATE_YES, the button
  // will crossfade to the new icon. If |animate| is ANIMATE_NO and
  // |icon| == |button|->icon(), the crossfade animation is progressed to the
  // end.
  void SetButtonIcon(FrameCaptionButton* button,
                     CaptionButtonIcon icon,
                     Animate animate);

  // Returns true if maximize mode is not enabled, and |frame_| widget delegate
  // can be maximized.
  bool ShouldSizeButtonBeVisible() const;

  // views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // FrameSizeButtonDelegate:
  virtual bool IsMinimizeButtonVisible() const OVERRIDE;
  virtual void SetButtonsToNormal(Animate animate) OVERRIDE;
  virtual void SetButtonIcons(CaptionButtonIcon minimize_button_icon,
                              CaptionButtonIcon close_button_icon,
                              Animate animate) OVERRIDE;
  virtual const FrameCaptionButton* GetButtonClosestTo(
      const gfx::Point& position_in_screen) const OVERRIDE;
  virtual void SetHoveredAndPressedButtons(
      const FrameCaptionButton* to_hover,
      const FrameCaptionButton* to_press) OVERRIDE;

  // The widget that the buttons act on.
  views::Widget* frame_;

  // The buttons. In the normal button style, at most one of |minimize_button_|
  // and |size_button_| is visible.
  FrameCaptionButton* minimize_button_;
  FrameCaptionButton* size_button_;
  FrameCaptionButton* close_button_;

  // Mapping of the images needed to paint a button for each of the values of
  // CaptionButtonIcon.
  std::map<CaptionButtonIcon, ButtonIconIds> button_icon_id_map_;

  // Animation that affects the position of |minimize_button_| and the
  // visibility of |size_button_|.
  scoped_ptr<gfx::SlideAnimation> maximize_mode_animation_;

  DISALLOW_COPY_AND_ASSIGN(FrameCaptionButtonContainerView);
};

}  // namespace ash

#endif  // ASH_FRAME_CAPTION_BUTTONS_FRAME_CAPTION_BUTTON_CONTAINER_VIEW_H_
