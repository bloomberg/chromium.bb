// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_FRAME_CAPTION_BUTTON_CONTAINER_VIEW_H_
#define ASH_WM_WORKSPACE_FRAME_CAPTION_BUTTON_CONTAINER_VIEW_H_

#include "ash/ash_export.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace views {
class ImageButton;
class NonClientFrameView;
class Widget;
}

namespace ash {

// Container view for the frame caption buttons. It performs the appropriate
// action when a caption button is clicked.
class ASH_EXPORT FrameCaptionButtonContainerView
    : public views::View,
      public views::ButtonListener {
 public:
  static const char kViewClassName[];

  // |frame_view| and |frame| are the NonClientFrameView and the views::Widget
  // that the caption buttons act on.
  // TODO(pkotwicz): Remove the |frame_view| parameter once FrameMaximizeButton
  // is refactored to take in a views::Widget instead.
  FrameCaptionButtonContainerView(views::NonClientFrameView* frame_view,
                                  views::Widget* frame);
  virtual ~FrameCaptionButtonContainerView();

  // For testing.
  class TestApi {
   public:
    explicit TestApi(FrameCaptionButtonContainerView* container_view)
        : container_view_(container_view) {
    }

    views::ImageButton* size_button() const {
      return container_view_->size_button_;
    }

    views::ImageButton* close_button() const {
      return container_view_->close_button_;
    }

   private:
    FrameCaptionButtonContainerView* container_view_;

    DISALLOW_COPY_AND_ASSIGN(TestApi);
  };

  // Tell the window controls to reset themselves to the normal state.
  void ResetWindowControls();

  // views::View overrides:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual const char* GetClassName() const OVERRIDE;

 private:
  // Sets the images for a button based on the given ids.
  void SetButtonImages(views::ImageButton* button,
                       int normal_image_id,
                       int hot_image_id,
                       int pushed_image_id);

  // views::ButtonListener override:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // The widget that the buttons act on.
  views::Widget* frame_;

  // The buttons.
  views::ImageButton* size_button_;
  views::ImageButton* close_button_;

  DISALLOW_COPY_AND_ASSIGN(FrameCaptionButtonContainerView);
};

}  // namesapace ash

#endif  // ASH_WM_WORKSPACE_FRAME_CAPTION_BUTTON_CONTAINER_VIEW_H_
