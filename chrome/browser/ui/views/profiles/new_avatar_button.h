// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_NEW_AVATAR_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_NEW_AVATAR_BUTTON_H_

#include "chrome/browser/profiles/profile_info_cache_observer.h"
#include "ui/views/controls/button/menu_button.h"

class Browser;

// Avatar button that displays the active profile's name in the caption area.
class NewAvatarButton : public views::MenuButton,
                        public ProfileInfoCacheObserver {
 public:
  // Different button styles that can be applied.
  enum AvatarButtonStyle {
    THEMED_BUTTON,   // Used in a themed browser window.
    NATIVE_BUTTON,    // Used in a native aero or metro window.
  };

  NewAvatarButton(views::ButtonListener* listener,
                  const base::string16& profile_name,
                  AvatarButtonStyle button_style,
                  Browser* browser);
  virtual ~NewAvatarButton();

  // views::View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

 private:
  friend class NewAvatarMenuButtonTest;
  FRIEND_TEST_ALL_PREFIXES(NewAvatarMenuButtonTest, SignOut);

  // ProfileInfoCacheObserver:
  virtual void OnProfileAdded(const base::FilePath& profile_path) OVERRIDE;
  virtual void OnProfileWasRemoved(
      const base::FilePath& profile_path,
      const base::string16& profile_name) OVERRIDE;
  virtual void OnProfileNameChanged(
      const base::FilePath& profile_path,
      const base::string16& old_profile_name) OVERRIDE;

  // Called when the profile info cache has changed, which means we might
  // have to re-display the profile name.
  void UpdateAvatarButtonAndRelayoutParent();

  Browser* browser_;

  DISALLOW_COPY_AND_ASSIGN(NewAvatarButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_NEW_AVATAR_BUTTON_H_
