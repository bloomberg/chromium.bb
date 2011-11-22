// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_INFO_INTERFACE_H_
#define CHROME_BROWSER_PROFILES_PROFILE_INFO_INTERFACE_H_

#include "base/file_path.h"
#include "base/string16.h"

namespace gfx {
class Image;
}

// This abstract interface is used to query the profiles backend for information
// about the different profiles. Its sole concrete implementation is the
// ProfileInfoCache. This interface exists largely to assist in testing.
class ProfileInfoInterface {
 public:
  virtual size_t GetNumberOfProfiles() const = 0;

  virtual size_t GetIndexOfProfileWithPath(
      const FilePath& profile_path) const = 0;

  virtual string16 GetNameOfProfileAtIndex(size_t index) const = 0;

  virtual FilePath GetPathOfProfileAtIndex(size_t index) const = 0;

  virtual string16 GetUserNameOfProfileAtIndex(size_t index) const = 0;

  virtual const gfx::Image& GetAvatarIconOfProfileAtIndex(
      size_t index) const = 0;

  // Returns true if the profile at the given index is currently running any
  // background apps.
  virtual bool GetBackgroundStatusOfProfileAtIndex(
      size_t index) const = 0;

  virtual string16 GetGAIANameOfProfileAtIndex(size_t index) const = 0;

  // Checks if the GAIA name should be used as the profile's name.
  virtual bool IsUsingGAIANameOfProfileAtIndex(size_t index) const = 0;

  virtual const gfx::Image& GetGAIAPictureOfProfileAtIndex(
      size_t index) const = 0;

  // Checks if the GAIA picture should be used as the profile's avatar icon.
  virtual bool IsUsingGAIAPictureOfProfileAtIndex(size_t index) const = 0;

 protected:
  virtual ~ProfileInfoInterface() {}
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_INFO_INTERFACE_H_
