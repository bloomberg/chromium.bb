// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_TAKE_PHOTO_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_TAKE_PHOTO_VIEW_H_
#pragma once

#include "base/compiler_specific.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

class PhotoCaptureObserver;
class SkBitmap;

namespace views {
class ImageButton;
class Label;
}  // namespace views

namespace chromeos {

class CameraImageView;

// View used for showing video from camera and taking a snapshot from it.
class TakePhotoView : public views::View,
                      public views::ButtonListener {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Called when the view has switched to capturing state.
    virtual void OnCapturingStarted() = 0;

    // Called when the view has switched from capturing state.
    virtual void OnCapturingStopped() = 0;
  };

  explicit TakePhotoView(Delegate* delegate);
  virtual ~TakePhotoView();

  // Initializes this view, its children and layout.
  void Init();

  // Updates image from camera.
  void UpdateVideoFrame(const SkBitmap& frame);

  // If in capturing mode, shows that camera is initializing by running
  // throbber above the picture. Disables snapshot button until frame is
  // received.
  void ShowCameraInitializing();

  // If in capturing mode, shows that camera is broken instead of video
  // frame and disables snapshot button until new frame is received.
  void ShowCameraError();

  // Returns the currently selected image.
  const SkBitmap& GetImage() const;

  // Sets the image indicating that the view is used only for image preview.
  void SetImage(SkBitmap* image);

  // Captures the image, as if the button was pressed.
  void CaptureImage();

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  // Overridden from views::ButtonListener.
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  bool is_capturing() const { return is_capturing_; }

  void set_show_title(bool show) { show_title_ = show; }

 private:
  // Initializes layout manager for this view.
  void InitLayout();

  // For automation purposes.
  friend class ::PhotoCaptureObserver;
  void FlipCapturingState();

  views::Label* title_label_;
  views::ImageButton* snapshot_button_;
  CameraImageView* user_image_;

  // Indicates that we're in capturing mode. When |false|, new video frames
  // are not shown to user if received.
  bool is_capturing_;

  // Whether title label is present or not.
  bool show_title_;

  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(TakePhotoView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_TAKE_PHOTO_VIEW_H_
