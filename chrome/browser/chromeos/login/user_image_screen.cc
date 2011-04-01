// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/user_image_screen.h"

#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/screen_observer.h"
#include "chrome/browser/chromeos/login/user_image_view.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "content/common/notification_service.h"
#include "content/common/notification_type.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace chromeos {

namespace {

// The resolution of the picture we want to get from the camera.
const int kFrameWidth = 480;
const int kFrameHeight = 480;

}  // namespace

UserImageScreen::UserImageScreen(WizardScreenDelegate* delegate)
    : ViewScreen<UserImageView>(delegate),
      camera_controller_(this) {
  camera_controller_.set_frame_width(kFrameWidth);
  camera_controller_.set_frame_height(kFrameHeight);
  registrar_.Add(
      this,
      NotificationType::SCREEN_LOCK_STATE_CHANGED,
      NotificationService::AllSources());
}

UserImageScreen::~UserImageScreen() {
}

void UserImageScreen::Refresh() {
  InitCamera();
}

void UserImageScreen::Hide() {
  camera_controller_.Stop();
  ViewScreen<UserImageView>::Hide();
}

UserImageView* UserImageScreen::AllocateView() {
  return new UserImageView(this);
}

void UserImageScreen::OnCaptureSuccess() {
  if (view()) {
    SkBitmap frame;
    camera_controller_.GetFrame(&frame);
    if (!frame.isNull())
      view()->UpdateVideoFrame(frame);
  }
}

void UserImageScreen::OnCaptureFailure() {
  if (view())
    view()->ShowCameraError();
}

void UserImageScreen::OnOK(const SkBitmap& image) {
  camera_controller_.Stop();

  UserManager* user_manager = UserManager::Get();
  DCHECK(user_manager);

  const UserManager::User& user = user_manager->logged_in_user();
  DCHECK(!user.email().empty());

  user_manager->SetLoggedInUserImage(image);
  user_manager->SaveUserImage(user.email(), image);
  if (delegate())
    delegate()->GetObserver(this)->OnExit(ScreenObserver::USER_IMAGE_SELECTED);
}

void UserImageScreen::OnSkip() {
  camera_controller_.Stop();
  if (delegate())
    delegate()->GetObserver(this)->OnExit(ScreenObserver::USER_IMAGE_SKIPPED);
}

void UserImageScreen::Observe(NotificationType type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  if (type != NotificationType::SCREEN_LOCK_STATE_CHANGED)
    return;

  bool is_screen_locked = *Details<bool>(details).ptr();
  if (is_screen_locked)
    camera_controller_.Stop();
  else
    InitCamera();
}

void UserImageScreen::InitCamera() {
  if (view())
    view()->ShowCameraInitializing();
  camera_controller_.Start();
}

}  // namespace chromeos

