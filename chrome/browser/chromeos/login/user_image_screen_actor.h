// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USER_IMAGE_SCREEN_ACTOR_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USER_IMAGE_SCREEN_ACTOR_H_

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

    // Called when user accepts photo as login user image.
    virtual void OnPhotoTaken(const gfx::ImageSkia& image) = 0;
    // Called when user accepts Profile image as login user image.
    virtual void OnProfileImageSelected() = 0;
    // Called when user accepts one of the default images as login user
    // image.
    virtual void OnDefaultImageSelected(int index) = 0;
    // Called when actor is destroyed so there's no dead reference to it.
    virtual void OnActorDestroyed(UserImageScreenActor* actor) = 0;
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

  // Starts camera presence check.
  virtual void CheckCameraPresence() = 0;

  // Inserts profile image in the list for user to select.
  virtual void AddProfileImage(const gfx::ImageSkia& image) {}

  // Indicates that there is no custom profile image for the user.
  virtual void OnProfileImageAbsent() {}
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USER_IMAGE_SCREEN_ACTOR_H_
