// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USER_IMAGE_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USER_IMAGE_SCREEN_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/login/camera_controller.h"
#include "chrome/browser/chromeos/login/user_image_screen_actor.h"
#include "chrome/browser/chromeos/login/wizard_screen.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace chromeos {

class UserImageScreen: public WizardScreen,
                       public CameraController::Delegate,
                       public UserImageScreenActor::Delegate,
                       public content::NotificationObserver {
 public:
  UserImageScreen(ScreenObserver* screen_observer,
                  UserImageScreenActor* actor);
  virtual ~UserImageScreen();

  // WizardScreen implementation:
  virtual void PrepareToShow() OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;

  // CameraController::Delegate implementation:
  virtual void OnCaptureSuccess() OVERRIDE;
  virtual void OnCaptureFailure() OVERRIDE;

  // UserImageScreenActor::Delegate implementation:
  virtual void StartCamera() OVERRIDE;
  virtual void StopCamera() OVERRIDE;
  virtual void OnPhotoTaken(const SkBitmap& image) OVERRIDE;
  virtual void OnProfileImageSelected() OVERRIDE;
  virtual void OnDefaultImageSelected(int index) OVERRIDE;
  virtual void OnActorDestroyed(UserImageScreenActor* actor) OVERRIDE;

  // content::NotificationObserver implementation:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  CameraController camera_controller_;

  content::NotificationRegistrar registrar_;

  UserImageScreenActor* actor_;

  DISALLOW_COPY_AND_ASSIGN(UserImageScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USER_IMAGE_SCREEN_H_
