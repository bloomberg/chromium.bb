// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/user_image_screen.h"

#include "base/compiler_specific.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/chromeos/login/default_user_images.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/screen_observer.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/wizard_accessibility_helper.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace chromeos {

namespace {

// The resolution of the picture we want to get from the camera.
const int kFrameWidth = 480;
const int kFrameHeight = 480;

}  // namespace

UserImageScreen::UserImageScreen(ScreenObserver* screen_observer,
                                 UserImageScreenActor* actor)
    : WizardScreen(screen_observer),
      camera_controller_(this),
      actor_(actor) {
  actor_->SetDelegate(this);
  camera_controller_.set_frame_width(kFrameWidth);
  camera_controller_.set_frame_height(kFrameHeight);
  registrar_.Add(
      this,
      chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED,
      content::NotificationService::AllSources());
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
  actor_->SelectImage(UserManager::Get()->logged_in_user().image_index());

  // Start fetching the profile image.
  UserManager::Get()->DownloadProfileImage();

  WizardAccessibilityHelper::GetInstance()->MaybeSpeak(
      l10n_util::GetStringUTF8(IDS_OPTIONS_CHANGE_PICTURE_DIALOG_TEXT).c_str(),
      false, false);
}

void UserImageScreen::Hide() {
  camera_controller_.Stop();
  if (actor_)
    actor_->Hide();
}

void UserImageScreen::OnCaptureSuccess() {
  if (!actor_)
    return;

  SkBitmap frame;
  camera_controller_.GetFrame(&frame);
  if (!frame.isNull())
    actor_->UpdateVideoFrame(frame);
}

void UserImageScreen::OnCaptureFailure() {
  if (actor_)
    actor_->ShowCameraError();
}

void UserImageScreen::StartCamera() {
  if (!actor_)
    return;

  actor_->ShowCameraInitializing();
  camera_controller_.Start();
}

void UserImageScreen::StopCamera() {
  camera_controller_.Stop();
}

void UserImageScreen::OnPhotoTaken(const SkBitmap& image) {
  UserManager* user_manager = UserManager::Get();
  user_manager->SaveUserImage(user_manager->logged_in_user().email(), image);

  get_screen_observer()->OnExit(ScreenObserver::USER_IMAGE_SELECTED);

  UMA_HISTOGRAM_ENUMERATION("UserImage.FirstTimeChoice",
                            kHistogramImageFromCamera,
                            kHistogramImagesCount);
}

void UserImageScreen::OnProfileImageSelected() {
  UserManager* user_manager = UserManager::Get();
  user_manager->SaveUserImageFromProfileImage(
      user_manager->logged_in_user().email());

  get_screen_observer()->OnExit(ScreenObserver::USER_IMAGE_SELECTED);

  UMA_HISTOGRAM_ENUMERATION("UserImage.FirstTimeChoice",
                            kHistogramImageFromProfile,
                            kHistogramImagesCount);
}

void UserImageScreen::OnDefaultImageSelected(int index) {
  UserManager* user_manager = UserManager::Get();
  user_manager->SaveUserDefaultImageIndex(
      user_manager->logged_in_user().email(), index);

  get_screen_observer()->OnExit(ScreenObserver::USER_IMAGE_SELECTED);

  UMA_HISTOGRAM_ENUMERATION("UserImage.FirstTimeChoice",
                            index,
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
    case chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED: {
      bool is_screen_locked = *content::Details<bool>(details).ptr();
      if (is_screen_locked)
        StopCamera();
      else if (actor_ && actor_->IsCapturing())
        StartCamera();
      break;
    }
    case chrome::NOTIFICATION_PROFILE_IMAGE_UPDATED: {
      // We've got a new profile image.
      if (actor_)
        actor_->AddProfileImage(
            *content::Details<const SkBitmap>(details).ptr());
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
