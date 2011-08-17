// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_FAKE_PROFILE_INFO_INTERFACE_H_
#define CHROME_BROWSER_PROFILES_FAKE_PROFILE_INFO_INTERFACE_H_

#include <vector>

#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/avatar_menu_model.h"
#include "chrome/browser/profiles/avatar_menu_model_observer.h"
#include "chrome/browser/profiles/profile_info_interface.h"
#include "chrome/common/chrome_notification_types.h"
#include "ui/gfx/image/image.h"

class FakeProfileInfo : public ProfileInfoInterface {
 public:
  FakeProfileInfo();
  virtual ~FakeProfileInfo();

  std::vector<AvatarMenuModel::Item*>* mock_profiles();

  static gfx::Image& GetTestImage();

  // ProfileInfoInterface:
  virtual size_t GetNumberOfProfiles() const OVERRIDE;
  virtual size_t GetIndexOfProfileWithPath(
      const FilePath& profile_path) const OVERRIDE;
  virtual string16 GetNameOfProfileAtIndex(size_t index) const OVERRIDE;
  virtual FilePath GetPathOfProfileAtIndex(size_t index) const OVERRIDE;
  virtual const gfx::Image& GetAvatarIconOfProfileAtIndex(
      size_t index) const OVERRIDE;

 private:
  std::vector<AvatarMenuModel::Item*> profiles_;
};

#endif  // CHROME_BROWSER_PROFILES_FAKE_PROFILE_INFO_INTERFACE_H_
