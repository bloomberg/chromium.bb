// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_NEW_AVATAR_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_NEW_AVATAR_BUTTON_H_

#include "base/macros.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/ui/avatar_button_error_controller.h"
#include "chrome/browser/ui/avatar_button_error_controller_delegate.h"
#include "chrome/browser/ui/views/profiles/avatar_button_style.h"
#include "ui/views/controls/button/label_button.h"

class AvatarButtonDelegate;
class Profile;

// Avatar button that displays the active profile's name in the caption area.
class NewAvatarButton : public views::LabelButton,
                        public AvatarButtonErrorControllerDelegate,
                        public ProfileAttributesStorage::Observer {
 public:
  NewAvatarButton(AvatarButtonDelegate* delegate,
                  AvatarButtonStyle button_style,
                  Profile* profile);
  ~NewAvatarButton() override;

  // Views::LabelButton
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;

  // Views
  void OnGestureEvent(ui::GestureEvent* event) override;

 private:
  friend class ProfileChooserViewExtensionsTest;

  // AvatarButtonErrorControllerDelegate:
  void OnAvatarErrorChanged() override;

  // ProfileAttributesStorage::Observer:
  void OnProfileAdded(const base::FilePath& profile_path) override;
  void OnProfileWasRemoved(const base::FilePath& profile_path,
                           const base::string16& profile_name) override;
  void OnProfileNameChanged(const base::FilePath& profile_path,
                            const base::string16& old_profile_name) override;
  void OnProfileSupervisedUserIdChanged(
      const base::FilePath& profile_path) override;

  // Called when the profile info cache or signin/sync error has changed, which
  // means we might have to update the icon/text of the button.
  void Update();

  AvatarButtonDelegate* delegate_;
  AvatarButtonErrorController error_controller_;
  Profile* profile_;

  // The icon displayed instead of the profile name in the local profile case.
  // Different assets are used depending on the OS version.
  gfx::ImageSkia generic_avatar_;

  // This is used to check if the bubble was showing during the mouse pressed
  // event. If this is true then the mouse released event is ignored to prevent
  // the bubble from reshowing.
  bool suppress_mouse_released_action_;

  DISALLOW_COPY_AND_ASSIGN(NewAvatarButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_NEW_AVATAR_BUTTON_H_
