// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/user_image_screen.h"

#include "base/compiler_specific.h"
#include "base/time.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/screen_observer.h"
#include "chrome/browser/chromeos/login/user_image_downloader.h"
#include "chrome/browser/chromeos/login/user_image_view.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"

namespace chromeos {

namespace {

// The resolution of the picture we want to get from the camera.
const int kFrameWidth = 480;
const int kFrameHeight = 480;

// Frame rate in milliseconds for video capturing.
// We want 25 FPS.
const int kFrameRate = 40;

}  // namespace

UserImageScreen::UserImageScreen(WizardScreenDelegate* delegate)
    : ViewScreen<UserImageView>(delegate),
      ALLOW_THIS_IN_INITIALIZER_LIST(camera_(new Camera(this))) {
  registrar_.Add(
      this,
      NotificationType::SCREEN_LOCK_STATE_CHANGED,
      NotificationService::AllSources());
  if (!camera_->Initialize(kFrameWidth, kFrameHeight)) {
    camera_.reset();
  } else {
    // We want to mimic mirror behavior so we want the image reflected from Y
    // axis.
    camera_->set_mirrored(true);
  }
}

UserImageScreen::~UserImageScreen() {
}

void UserImageScreen::Refresh() {
  if (camera_.get())
    camera_->StartCapturing(base::TimeDelta::FromMilliseconds(kFrameRate));
}

void UserImageScreen::Hide() {
  if (camera_.get())
    camera_->StopCapturing();
  ViewScreen<UserImageView>::Hide();
}

UserImageView* UserImageScreen::AllocateView() {
  return new UserImageView(this);
}

void UserImageScreen::OnVideoFrameCaptured(const SkBitmap& frame) {
  if (view())
    view()->UpdateVideoFrame(frame);
}

void UserImageScreen::OnOK(const SkBitmap& image) {
  UserManager* user_manager = UserManager::Get();
  if (user_manager) {
    // TODO(avayvod): Check that there's logged in user actually.
    const UserManager::User& user = user_manager->logged_in_user();
    user_manager->SaveUserImage(user.email(), image);
  }
  if (delegate())
    delegate()->GetObserver(this)->OnExit(ScreenObserver::USER_IMAGE_SELECTED);
}

void UserImageScreen::OnCancel() {
  // Download user image from his Google account.
  UserManager* user_manager = UserManager::Get();
  if (user_manager) {
    // TODO(avayvod): Check that there's logged in user actually.
    const UserManager::User& user = user_manager->logged_in_user();
    new UserImageDownloader(user.email(),
                            LoginUtils::Get()->GetAuthToken());
  }
  if (delegate())
    delegate()->GetObserver(this)->OnExit(ScreenObserver::USER_IMAGE_SKIPPED);
}

void UserImageScreen::Observe(NotificationType type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  if (type != NotificationType::SCREEN_LOCK_STATE_CHANGED ||
      !camera_.get())
    return;

  bool is_screen_locked = *Details<bool>(details).ptr();
  if (is_screen_locked)
    camera_->StopCapturing();
  else
    camera_->StartCapturing(base::TimeDelta::FromMilliseconds(kFrameRate));
}

}  // namespace chromeos

