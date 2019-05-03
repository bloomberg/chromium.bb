// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/macros.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/test/hid_controller_mixin.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ui/webui/chromeos/login/hid_detection_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/welcome_screen_handler.h"

using testing::_;

namespace chromeos {

class HidDetectionTest : public OobeBaseTest {
 public:
  HidDetectionTest() = default;
  ~HidDetectionTest() override = default;

 protected:
  test::HIDControllerMixin hid_controller_{&mixin_host_};

 private:
  DISALLOW_COPY_AND_ASSIGN(HidDetectionTest);
};

class HidDetectionSkipTest : public HidDetectionTest {
 public:
  HidDetectionSkipTest() {
    hid_controller_.AddUsbMouse(test::HIDControllerMixin::kMouseId);
    hid_controller_.AddUsbKeyboard(test::HIDControllerMixin::kKeyboardId);
  }

  ~HidDetectionSkipTest() override = default;
};

IN_PROC_BROWSER_TEST_F(HidDetectionTest, NoDevicesConnected) {
  OobeScreenWaiter(HIDDetectionView::kScreenId).Wait();
}

IN_PROC_BROWSER_TEST_F(HidDetectionSkipTest, BothDevicesPreConnected) {
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
}

}  // namespace chromeos
