// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_NEW_AVATAR_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_NEW_AVATAR_BUTTON_H_

#include "chrome/browser/profiles/profile_info_cache_observer.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "ui/views/controls/button/menu_button.h"

class Browser;

// Avatar button that displays the active profile's name in the caption area.
class NewAvatarButton : public views::MenuButton,
                        public ProfileInfoCacheObserver,
                        public SigninErrorController::Observer {
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

  // Views::MenuButton
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const ui::MouseEvent& event) OVERRIDE;

 private:
  friend class NewAvatarMenuButtonTest;
  friend class ProfileChooserViewBrowserTest;
  FRIEND_TEST_ALL_PREFIXES(NewAvatarMenuButtonTest, SignOut);
  FRIEND_TEST_ALL_PREFIXES(ProfileChooserViewBrowserTest, ViewProfileUMA);

  // ProfileInfoCacheObserver:
  virtual void OnProfileAdded(const base::FilePath& profile_path) OVERRIDE;
  virtual void OnProfileWasRemoved(
      const base::FilePath& profile_path,
      const base::string16& profile_name) OVERRIDE;
  virtual void OnProfileNameChanged(
      const base::FilePath& profile_path,
      const base::string16& old_profile_name) OVERRIDE;
  virtual void OnProfileAvatarChanged(
      const base::FilePath& profile_path) OVERRIDE;
  virtual void OnProfileSupervisedUserIdChanged(
      const base::FilePath& profile_path) OVERRIDE;

  // SigninErrorController::Observer:
  virtual void OnErrorChanged() OVERRIDE;

  // Called when the profile info cache has changed, which means we might
  // have to re-display the profile name.
  void UpdateAvatarButtonAndRelayoutParent();

  Browser* browser_;

  // This is used to check if the bubble was showing during the mouse pressed
  // event. If this is true then the mouse released event is ignored to prevent
  // the bubble from reshowing.
  bool suppress_mouse_released_action_;

  DISALLOW_COPY_AND_ASSIGN(NewAvatarButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_NEW_AVATAR_BUTTON_H_
