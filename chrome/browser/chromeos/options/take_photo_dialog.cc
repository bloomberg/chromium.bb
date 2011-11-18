// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/options/take_photo_dialog.h"

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/layout/layout_constants.h"

namespace chromeos {

namespace {

// The resolution of the picture we want to get from the camera.
const int kFrameWidth = 480;
const int kFrameHeight = 480;

}  // namespace

TakePhotoDialog::TakePhotoDialog(Delegate* delegate)
    : take_photo_view_(NULL),
      camera_controller_(this),
      delegate_(delegate) {
  camera_controller_.set_frame_width(kFrameWidth);
  camera_controller_.set_frame_height(kFrameHeight);
  registrar_.Add(
      this,
      chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED,
      content::NotificationService::AllSources());
}

TakePhotoDialog::~TakePhotoDialog() {
}

bool TakePhotoDialog::IsDialogButtonEnabled(ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_CANCEL)
    return true;
  else if (button == ui::DIALOG_BUTTON_OK)
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

  if (delegate_)
    delegate_->OnPhotoAccepted(take_photo_view_->GetImage());
  return true;
}

bool TakePhotoDialog::IsModal() const {
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
  NotifyOnCapturingStopped();
}

void TakePhotoDialog::OnCaptureSuccess() {
  SkBitmap frame;
  camera_controller_.GetFrame(&frame);
  if (!frame.isNull()) {
    take_photo_view_->UpdateVideoFrame(frame);
    NotifyOnCaptureSuccess();
  }
}

void TakePhotoDialog::OnCaptureFailure() {
  take_photo_view_->ShowCameraError();
  NotifyOnCaptureFailure();
}

void TakePhotoDialog::AddObserver(Observer* obs) {
  observer_list_.AddObserver(obs);
}

void TakePhotoDialog::RemoveObserver(Observer* obs) {
  observer_list_.RemoveObserver(obs);
}

void TakePhotoDialog::NotifyOnCaptureSuccess() {
 FOR_EACH_OBSERVER(
    Observer,
    observer_list_,
    OnCaptureSuccess(this, take_photo_view_));
}

void TakePhotoDialog::NotifyOnCaptureFailure() {
 FOR_EACH_OBSERVER(
    Observer,
    observer_list_,
    OnCaptureFailure(this, take_photo_view_));
}

void TakePhotoDialog::NotifyOnCapturingStopped() {
 FOR_EACH_OBSERVER(
    Observer,
    observer_list_,
    OnCapturingStopped(this, take_photo_view_));
}

void TakePhotoDialog::Observe(int type,
                              const content::NotificationSource& source,
                              const content::NotificationDetails& details) {
  if (type != chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED)
    return;

  bool is_screen_locked = *content::Details<bool>(details).ptr();
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
