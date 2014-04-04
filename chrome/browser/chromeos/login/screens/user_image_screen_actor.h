// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_USER_IMAGE_SCREEN_ACTOR_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_USER_IMAGE_SCREEN_ACTOR_H_

#include <string>

class SkBitmap;

namespace gfx {
class ImageSkia;
}

namespace chromeos {

// Interface for dependency injection between UserImageScreen and its actual
// representation, either views based or WebUI.
class UserImageScreenActor {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

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
    virtual void OnActorDestroyed(UserImageScreenActor* actor) = 0;

    virtual bool profile_picture_absent() = 0;
    virtual int selected_image() = 0;
    virtual std::string profile_picture_data_url() = 0;

  };

  virtual ~UserImageScreenActor() {}

  // Sets screen this actor belongs to.
  virtual void SetDelegate(Delegate* screen) = 0;

  // Prepare the contents to showing.
  virtual void PrepareToShow() = 0;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Hides the contents of the screen.
  virtual void Hide() = 0;

  // Selects image with the index specified.
  virtual void SelectImage(int index) = 0;

  // Sends profile image as a data URL to the page.
  virtual void SendProfileImage(const std::string& data_url) = 0;

  // Indicates that there is no custom profile image for the user.
  virtual void OnProfileImageAbsent() = 0;

  // Enables or disables profile picture.
  virtual void SetProfilePictureEnabled(bool enabled) = 0;

  // Sends result of camera check
  virtual void SetCameraPresent(bool enabled) = 0;

  // Hides curtain with spinner.
  virtual void HideCurtain() = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_USER_IMAGE_SCREEN_ACTOR_H_
