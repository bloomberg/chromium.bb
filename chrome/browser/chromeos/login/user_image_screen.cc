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
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace chromeos {

namespace {

// The resolution of the picture we want to get from the camera.
const int kFrameWidth = 480;
const int kFrameHeight = 480;

// Maximum number of capture failures we ignore before we try to initialize
// the camera again.
const int kMaxCaptureFailureCounter = 5;

// Maximum number of camera initialization retries.
const int kMaxCameraInitFailureCounter = 3;

// Name for camera thread.
const char kCameraThreadName[] = "Chrome_CameraThread";

}  // namespace

UserImageScreen::UserImageScreen(WizardScreenDelegate* delegate)
    : ViewScreen<UserImageView>(delegate),
      capture_failure_counter_(0),
      camera_init_failure_counter_(0),
      camera_thread_(kCameraThreadName) {
  registrar_.Add(
      this,
      NotificationType::SCREEN_LOCK_STATE_CHANGED,
      NotificationService::AllSources());
}

UserImageScreen::~UserImageScreen() {
  if (camera_.get())
    camera_->set_delegate(NULL);
}

void UserImageScreen::Refresh() {
  camera_thread_.Start();
  camera_ = new Camera(this, &camera_thread_, true);
  InitCamera();
}

void UserImageScreen::Hide() {
  if (camera_.get())
    camera_->StopCapturing();
  ViewScreen<UserImageView>::Hide();
}

UserImageView* UserImageScreen::AllocateView() {
  return new UserImageView(this);
}

void UserImageScreen::OnInitializeSuccess() {
  if (camera_.get())
    camera_->StartCapturing();
}

void UserImageScreen::OnInitializeFailure() {
  ++camera_init_failure_counter_;
  if (camera_init_failure_counter_ > kMaxCameraInitFailureCounter) {
    if (view())
      view()->ShowCameraError();
    return;
  }
  // Retry initializing the camera.
  if (camera_.get()) {
    camera_->Uninitialize();
    InitCamera();
  }
}

void UserImageScreen::OnStartCapturingSuccess() {
}

void UserImageScreen::OnStartCapturingFailure() {
  // Try to reinitialize camera.
  OnInitializeFailure();
}

void UserImageScreen::OnCaptureSuccess() {
  capture_failure_counter_ = 0;
  camera_init_failure_counter_ = 0;
  if (view() && camera_.get()) {
    SkBitmap frame;
    camera_->GetFrame(&frame);
    if (!frame.isNull())
      view()->UpdateVideoFrame(frame);
  }
}

void UserImageScreen::OnCaptureFailure() {
  ++capture_failure_counter_;
  if (capture_failure_counter_ < kMaxCaptureFailureCounter)
    return;

  capture_failure_counter_ = 0;
  OnInitializeFailure();
}

void UserImageScreen::OnOK(const SkBitmap& image) {
  if (camera_.get())
    camera_->Uninitialize();
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
  if (camera_.get())
    camera_->Uninitialize();
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
    camera_->Uninitialize();
  else
    InitCamera();
}

void UserImageScreen::InitCamera() {
  if (view())
    view()->ShowCameraInitializing();
  camera_->Initialize(kFrameWidth, kFrameHeight);
}

}  // namespace chromeos

