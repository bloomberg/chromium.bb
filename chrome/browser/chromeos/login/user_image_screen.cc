// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/user_image_screen.h"

#include "base/compiler_specific.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/chromeos/accessibility/accessibility_util.h"
#include "chrome/browser/chromeos/login/default_user_images.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/screen_observer.h"
#include "chrome/browser/chromeos/login/user_image.h"
#include "chrome/browser/chromeos/login/user_image_manager.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

namespace chromeos {

namespace {

// Time histogram suffix for profile image download.
const char kProfileDownloadReason[] = "OOBE";

}  // namespace

UserImageScreen::UserImageScreen(ScreenObserver* screen_observer,
                                 UserImageScreenActor* actor)
    : WizardScreen(screen_observer),
      actor_(actor) {
  actor_->SetDelegate(this);
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_IMAGE_UPDATED,
      content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_IMAGE_UPDATE_FAILED,
      content::NotificationService::AllSources());
}

UserImageScreen::~UserImageScreen() {
  if (actor_)
    actor_->SetDelegate(NULL);
}

void UserImageScreen::PrepareToShow() {
  if (actor_)
    actor_->PrepareToShow();
}

void UserImageScreen::Show() {
  if (!actor_)
    return;

  actor_->Show();
  actor_->SelectImage(UserManager::Get()->GetLoggedInUser()->image_index());

  // Start fetching the profile image.
  UserManager::Get()->GetUserImageManager()->
      DownloadProfileImage(kProfileDownloadReason);

  accessibility::MaybeSpeak(
      l10n_util::GetStringUTF8(IDS_OPTIONS_CHANGE_PICTURE_DIALOG_TEXT));
}

void UserImageScreen::Hide() {
  if (actor_)
    actor_->Hide();
}

std::string UserImageScreen::GetName() const {
  return WizardController::kUserImageScreenName;
}

void UserImageScreen::OnPhotoTaken(const gfx::ImageSkia& image) {
  UserManager* user_manager = UserManager::Get();
  user_manager->GetUserImageManager()->SaveUserImage(
      user_manager->GetLoggedInUser()->email(),
      UserImage::CreateAndEncode(image));

  get_screen_observer()->OnExit(ScreenObserver::USER_IMAGE_SELECTED);

  UMA_HISTOGRAM_ENUMERATION("UserImage.FirstTimeChoice",
                            kHistogramImageFromCamera,
                            kHistogramImagesCount);
}

void UserImageScreen::OnProfileImageSelected() {
  UserManager* user_manager = UserManager::Get();
  user_manager->GetUserImageManager()->SaveUserImageFromProfileImage(
      user_manager->GetLoggedInUser()->email());

  get_screen_observer()->OnExit(ScreenObserver::USER_IMAGE_SELECTED);

  UMA_HISTOGRAM_ENUMERATION("UserImage.FirstTimeChoice",
                            kHistogramImageFromProfile,
                            kHistogramImagesCount);
}

void UserImageScreen::OnDefaultImageSelected(int index) {
  UserManager* user_manager = UserManager::Get();
  user_manager->GetUserImageManager()->SaveUserDefaultImageIndex(
      user_manager->GetLoggedInUser()->email(), index);

  get_screen_observer()->OnExit(ScreenObserver::USER_IMAGE_SELECTED);

  UMA_HISTOGRAM_ENUMERATION("UserImage.FirstTimeChoice",
                            GetDefaultImageHistogramValue(index),
                            kHistogramImagesCount);
}

void UserImageScreen::OnActorDestroyed(UserImageScreenActor* actor) {
  if (actor_ == actor)
    actor_ = NULL;
}

void UserImageScreen::Observe(int type,
                              const content::NotificationSource& source,
                              const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_PROFILE_IMAGE_UPDATED: {
      // We've got a new profile image.
      if (actor_) {
        actor_->AddProfileImage(
            *content::Details<const gfx::ImageSkia>(details).ptr());
      }
      break;
    }
    case chrome::NOTIFICATION_PROFILE_IMAGE_UPDATE_FAILED: {
      // User has a default profile image or fetching profile image has failed.
      if (actor_)
        actor_->OnProfileImageAbsent();
      break;
    }
    default:
      NOTREACHED();
  }
}

}  // namespace chromeos
