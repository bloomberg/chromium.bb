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
#include "components/keyed_service/core/keyed_service_shutdown_notifier.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/widget/widget_observer.h"

class Profile;

// Base class for avatar buttons that display the active profile's name in the
// caption area.
class AvatarButton : public views::LabelButton,
                     public AvatarButtonErrorControllerDelegate,
                     public ProfileAttributesStorage::Observer,
                     public views::WidgetObserver {
 public:
  AvatarButton(views::ButtonListener* listener,
               AvatarButtonStyle button_style,
               Profile* profile);
  ~AvatarButton() override;

  void SetupThemeColorButton();

  // views::LabelButton:
  void AddedToWidget() override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size CalculatePreferredSize() const override;
  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override;
  std::unique_ptr<views::InkDropMask> CreateInkDropMask() const override;
  void NotifyClick(const ui::Event& event) override;

 protected:
  // views::LabelButton:
  bool ShouldEnterPushedState(const ui::Event& event) override;
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

  // views::WidgetObserver
  void OnWidgetClosing(views::Widget* widget) override;

  // Called when |profile_| is shutting down.
  void OnProfileShutdown();

  // Called when the profile info cache or signin/sync error has changed, which
  // means we might have to update the icon/text of the button.
  void Update();

  // Sets generic_avatar_ to the image with the specified IDR.
  void SetButtonAvatar(int avatar_idr);

  // Returns true when the button can get smaller to accomodate a more crowded
  // browser frame.
  bool IsCondensible() const;

  AvatarButtonErrorController error_controller_;
  Profile* profile_;

  // TODO(msarda): Remove |profile_shutdown_notifier_| when
  // http://crbug.com/579690 is fixed (it was added to track down the crash in
  // that bug).
  std::unique_ptr<KeyedServiceShutdownNotifier::Subscription>
      profile_shutdown_notifier_;
  ScopedObserver<ProfileAttributesStorage, AvatarButton> profile_observer_;

  // The icon displayed instead of the profile name in the local profile case.
  // Different assets are used depending on the OS version.
  gfx::ImageSkia generic_avatar_;

  AvatarButtonStyle button_style_;

  DISALLOW_COPY_AND_ASSIGN(AvatarButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_BUTTON_H_
