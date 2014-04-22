// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/device/input_service_proxy.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_thread.h"
#include "device/hid/input_service_linux.h"

using content::BrowserThread;
using device::InputServiceLinux;

typedef InputServiceLinux::InputDeviceInfo InputDeviceInfo;

namespace chromeos {

namespace {

const char kKeyboardId[] = "keyboard";
const char kMouseId[] = "mouse";

class InputServiceLinuxTestImpl : public InputServiceLinux {
 public:
  InputServiceLinuxTestImpl() {}
  virtual ~InputServiceLinuxTestImpl() {}

  void AddDeviceForTesting(const InputDeviceInfo& info) { AddDevice(info); }
  void RemoveDeviceForTesting(const std::string& id) { RemoveDevice(id); }

 private:
  DISALLOW_COPY_AND_ASSIGN(InputServiceLinuxTestImpl);
};

class TestObserver : public InputServiceProxy::Observer {
 public:
  TestObserver()
      : wait_for_device_addition_(false), wait_for_device_removal_(false) {}
  virtual ~TestObserver() {}

  virtual void OnInputDeviceAdded(const InputDeviceInfo& info) OVERRIDE {
    if (!wait_for_device_addition_)
      return;
    EXPECT_TRUE(Equals(expected_info_, info));
    done_.Run();
  }

  virtual void OnInputDeviceRemoved(const std::string& id) OVERRIDE {
    if (!wait_for_device_removal_)
      return;
    EXPECT_EQ(expected_id_, id);
    done_.Run();
  }

  void WaitForDeviceAddition(const InputDeviceInfo& info) {
    base::RunLoop run;
    expected_info_ = info;
    wait_for_device_addition_ = true;
    done_ = run.QuitClosure();

    run.Run();

    done_.Reset();
    wait_for_device_addition_ = false;
  }

  void WaitForDeviceRemoval(const std::string& id) {
    base::RunLoop run;
    expected_id_ = id;
    wait_for_device_removal_ = true;
    done_ = run.QuitClosure();

    run.Run();

    done_.Reset();
    wait_for_device_removal_ = false;
  }

 private:
  static bool Equals(const InputDeviceInfo& lhs, const InputDeviceInfo& rhs) {
    return lhs.id == rhs.id && lhs.name == rhs.name &&
           lhs.subsystem == rhs.subsystem &&
           lhs.type == rhs.type &&
           lhs.is_accelerometer == rhs.is_accelerometer &&
           lhs.is_joystick == rhs.is_joystick && lhs.is_key == rhs.is_key &&
           lhs.is_keyboard == rhs.is_keyboard && lhs.is_mouse == rhs.is_mouse &&
           lhs.is_tablet == rhs.is_tablet &&
           lhs.is_touchpad == rhs.is_touchpad &&
           lhs.is_touchscreen == rhs.is_touchscreen;
  }

  InputDeviceInfo expected_info_;
  std::string expected_id_;

  bool wait_for_device_addition_;
  bool wait_for_device_removal_;

  base::Closure done_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

void InitInputService() {
  InputServiceLinux::SetForTesting(new InputServiceLinuxTestImpl());
}

void AddDevice(const InputDeviceInfo& device) {
  InputServiceLinuxTestImpl* service =
      static_cast<InputServiceLinuxTestImpl*>(InputServiceLinux::GetInstance());
  service->AddDeviceForTesting(device);
}

void RemoveDevice(const std::string& id) {
  InputServiceLinuxTestImpl* service =
      static_cast<InputServiceLinuxTestImpl*>(InputServiceLinux::GetInstance());
  service->RemoveDeviceForTesting(id);
}

void OnGetDevices(const base::Closure& done,
                  const std::vector<InputDeviceInfo>& devices) {
  EXPECT_EQ(2, static_cast<int>(devices.size()));
  done.Run();
}

void OnGetKeyboard(const base::Closure& done,
                   bool success,
                   const InputDeviceInfo& info) {
  EXPECT_TRUE(success);
  EXPECT_EQ("keyboard", info.id);
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

class InputServiceProxyTest : public InProcessBrowserTest {
 public:
  InputServiceProxyTest() {}
  virtual ~InputServiceProxyTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(InputServiceProxyTest);
};

IN_PROC_BROWSER_TEST_F(InputServiceProxyTest, Simple) {
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE, base::Bind(&InitInputService));
  InputServiceProxy proxy;
  TestObserver observer;
  proxy.AddObserver(&observer);

  InputDeviceInfo keyboard;
  keyboard.id = kKeyboardId;
  keyboard.subsystem = InputServiceLinux::InputDeviceInfo::SUBSYSTEM_INPUT;
  keyboard.type = InputServiceLinux::InputDeviceInfo::TYPE_USB;
  keyboard.is_keyboard = true;
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE, base::Bind(&AddDevice, keyboard));
  observer.WaitForDeviceAddition(keyboard);

  InputDeviceInfo mouse;
  mouse.id = kMouseId;
  mouse.subsystem = InputServiceLinux::InputDeviceInfo::SUBSYSTEM_INPUT;
  mouse.type = InputServiceLinux::InputDeviceInfo::TYPE_BLUETOOTH;
  mouse.is_mouse = true;
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE, base::Bind(&AddDevice, mouse));
  observer.WaitForDeviceAddition(mouse);

  {
    base::RunLoop run;
    proxy.GetDevices(base::Bind(&OnGetDevices, run.QuitClosure()));
    run.Run();
  }

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE, base::Bind(&RemoveDevice, kMouseId));
  observer.WaitForDeviceRemoval(kMouseId);

  {
    base::RunLoop run;
    proxy.GetDeviceInfo(kKeyboardId,
                        base::Bind(&OnGetKeyboard, run.QuitClosure()));
    run.Run();
  }

  {
    base::RunLoop run;
    proxy.GetDeviceInfo(kMouseId, base::Bind(&OnGetMouse, run.QuitClosure()));
    run.Run();
  }

  proxy.RemoveObserver(&observer);
}

}  // namespace chromeos
