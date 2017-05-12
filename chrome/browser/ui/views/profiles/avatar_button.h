// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_BUTTON_H_

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/ui/avatar_button_error_controller.h"
#include "chrome/browser/ui/avatar_button_error_controller_delegate.h"
#include "chrome/browser/ui/views/profiles/avatar_button_style.h"
#include "ui/views/controls/button/label_button.h"

class Profile;

// Base class for avatar buttons that display the active profile's name in the
// caption area.
class AvatarButton : public views::LabelButton,
                     public AvatarButtonErrorControllerDelegate,
                     public ProfileAttributesStorage::Observer {
 public:
  AvatarButton(views::ButtonListener* listener,
               AvatarButtonStyle button_style,
               Profile* profile);
  ~AvatarButton() override;

  // views::LabelButton:
  void OnGestureEvent(ui::GestureEvent* event) override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetPreferredSize() const override;
  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override;

 protected:
  // views::LabelButton:
  bool ShouldUseFloodFillInkDrop() const override;

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

  // Sets generic_avatar_ to the image with the specified IDR.
  void SetButtonAvatar(int avatar_idr);

  AvatarButtonErrorController error_controller_;
  Profile* profile_;
  ScopedObserver<ProfileAttributesStorage, AvatarButton> profile_observer_;

  // The icon displayed instead of the profile name in the local profile case.
  // Different assets are used depending on the OS version.
  gfx::ImageSkia generic_avatar_;

  // True to use the Windows 10 native (non-themed) button, which is drawn using
  // a vector icon and can change height to slide atop the tabstrip. False to
  // use the old PNG, fixed-size icon button or a themed button.
  bool use_win10_native_button_;

  DISALLOW_COPY_AND_ASSIGN(AvatarButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_BUTTON_H_
