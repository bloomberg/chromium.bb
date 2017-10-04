// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DEVICE_INPUT_SERVICE_TEST_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_DEVICE_INPUT_SERVICE_TEST_HELPER_H_

#include "base/macros.h"
#include "device/hid/input_service_linux.h"
#include "device/hid/public/interfaces/input_service.mojom.h"

namespace chromeos {

class InputServiceProxy;
class TestObserver;

class InputServiceTestHelper {
 public:
  static const char kKeyboardId[];
  static const char kMouseId[];

  // This class must be instantiated BEFORE any InputServiceProxy. Otherwise, a
  // default InputServiceLinux will be created and become the proxy observer,
  // instead of the FakeInputServiceLinux from the .cc file.
  InputServiceTestHelper();
  ~InputServiceTestHelper();

  void SetProxy(InputServiceProxy* proxy);
  void ClearProxy();
  void AddDeviceToService(bool is_mouse, device::mojom::InputDeviceType type);
  void RemoveDeviceFromService(bool is_mouse);

 private:
  std::unique_ptr<TestObserver> observer_;
  InputServiceProxy* proxy_;

  DISALLOW_COPY_AND_ASSIGN(InputServiceTestHelper);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DEVICE_INPUT_SERVICE_TEST_HELPER_H_
