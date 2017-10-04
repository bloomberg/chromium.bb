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
                  std::vector<device::mojom::InputDeviceInfoPtr> devices) {
  EXPECT_EQ(2, static_cast<int>(devices.size()));
  done.Run();
}

}  // namespace

typedef InProcessBrowserTest InputServiceProxyTest;

IN_PROC_BROWSER_TEST_F(InputServiceProxyTest, Simple) {
  InputServiceTestHelper helper;
  InputServiceProxy proxy;
  helper.SetProxy(&proxy);

  // Add USB keyboard.
  helper.AddDeviceToService(false, device::mojom::InputDeviceType::TYPE_USB);

  // Add bluetooth mouse.
  helper.AddDeviceToService(true,
                            device::mojom::InputDeviceType::TYPE_BLUETOOTH);

  base::RunLoop run;
  proxy.GetDevices(base::BindOnce(&OnGetDevices, run.QuitClosure()));
  run.Run();

  helper.ClearProxy();
}

}  // namespace chromeos
