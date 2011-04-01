// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_OPTIONS_TAKE_PHOTO_DIALOG_H_
#define CHROME_BROWSER_CHROMEOS_OPTIONS_TAKE_PHOTO_DIALOG_H_
#pragma once

#include "base/scoped_ptr.h"
#include "chrome/browser/chromeos/login/camera_controller.h"
#include "chrome/browser/chromeos/login/take_photo_view.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "views/window/dialog_delegate.h"

namespace views {
class View;
class Window;
}

namespace chromeos {

// A dialog box for taking new user picture.
class TakePhotoDialog : public views::View,
                        public views::DialogDelegate,
                        public TakePhotoView::Delegate,
                        public CameraController::Delegate,
                        public NotificationObserver {
 public:
  TakePhotoDialog();

  // views::DialogDelegate overrides.
  virtual bool IsDialogButtonEnabled(
      MessageBoxFlags::DialogButton button) const;
  virtual bool Cancel();
  virtual bool Accept();

  // views::WindowDelegate overrides.
  virtual bool IsModal() const { return true; }
  virtual views::View* GetContentsView();

  // views::View overrides.
  virtual void GetAccessibleState(ui::AccessibleViewState* state);

  // TakePhotoView::Delegate overrides.
  virtual void OnCapturingStarted();
  virtual void OnCapturingStopped();

  // CameraController::Delegate implementation:
  virtual void OnCaptureSuccess();
  virtual void OnCaptureFailure();

  // NotificationObserver implementation:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 protected:
  // views::View overrides:
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();

 private:
  // Starts initializing the camera and shows the appropriate status on the
  // screen.
  void InitCamera();

  TakePhotoView* take_photo_view_;

  CameraController camera_controller_;

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(TakePhotoDialog);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_OPTIONS_TAKE_PHOTO_DIALOG_H_
