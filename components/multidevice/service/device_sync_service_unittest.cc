// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "components/multidevice/service/device_sync_service.h"
#include "components/multidevice/service/fake_device_sync_observer.h"
#include "components/multidevice/service/public/interfaces/constants.mojom.h"
#include "components/multidevice/service/public/interfaces/device_sync.mojom.h"
#include "services/service_manager/public/cpp/test/test_connector_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device_sync {

namespace {

class FakeDeviceSyncObserverDelegate : public FakeDeviceSyncObserver::Delegate {
 public:
  FakeDeviceSyncObserverDelegate(
      const base::Closure& on_delegate_function_called_closure)
      : on_delegate_function_called_closure_(
            on_delegate_function_called_closure) {}
  ~FakeDeviceSyncObserverDelegate() = default;

 private:
  // FakeDeviceSyncObserver::Delegate:
  void OnEnrollmentFinishedCalled() override {
    on_delegate_function_called_closure_.Run();
  }

  void OnDevicesSyncedCalled() override {
    on_delegate_function_called_closure_.Run();
  }

  base::Closure on_delegate_function_called_closure_;
};

}  // namespace

class DeviceSyncServiceTest : public testing::Test {
 public:
  DeviceSyncServiceTest() = default;
  ~DeviceSyncServiceTest() override = default;

  void SetUp() override {
    fake_device_sync_observer_ = std::make_unique<FakeDeviceSyncObserver>();
    connector_factory_ =
        service_manager::TestConnectorFactory::CreateForUniqueService(
            std::make_unique<DeviceSyncService>());
  }

  device_sync::mojom::DeviceSync* GetDeviceSync() {
    if (!device_sync_) {
      EXPECT_EQ(nullptr, connector_);

      // Create the Connector and bind it to |device_sync_|.
      connector_ = connector_factory_->CreateConnector();
      connector_->BindInterface(device_sync::mojom::kServiceName,
                                &device_sync_);

      // Set |fake_device_sync_observer_|.
      CallAddObserver();
    }

    return device_sync_.get();
  }

  FakeDeviceSyncObserver* fake_device_sync_observer() {
    return fake_device_sync_observer_.get();
  }

  void CallAddObserver() {
    base::RunLoop run_loop;
    GetDeviceSync()->AddObserver(
        fake_device_sync_observer_->GenerateInterfacePtr(),
        base::BindOnce(&DeviceSyncServiceTest::OnObserverRegistered,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  void CallForceEnrollmentNow() {
    base::RunLoop run_loop;

    FakeDeviceSyncObserverDelegate delegate(run_loop.QuitClosure());
    fake_device_sync_observer()->SetDelegate(&delegate);

    GetDeviceSync()->ForceEnrollmentNow();
    run_loop.Run();

    fake_device_sync_observer()->SetDelegate(nullptr);
  }

  void CallForceSyncNow() {
    base::RunLoop run_loop;

    FakeDeviceSyncObserverDelegate delegate(run_loop.QuitClosure());
    fake_device_sync_observer()->SetDelegate(&delegate);

    GetDeviceSync()->ForceSyncNow();
    run_loop.Run();

    fake_device_sync_observer()->SetDelegate(nullptr);
  }

 private:
  void OnObserverRegistered(base::OnceClosure quit_closure) {
    std::move(quit_closure).Run();
  }

  const base::test::ScopedTaskEnvironment scoped_task_environment_;

  std::unique_ptr<service_manager::TestConnectorFactory> connector_factory_;
  std::unique_ptr<service_manager::Connector> connector_;

  std::unique_ptr<FakeDeviceSyncObserver> fake_device_sync_observer_;
  device_sync::mojom::DeviceSyncPtr device_sync_;

  DISALLOW_COPY_AND_ASSIGN(DeviceSyncServiceTest);
};

TEST_F(DeviceSyncServiceTest, TestForceEnrollment) {
  // Note: Currently, ForceEnrollmentNow() instantly replies with success ==
  //       true. Enrollment will be implemented in a future CL.
  CallForceEnrollmentNow();
  EXPECT_EQ(1u, fake_device_sync_observer()->num_enrollment_success_events());

  CallForceEnrollmentNow();
  EXPECT_EQ(2u, fake_device_sync_observer()->num_enrollment_success_events());
}

TEST_F(DeviceSyncServiceTest, TestForceSync) {
  // Note: Currently, ForceSyncNow() instantly replies with success == true.
  //       Syncing will be implemented in a future CL.
  CallForceSyncNow();
  EXPECT_EQ(1u, fake_device_sync_observer()->num_sync_success_events());

  CallForceSyncNow();
  EXPECT_EQ(2u, fake_device_sync_observer()->num_sync_success_events());
}

}  // namespace device_sync
