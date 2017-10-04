// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/device/input_service_test_helper.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/device/input_service_proxy.h"
#include "device/hid/fake_input_service_linux.h"
#include "testing/gtest/include/gtest/gtest.h"

using device::InputServiceLinux;
using device::FakeInputServiceLinux;

namespace chromeos {

typedef device::mojom::InputDeviceInfoPtr InputDeviceInfoPtr;

namespace {

void InitInputServiceOnInputServiceSequence() {
  InputServiceLinux::SetForTesting(base::MakeUnique<FakeInputServiceLinux>());
}

void AddDeviceOnInputServiceSequence(InputDeviceInfoPtr device) {
  FakeInputServiceLinux* service =
      static_cast<FakeInputServiceLinux*>(InputServiceLinux::GetInstance());
  service->AddDeviceForTesting(std::move(device));
}

void RemoveDeviceOnInputServiceSequence(const std::string& id) {
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

  void WaitForDeviceAddition(InputDeviceInfoPtr info) {
    base::RunLoop run;
    expected_info_ = std::move(info);
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
  static bool Equals(const InputDeviceInfoPtr& lhs,
                     const InputDeviceInfoPtr& rhs) {
    return lhs->id == rhs->id && lhs->name == rhs->name &&
           lhs->subsystem == rhs->subsystem && lhs->type == rhs->type &&
           lhs->is_accelerometer == rhs->is_accelerometer &&
           lhs->is_joystick == rhs->is_joystick && lhs->is_key == rhs->is_key &&
           lhs->is_keyboard == rhs->is_keyboard &&
           lhs->is_mouse == rhs->is_mouse && lhs->is_tablet == rhs->is_tablet &&
           lhs->is_touchpad == rhs->is_touchpad &&
           lhs->is_touchscreen == rhs->is_touchscreen;
  }

  // InputServiceProxy::Observer implementation.
  void OnInputDeviceAdded(InputDeviceInfoPtr info) override {
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

  InputDeviceInfoPtr expected_info_;
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
  InputServiceProxy::GetInputServiceTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&InitInputServiceOnInputServiceSequence));
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

void InputServiceTestHelper::AddDeviceToService(
    bool is_mouse,
    device::mojom::InputDeviceType type) {
  InputDeviceInfoPtr device = device::mojom::InputDeviceInfo::New();
  device->id = is_mouse ? kMouseId : kKeyboardId;
  device->subsystem = device::mojom::InputDeviceSubsystem::SUBSYSTEM_INPUT;
  device->type = type;
  device->is_mouse = is_mouse;
  device->is_keyboard = !is_mouse;
  InputServiceProxy::GetInputServiceTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&AddDeviceOnInputServiceSequence, device.Clone()));
  observer_->WaitForDeviceAddition(std::move(device));
}

void InputServiceTestHelper::RemoveDeviceFromService(bool is_mouse) {
  std::string id = is_mouse ? kMouseId : kKeyboardId;
  InputServiceProxy::GetInputServiceTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&RemoveDeviceOnInputServiceSequence, id));
  observer_->WaitForDeviceRemoval(id);
}

}  // namespace chromeos
