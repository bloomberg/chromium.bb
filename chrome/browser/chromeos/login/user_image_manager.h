// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USER_IMAGE_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USER_IMAGE_MANAGER_H_

#include <string>

#include "chrome/browser/chromeos/login/user.h"

class FilePath;
class PrefServiceSimple;

namespace gfx {
class ImageSkia;
}

namespace chromeos {

class UserImage;

// Base class that provides a mechanism for updating user images.
class UserImageManager {
 public:
  // Registers user image manager preferences.
  static void RegisterPrefs(PrefServiceSimple* local_state);

  virtual ~UserImageManager();

  // Loads user image data from Local State.
  virtual void LoadUserImages(const UserList& users) = 0;

  // Indicates that a user with the given |email| has just logged in.
  virtual void UserLoggedIn(const std::string& email,
                            bool user_is_new,
                            bool user_is_local) = 0;

  // Sets user image to the default image with index |image_index|, sends
  // LOGIN_USER_IMAGE_CHANGED notification and updates Local State.
  virtual void SaveUserDefaultImageIndex(const std::string& username,
                                         int image_index) = 0;

  // Saves image to file, sends LOGIN_USER_IMAGE_CHANGED notification and
  // updates Local State.
  virtual void SaveUserImage(const std::string& username,
                             const UserImage& user_image) = 0;

  // Tries to load user image from disk; if successful, sets it for the user,
  // sends LOGIN_USER_IMAGE_CHANGED notification and updates Local State.
  virtual void SaveUserImageFromFile(const std::string& username,
                                     const FilePath& path) = 0;

  // Sets profile image as user image for |username|, sends
  // LOGIN_USER_IMAGE_CHANGED notification and updates Local State. If the user
  // is not logged-in or the last |DownloadProfileImage| call has failed, a
  // default grey avatar will be used until the user logs in and profile image
  // is downloaded successfully.
  virtual void SaveUserImageFromProfileImage(const std::string& username) = 0;

  // Deletes user image and the corresponding image file.
  virtual void DeleteUserImage(const std::string& username) = 0;

  // Starts downloading the profile image for the logged-in user.
  // If user's image index is |kProfileImageIndex|, newly downloaded image
  // is immediately set as user's current picture.
  // |reason| is an arbitrary string (used to report UMA histograms with
  // download times).
  virtual void DownloadProfileImage(const std::string& reason) = 0;

  // Returns the result of the last successful profile image download, if any.
  // Otherwise, returns an empty bitmap.
  virtual const gfx::ImageSkia& DownloadedProfileImage() const = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USER_IMAGE_MANAGER_H_
