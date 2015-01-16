// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_USER_IMAGE_MODEL_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_USER_IMAGE_MODEL_H_

#include <string>

#include "chrome/browser/chromeos/login/screens/base_screen.h"

namespace chromeos {

class BaseScreenDelegate;
class UserImageView;

class UserImageModel : public BaseScreen {
 public:
  static const char kContextKeyIsCameraPresent[];
  static const char kContextKeySelectedImageURL[];
  static const char kContextKeyProfilePictureDataURL[];

  explicit UserImageModel(BaseScreenDelegate* base_screen_delegate);
  ~UserImageModel() override;

  // BaseScreen implementation:
  std::string GetName() const override;

  // Called when UI ready to be shown.
  virtual void OnScreenReady() = 0;

  // Called when user accepts photo as login user image.
  virtual void OnPhotoTaken(const std::string& raw_data) = 0;

  // Called when some image was selected. |is_user_selection| indicates if
  // it was user selection or image was selected programmatically.
  virtual void OnImageSelected(const std::string& image_url,
                               const std::string& image_type,
                               bool is_user_selection) = 0;

  // Called when user accepts currently selected image
  virtual void OnImageAccepted() = 0;

  // Called when actor is destroyed so there's no dead reference to it.
  virtual void OnViewDestroyed(UserImageView* view) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_USER_IMAGE_MODEL_H_
