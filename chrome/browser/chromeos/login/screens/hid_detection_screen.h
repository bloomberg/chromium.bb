// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_HID_DETECTION_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_HID_DETECTION_SCREEN_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "chrome/browser/chromeos/login/screens/hid_detection_screen_actor.h"

namespace chromeos {

// Representation independent class that controls screen showing warning about
// HID absence to users.
class HIDDetectionScreen : public BaseScreen,
                           public HIDDetectionScreenActor::Delegate {
 public:
  HIDDetectionScreen(BaseScreenDelegate* base_screen_delegate,
                     HIDDetectionScreenActor* actor);
  virtual ~HIDDetectionScreen();

  // BaseScreen implementation:
  virtual void PrepareToShow() override;
  virtual void Show() override;
  virtual void Hide() override;
  virtual std::string GetName() const override;

  // HIDDetectionScreenActor::Delegate implementation:
  virtual void OnExit() override;
  virtual void OnActorDestroyed(HIDDetectionScreenActor* actor) override;

 private:
  HIDDetectionScreenActor* actor_;

  DISALLOW_COPY_AND_ASSIGN(HIDDetectionScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_HID_DETECTION_SCREEN_H_
