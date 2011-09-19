// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_OPTIONS_TAKE_PHOTO_DIALOG_H_
#define CHROME_BROWSER_CHROMEOS_OPTIONS_TAKE_PHOTO_DIALOG_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/login/camera_controller.h"
#include "chrome/browser/chromeos/login/take_photo_view.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "views/window/dialog_delegate.h"

namespace views {
class View;
}

namespace chromeos {

// A dialog box for taking new user picture.
class TakePhotoDialog : public views::DialogDelegateView,
                        public TakePhotoView::Delegate,
                        public CameraController::Delegate,
                        public NotificationObserver {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Called when user accepts the photo.
    virtual void OnPhotoAccepted(const SkBitmap& photo) = 0;
  };

  explicit TakePhotoDialog(Delegate* delegate);
  virtual ~TakePhotoDialog();

  // views::DialogDelegateView overrides.
  virtual bool IsDialogButtonEnabled(
      MessageBoxFlags::DialogButton button) const;
  virtual bool Cancel();
  virtual bool Accept();

  // views::WidgetDelegate overrides.
  virtual bool IsModal() const;
  virtual views::View* GetContentsView();

  // views::View overrides.
  virtual void GetAccessibleState(ui::AccessibleViewState* state);

  // TakePhotoView::Delegate overrides.
  virtual void OnCapturingStarted();
  virtual void OnCapturingStopped();

  // CameraController::Delegate implementation:
  virtual void OnCaptureSuccess();
  virtual void OnCaptureFailure();

  // Interface that observers of this dialog must implement in order
  // to receive notification for capture success/failure.
  class Observer {
   public:
    // Called when image is captured and is displayed
    virtual void OnCaptureSuccess(
        TakePhotoDialog* dialog,
        TakePhotoView* view) = 0;
    // Called when capture fails and error image is displayed
    virtual void OnCaptureFailure(
        TakePhotoDialog* dialog,
        TakePhotoView* view) = 0;
    // Called when capture is stopped and image is not being updated
    virtual void OnCapturingStopped(
        TakePhotoDialog* dialog,
        TakePhotoView* view) = 0;

   protected:
    virtual ~Observer() {}
  };

  void AddObserver(Observer* obs);
  void RemoveObserver(Observer* obs);

  void NotifyOnCaptureSuccess();
  void NotifyOnCaptureFailure();
  void NotifyOnCapturingStopped();

  // NotificationObserver implementation:
  virtual void Observe(int type,
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

  Delegate* delegate_;

  ObserverList<Observer> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(TakePhotoDialog);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_OPTIONS_TAKE_PHOTO_DIALOG_H_
