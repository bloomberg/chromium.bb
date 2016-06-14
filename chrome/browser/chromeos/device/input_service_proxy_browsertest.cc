// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/device/input_service_proxy.h"
#include "chrome/browser/chromeos/device/input_service_test_helper.h"
#include "chrome/test/base/in_process_browser_test.h"

namespace chromeos {

namespace {

void OnGetDevices(const base::Closure& done,
                  const std::vector<InputDeviceInfo>& devices) {
  EXPECT_EQ(2, static_cast<int>(devices.size()));
  done.Run();
}

void OnGetKeyboard(const base::Closure& done,
                   bool success,
                   const InputDeviceInfo& info) {
  EXPECT_TRUE(success);
  EXPECT_EQ(InputServiceTestHelper::kKeyboardId, info.id);
  EXPECT_TRUE(info.is_keyboard);
  done.Run();
}

void OnGetMouse(const base::Closure& done,
                bool success,
                const InputDeviceInfo& /* info */) {
  EXPECT_FALSE(success);
  done.Run();
}

}  // namespace

typedef InProcessBrowserTest InputServiceProxyTest;

IN_PROC_BROWSER_TEST_F(InputServiceProxyTest, Simple) {
  InputServiceTestHelper helper;
  InputServiceProxy proxy;
  helper.SetProxy(&proxy);

  // Add USB keyboard.
  helper.AddDeviceToService(false, InputDeviceInfo::TYPE_USB);

  // Add bluetooth mouse.
  helper.AddDeviceToService(true, InputDeviceInfo::TYPE_BLUETOOTH);

  {
    base::RunLoop run;
    proxy.GetDevices(base::Bind(&OnGetDevices, run.QuitClosure()));
    run.Run();
  }

  // Remove mouse.
  helper.RemoveDeviceFromService(true);

  {
    base::RunLoop run;
    proxy.GetDeviceInfo(InputServiceTestHelper::kKeyboardId,
                        base::Bind(&OnGetKeyboard, run.QuitClosure()));
    run.Run();
  }

  {
    base::RunLoop run;
    proxy.GetDeviceInfo(InputServiceTestHelper::kMouseId,
                        base::Bind(&OnGetMouse, run.QuitClosure()));
    run.Run();
  }

  helper.ClearProxy();
}

}  // namespace chromeos
