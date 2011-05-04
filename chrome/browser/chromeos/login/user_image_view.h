// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USER_IMAGE_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USER_IMAGE_VIEW_H_
#pragma once

#include "chrome/browser/chromeos/login/default_images_view.h"
#include "chrome/browser/chromeos/login/take_photo_view.h"
#include "views/controls/button/button.h"
#include "views/view.h"

class SkBitmap;

namespace views {
class Label;
class NativeButton;
}  // namespace views

namespace chromeos {

// View used for selecting user image on OOBE screen.
class UserImageView : public views::View,
                      public views::ButtonListener,
                      public TakePhotoView::Delegate,
                      public DefaultImagesView::Delegate {
 public:
  // Delegate class to get notifications from the view.
  class Delegate {
  public:
    virtual ~Delegate() {}

    // Initializes camera if needed and starts capturing.
    virtual void StartCamera() = 0;

    // Stops capturing from camera.
    virtual void StopCamera() = 0;

    // Called if user accepts the taken photo. The image is passed as a
    // parameter.
    virtual void OnPhotoTaken(const SkBitmap& image) = 0;

    // Called if user accepts the chosen default image. The image index is
    // passed as a parameter.
    virtual void OnDefaultImageSelected(int index) = 0;
  };

  explicit UserImageView(Delegate* delegate);
  virtual ~UserImageView();

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

  // Returns true if video capture is the current state of the view.
  bool IsCapturing() const;

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize();

  // Overridden from views::ButtonListener.
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // Overridden from TakePhotoView::Delegate.
  virtual void OnCapturingStarted();
  virtual void OnCapturingStopped();

  // Overridden from DefaultImagesView::Delegate.
  virtual void OnCaptureButtonClicked();
  virtual void OnImageSelected(int image_index);

 private:
  // Initializes layout manager for this view.
  void InitLayout();

  views::Label* title_label_;
  DefaultImagesView* default_images_view_;
  TakePhotoView* take_photo_view_;
  views::View* splitter_;
  views::NativeButton* ok_button_;

  // Notifications receiver.
  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(UserImageView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USER_IMAGE_VIEW_H_
