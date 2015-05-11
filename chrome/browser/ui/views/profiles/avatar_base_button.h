// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_BASE_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_BASE_BUTTON_H_

#include "chrome/browser/profiles/profile_info_cache_observer.h"

class Browser;

// This view manages the button that sits in the top of the window frame and
// displays the active profile's info when using multi-profiles.
class AvatarBaseButton : public ProfileInfoCacheObserver {
 public:
  explicit AvatarBaseButton(Browser* browser);
  ~AvatarBaseButton() override;

 protected:
  Browser* browser() const { return browser_; }

  // Called when the profile info cache has changed, which means we might
  // have to update the icon/text of the button.
  virtual void Update() = 0;

 private:
  // ProfileInfoCacheObserver:
  void OnProfileAdded(const base::FilePath& profile_path) override;
  void OnProfileWasRemoved(const base::FilePath& profile_path,
                           const base::string16& profile_name) override;
  void OnProfileNameChanged(const base::FilePath& profile_path,
                            const base::string16& old_profile_name) override;
  void OnProfileAvatarChanged(const base::FilePath& profile_path) override;
  void OnProfileSupervisedUserIdChanged(
      const base::FilePath& profile_path) override;

  Browser* browser_;

  DISALLOW_COPY_AND_ASSIGN(AvatarBaseButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_BASE_BUTTON_H_
