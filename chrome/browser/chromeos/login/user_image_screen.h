// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USER_IMAGE_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USER_IMAGE_SCREEN_H_
#pragma once

#include "chrome/browser/chromeos/login/camera_controller.h"
#include "chrome/browser/chromeos/login/user_image_screen_actor.h"
#include "chrome/browser/chromeos/login/wizard_screen.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace chromeos {

class UserImageScreen: public WizardScreen,
                       public CameraController::Delegate,
                       public UserImageScreenActor::Delegate,
                       public NotificationObserver {
 public:
  UserImageScreen(ScreenObserver* screen_observer,
                  UserImageScreenActor* actor);
  virtual ~UserImageScreen();

  // WizardScreen implementation:
  virtual void PrepareToShow();
  virtual void Show();
  virtual void Hide();

  // CameraController::Delegate implementation:
  virtual void OnCaptureSuccess();
  virtual void OnCaptureFailure();

  // UserImageScreenActor::Delegate implementation:
  virtual void StartCamera();
  virtual void StopCamera();
  virtual void OnPhotoTaken(const SkBitmap& image);
  virtual void OnDefaultImageSelected(int index);
  virtual void OnActorDestroyed(UserImageScreenActor* actor);

  // NotificationObserver implementation:
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  CameraController camera_controller_;

  NotificationRegistrar registrar_;

  UserImageScreenActor* actor_;

  DISALLOW_COPY_AND_ASSIGN(UserImageScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USER_IMAGE_SCREEN_H_

