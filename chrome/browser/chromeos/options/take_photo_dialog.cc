// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/options/take_photo_dialog.h"

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "content/common/notification_service.h"
#include "content/common/notification_type.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "views/layout/layout_constants.h"

namespace chromeos {

namespace {

// The resolution of the picture we want to get from the camera.
const int kFrameWidth = 480;
const int kFrameHeight = 480;

}  // namespace

TakePhotoDialog::TakePhotoDialog()
    : take_photo_view_(NULL),
      camera_controller_(this) {
  camera_controller_.set_frame_width(kFrameWidth);
  camera_controller_.set_frame_height(kFrameHeight);
  registrar_.Add(
      this,
      NotificationType::SCREEN_LOCK_STATE_CHANGED,
      NotificationService::AllSources());
}

bool TakePhotoDialog::IsDialogButtonEnabled(
    MessageBoxFlags::DialogButton button) const {
  if (button == MessageBoxFlags::DIALOGBUTTON_CANCEL)
    return true;
  else if (button == MessageBoxFlags::DIALOGBUTTON_OK)
    return !take_photo_view_->is_capturing();
  NOTREACHED();
  return false;
}

bool TakePhotoDialog::Cancel() {
  camera_controller_.Stop();
  return true;
}

bool TakePhotoDialog::Accept() {
  camera_controller_.Stop();

  UserManager* user_manager = UserManager::Get();
  DCHECK(user_manager);

  const UserManager::User& user = user_manager->logged_in_user();
  DCHECK(!user.email().empty());

  const SkBitmap& image = take_photo_view_->GetImage();
  user_manager->SetLoggedInUserImage(image);
  user_manager->SaveUserImage(user.email(), image);
  return true;
}

views::View* TakePhotoDialog::GetContentsView() {
  // Lazy initialization upon request.
  if (!take_photo_view_) {
    take_photo_view_ = new TakePhotoView(this);
    take_photo_view_->Init();
    AddChildView(take_photo_view_);
    InitCamera();
  }
  return this;
}

void TakePhotoDialog::GetAccessibleState(ui::AccessibleViewState* state) {
  state->name = l10n_util::GetStringUTF16(
      IDS_OPTIONS_CHANGE_PICTURE_TAKE_PHOTO);
  state->role = ui::AccessibilityTypes::ROLE_DIALOG;
}

void TakePhotoDialog::OnCapturingStarted() {
  GetDialogClientView()->ok_button()->SetEnabled(false);
}

void TakePhotoDialog::OnCapturingStopped() {
  GetDialogClientView()->ok_button()->SetEnabled(true);
  GetDialogClientView()->ok_button()->RequestFocus();
}

void TakePhotoDialog::OnCaptureSuccess() {
  SkBitmap frame;
  camera_controller_.GetFrame(&frame);
  if (!frame.isNull())
    take_photo_view_->UpdateVideoFrame(frame);
}

void TakePhotoDialog::OnCaptureFailure() {
  take_photo_view_->ShowCameraError();
}

void TakePhotoDialog::Observe(NotificationType type,
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

void TakePhotoDialog::Layout() {
  int left = views::kPanelHorizMargin;
  int top = views::kPanelVertMargin;
  int contents_width = width() - 2 * left;
  int contents_height = height() - 2 * top;
  take_photo_view_->SetBounds(left, top, contents_width, contents_height);
}

gfx::Size TakePhotoDialog::GetPreferredSize() {
  return gfx::Size(login::kUserImageSize * 2, (login::kUserImageSize * 3 / 2));
}

void TakePhotoDialog::InitCamera() {
  take_photo_view_->ShowCameraInitializing();
  camera_controller_.Start();
}

}  // namespace chromeos

