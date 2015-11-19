// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_WM_FRAME_CAPTION_BUTTONS_FRAME_CAPTION_BUTTON_CONTAINER_VIEW_H_
#define MASH_WM_FRAME_CAPTION_BUTTONS_FRAME_CAPTION_BUTTON_CONTAINER_VIEW_H_

#include <map>

#include "base/macros.h"
#include "mash/wm/frame/caption_buttons/caption_button_types.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace gfx {
class SlideAnimation;
}

namespace views {
class Widget;
}

namespace mash {
namespace wm {

class FrameCaptionButton;

// Container view for the frame caption buttons. It performs the appropriate
// action when a caption button is clicked.
class FrameCaptionButtonContainerView : public views::View,
                                        public views::ButtonListener,
                                        public gfx::AnimationDelegate {
 public:
  enum Animate { ANIMATE_YES, ANIMATE_NO };

  static const char kViewClassName[];

  // |frame| is the views::Widget that the caption buttons act on.
  explicit FrameCaptionButtonContainerView(views::Widget* frame);
  ~FrameCaptionButtonContainerView() override;

  // Sets the resource ids of the images to paint the button for |icon|. The
  // FrameCaptionButtonContainerView will keep track of the images to use for
  // |icon| even if none of the buttons currently use |icon|.
  void SetButtonImages(CaptionButtonIcon icon,
                       int icon_image_id,
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
  gfx::Size GetPreferredSize() const override;
  void Layout() override;
  const char* GetClassName() const override;

  // Overridden from gfx::AnimationDelegate:
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;

 private:
  friend class FrameCaptionButtonContainerViewTest;

  struct ButtonIconIds {
    ButtonIconIds();
    ButtonIconIds(int icon_id,
                  int hovered_background_id,
                  int pressed_background_id);
    ~ButtonIconIds();

    int icon_image_id;
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

  void SetButtonsToNormal(Animate animate);
  void SetButtonIcons(CaptionButtonIcon minimize_button_icon,
                      CaptionButtonIcon close_button_icon,
                      Animate animate);

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

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

}  // namespace wm
}  // namespace mash

#endif  // MASH_WM_FRAME_CAPTION_BUTTONS_FRAME_CAPTION_BUTTON_CONTAINER_VIEW_H_
