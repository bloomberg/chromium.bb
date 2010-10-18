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
const int64 kFrameRate = 40;

}  // namespace

UserImageScreen::UserImageScreen(WizardScreenDelegate* delegate)
    : ViewScreen<UserImageView>(delegate),
      ALLOW_THIS_IN_INITIALIZER_LIST(camera_(new Camera(this, true))),
      camera_initialized_(false) {
  registrar_.Add(
      this,
      NotificationType::SCREEN_LOCK_STATE_CHANGED,
      NotificationService::AllSources());
  camera_->Initialize(kFrameWidth, kFrameHeight);
}

UserImageScreen::~UserImageScreen() {
  if (camera_.get())
    camera_->set_delegate(NULL);
}

void UserImageScreen::Refresh() {
  if (camera_.get() && camera_initialized_)
    camera_->StartCapturing(kFrameRate);
}

void UserImageScreen::Hide() {
  if (camera_.get() && camera_initialized_)
    camera_->StopCapturing();
  ViewScreen<UserImageView>::Hide();
}

UserImageView* UserImageScreen::AllocateView() {
  return new UserImageView(this);
}

void UserImageScreen::OnInitializeSuccess() {
  camera_initialized_ = true;
  if (camera_.get())
    camera_->StartCapturing(kFrameRate);
}

void UserImageScreen::OnInitializeFailure() {
  if (view())
    view()->ShowCameraError();
  camera_initialized_ = false;
}

void UserImageScreen::OnStartCapturingSuccess() {
}

void UserImageScreen::OnStartCapturingFailure() {
  if (view())
    view()->ShowCameraError();
}

void UserImageScreen::OnCaptureSuccess(const SkBitmap& frame) {
  if (view())
    view()->UpdateVideoFrame(frame);
}

void UserImageScreen::OnCaptureFailure() {
  // If camera failed to provide a picture we don't want to show broken
  // camera image since it may lead to flicker if capturing fails and then
  // works again.
  // TODO(avayvod): Find a better way to handle such cases.
  VLOG(1) << "Capturing image failed.";
}

void UserImageScreen::OnOK(const SkBitmap& image) {
  if (camera_.get())
    camera_->Uninitialize();
  UserManager* user_manager = UserManager::Get();
  DCHECK(user_manager);

  const UserManager::User& user = user_manager->logged_in_user();
  DCHECK(!user.email().empty());

  user_manager->SaveUserImage(user.email(), image);
  if (delegate())
    delegate()->GetObserver(this)->OnExit(ScreenObserver::USER_IMAGE_SELECTED);
}

void UserImageScreen::OnSkip() {
  if (camera_.get())
    camera_->Uninitialize();
  UserManager* user_manager = UserManager::Get();
  DCHECK(user_manager);

  const UserManager::User& user = user_manager->logged_in_user();
  DCHECK(!user.email().empty());

  user_manager->SetDefaultUserImage(user.email());
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
    camera_->StartCapturing(kFrameRate);
}

}  // namespace chromeos

