// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/views_user_image_screen_actor.h"

namespace chromeos {

ViewsUserImageScreenActor::ViewsUserImageScreenActor(
    ViewScreenDelegate* delegate)
    : ViewScreen<UserImageView>(delegate),
      screen_(NULL) {
}

ViewsUserImageScreenActor::~ViewsUserImageScreenActor() {
  if (screen_)
    screen_->OnActorDestroyed(this);
}

UserImageView* ViewsUserImageScreenActor::AllocateView() {
  return new UserImageView(this);
}

void ViewsUserImageScreenActor::PrepareToShow() {
  ViewScreen<UserImageView>::PrepareToShow();
}

void ViewsUserImageScreenActor::Show() {
  ViewScreen<UserImageView>::Show();
}

void ViewsUserImageScreenActor::Hide() {
  ViewScreen<UserImageView>::Hide();
}

void ViewsUserImageScreenActor::SetDelegate(
    UserImageScreenActor::Delegate* delegate) {
  screen_ = delegate;
}

void ViewsUserImageScreenActor::SelectImage(int index) {
  view()->OnImageSelected(index);
}

void ViewsUserImageScreenActor::UpdateVideoFrame(const SkBitmap& frame) {
  view()->UpdateVideoFrame(frame);
}

void ViewsUserImageScreenActor::ShowCameraError() {
  view()->ShowCameraError();
}

void ViewsUserImageScreenActor::ShowCameraInitializing() {
  view()->ShowCameraInitializing();
}

bool ViewsUserImageScreenActor::IsCapturing() const {
  return view()->IsCapturing();
}

void ViewsUserImageScreenActor::StartCamera() {
  if (screen_)
    screen_->StartCamera();
}

void ViewsUserImageScreenActor::StopCamera() {
  if (screen_)
    screen_->StopCamera();
}

void ViewsUserImageScreenActor::OnPhotoTaken(const SkBitmap& image) {
  if (screen_)
    screen_->OnPhotoTaken(image);
}

void ViewsUserImageScreenActor::OnDefaultImageSelected(int index) {
  if (screen_)
    screen_->OnDefaultImageSelected(index);
}

}  // namespace chromeos

