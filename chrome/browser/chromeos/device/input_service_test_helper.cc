// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/device/input_service_test_helper.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/device/input_service_proxy.h"
#include "content/public/browser/browser_thread.h"
#include "device/hid/fake_input_service_linux.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using device::InputServiceLinux;
using device::FakeInputServiceLinux;

namespace chromeos {

namespace {

void InitInputServiceOnFileThread() {
  InputServiceLinux::SetForTesting(base::MakeUnique<FakeInputServiceLinux>());
}

void AddDeviceOnFileThread(const InputDeviceInfo& device) {
  FakeInputServiceLinux* service =
      static_cast<FakeInputServiceLinux*>(InputServiceLinux::GetInstance());
  service->AddDeviceForTesting(device);
}

void RemoveDeviceOnFileThread(const std::string& id) {
  FakeInputServiceLinux* service =
      static_cast<FakeInputServiceLinux*>(InputServiceLinux::GetInstance());
  service->RemoveDeviceForTesting(id);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// TestObserver

class TestObserver : public InputServiceProxy::Observer {
 public:
  TestObserver()
      : wait_for_device_addition_(false), wait_for_device_removal_(false) {}

  ~TestObserver() override {}

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
           lhs.subsystem == rhs.subsystem && lhs.type == rhs.type &&
           lhs.is_accelerometer == rhs.is_accelerometer &&
           lhs.is_joystick == rhs.is_joystick && lhs.is_key == rhs.is_key &&
           lhs.is_keyboard == rhs.is_keyboard && lhs.is_mouse == rhs.is_mouse &&
           lhs.is_tablet == rhs.is_tablet &&
           lhs.is_touchpad == rhs.is_touchpad &&
           lhs.is_touchscreen == rhs.is_touchscreen;
  }

  // InputServiceProxy::Observer implementation.
  void OnInputDeviceAdded(const InputDeviceInfo& info) override {
    if (!wait_for_device_addition_)
      return;
    EXPECT_TRUE(Equals(expected_info_, info));
    done_.Run();
  }

  void OnInputDeviceRemoved(const std::string& id) override {
    if (!wait_for_device_removal_)
      return;
    EXPECT_EQ(expected_id_, id);
    done_.Run();
  }

  InputDeviceInfo expected_info_;
  std::string expected_id_;

  bool wait_for_device_addition_;
  bool wait_for_device_removal_;

  base::Closure done_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

////////////////////////////////////////////////////////////////////////////////
// InputServiceTestHelper

// static
const char InputServiceTestHelper::kKeyboardId[] = "keyboard";
const char InputServiceTestHelper::kMouseId[] = "mouse";

InputServiceTestHelper::InputServiceTestHelper() : observer_(new TestObserver) {
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                          base::Bind(&InitInputServiceOnFileThread));
}

InputServiceTestHelper::~InputServiceTestHelper() {}

void InputServiceTestHelper::SetProxy(InputServiceProxy* proxy) {
  proxy_ = proxy;
  proxy_->AddObserver(observer_.get());
}

void InputServiceTestHelper::ClearProxy() {
  proxy_->RemoveObserver(observer_.get());
  proxy_ = nullptr;
}

void InputServiceTestHelper::AddDeviceToService(bool is_mouse,
                                                InputDeviceInfo::Type type) {
  InputDeviceInfo device;
  device.id = is_mouse ? kMouseId : kKeyboardId;
  device.subsystem = InputDeviceInfo::SUBSYSTEM_INPUT;
  device.type = type;
  device.is_mouse = is_mouse;
  device.is_keyboard = !is_mouse;
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                          base::Bind(&AddDeviceOnFileThread, device));
  observer_->WaitForDeviceAddition(device);
}

void InputServiceTestHelper::RemoveDeviceFromService(bool is_mouse) {
  std::string id = is_mouse ? kMouseId : kKeyboardId;
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                          base::Bind(&RemoveDeviceOnFileThread, id));
  observer_->WaitForDeviceRemoval(id);
}

}  // namespace chromeos
