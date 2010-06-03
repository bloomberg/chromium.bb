// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USER_IMAGE_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USER_IMAGE_VIEW_H_

#include "views/controls/button/button.h"
#include "views/view.h"

class SkBitmap;

namespace views {
class ImageView;
class Label;
class NativeButton;
class ImageButton;
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
    virtual void OnCancel() = 0;
  };

  explicit UserImageView(Delegate* delegate);
  virtual ~UserImageView();

  // Initialize view layout.
  void Init();

  // Update strings from the resources. Executed on language change.
  void UpdateLocalizedStrings();

  // Updates snapshot from camera on camera image view.
  void UpdateVideoFrame(const SkBitmap& frame);

  // Called when user left-clicks video image control.
  void OnVideoImageClicked();

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();

  // Overridden from views::ButtonListener.
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

 protected:
  // views::View overrides:
  virtual void LocaleChanged();

 private:
  // Delete and recreate native controls that fail to update preferred size
  // after string update.
  void RecreateNativeControls();

  views::Label* title_label_;
  views::NativeButton* ok_button_;
  views::NativeButton* cancel_button_;
  views::ImageButton* video_button_;
  views::ImageView* selected_image_;

  // Notifications receiver.
  Delegate* delegate_;

  // Indicates that some image was selected and OK can be pressed.
  bool image_selected_;

  // Last frame that was received from the camera in its original resolution.
  scoped_ptr<SkBitmap> last_frame_;

  DISALLOW_COPY_AND_ASSIGN(UserImageView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USER_IMAGE_VIEW_H_

