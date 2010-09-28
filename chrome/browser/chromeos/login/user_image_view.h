// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USER_IMAGE_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USER_IMAGE_VIEW_H_
#pragma once

#include "views/controls/button/button.h"
#include "views/view.h"

class SkBitmap;

namespace views {
class ImageButton;
class ImageView;
class Label;
class NativeButton;
}  // namespace views

namespace chromeos {

// View used for selecting user image.
class UserImageView : public views::View,
                      public views::ButtonListener {
 public:
  // Delegate class to get notifications from the view.
  class Delegate {
  public:
    virtual ~Delegate() {}

    // Called if user accepts the selected image. The image is passed as a
    // parameter.
    virtual void OnOK(const SkBitmap& image) = 0;

    // Called if user decides to skip image selection screen.
    virtual void OnSkip() = 0;
  };

  explicit UserImageView(Delegate* delegate);
  virtual ~UserImageView();

  // Initializes this view, its children and layout.
  void Init();

  // Updates image from camera.
  void UpdateVideoFrame(const SkBitmap& frame);

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize();

  // Overridden from views::ButtonListener.
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

 protected:
  // views::View overrides:
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child);

 private:
  // Initializes layout manager for this view.
  void InitLayout();

  views::Label* title_label_;
  views::NativeButton* ok_button_;
  views::NativeButton* skip_button_;
  views::ImageButton* snapshot_button_;
  views::ImageView* user_image_;

  // Notifications receiver.
  Delegate* delegate_;

  // Indicates that we're in capturing mode. When |false|, new video frames
  // are not shown to user if received.
  bool is_capturing_;

  // Last frame that was received from the camera in its original resolution.
  scoped_ptr<SkBitmap> last_frame_;

  DISALLOW_COPY_AND_ASSIGN(UserImageView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USER_IMAGE_VIEW_H_

