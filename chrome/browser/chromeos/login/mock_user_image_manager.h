// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_USER_IMAGE_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_USER_IMAGE_MANAGER_H_

#include <string>

#include "base/files/file_path.h"
#include "chrome/browser/chromeos/login/user_image.h"
#include "chrome/browser/chromeos/login/user_image_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockUserImageManager : public UserImageManager {
 public:
  MockUserImageManager();
  virtual ~MockUserImageManager();

  MOCK_METHOD2(SaveUserDefaultImageIndex, void(const std::string&, int));
  MOCK_METHOD2(SaveUserImage, void(const std::string&, const UserImage&));
  MOCK_METHOD2(SaveUserImageFromFile, void(const std::string&,
                                           const base::FilePath&));
  MOCK_METHOD1(SaveUserImageFromProfileImage, void(const std::string&));
  MOCK_METHOD1(DownloadProfileImage, void(const std::string&));
  MOCK_CONST_METHOD0(DownloadedProfileImage, const gfx::ImageSkia& (void));

};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_USER_IMAGE_MANAGER_H_
