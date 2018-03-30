// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_SCREEN_SECURITY_SCREEN_SECURITY_NOTIFICATION_CONTROLLER_H_
#define ASH_SYSTEM_SCREEN_SECURITY_SCREEN_SECURITY_NOTIFICATION_CONTROLLER_H_

#include "ash/shell_observer.h"
#include "ash/system/screen_security/screen_capture_observer.h"
#include "ash/system/screen_security/screen_share_observer.h"
#include "base/macros.h"

namespace ash {

extern ASH_EXPORT const char kScreenCaptureNotificationId[];
extern ASH_EXPORT const char kScreenShareNotificationId[];
extern ASH_EXPORT const char kNotifierScreenCapture[];
extern ASH_EXPORT const char kNotifierScreenShare[];

// Controller class to manage screen security notifications.
class ASH_EXPORT ScreenSecurityNotificationController
    : public ScreenCaptureObserver,
      public ScreenShareObserver,
      public ShellObserver {
 public:
  ScreenSecurityNotificationController();
  ~ScreenSecurityNotificationController() override;

 private:
  // ScreenCaptureObserver:
  void OnScreenCaptureStart(
      const base::Closure& stop_callback,
      const base::string16& screen_capture_status) override;
  void OnScreenCaptureStop() override;

  // ScreenShareObserver:
  void OnScreenShareStart(const base::Closure& stop_callback,
                          const base::string16& helper_name) override;
  void OnScreenShareStop() override;

  // ShellObserver:
  void OnCastingSessionStartedOrStopped(bool started) override;

  bool is_casting_ = false;

  DISALLOW_COPY_AND_ASSIGN(ScreenSecurityNotificationController);
};

}  // namespace ash

#endif  // ASH_SYSTEM_SCREEN_SECURITY_SCREEN_SECURITY_NOTIFICATION_CONTROLLER_H_
