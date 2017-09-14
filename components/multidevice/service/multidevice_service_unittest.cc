// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// #include "multidevice_service.h"

#include <memory>

#include "base/barrier_closure.h"
#include "base/run_loop.h"
#include "components/multidevice/service/public/interfaces/constants.mojom.h"
#include "components/multidevice/service/public/interfaces/device_sync.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/service_manager/public/cpp/service_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kServiceTestName[] = "multidevice_service_unittest";

enum class MultiDeviceServiceActionType {
  FORCE_ENROLLMENT_NOW,
  FORCE_SYNC_NOW
};

}  // namespace

namespace multidevice {

class MultiDeviceServiceTest : public service_manager::test::ServiceTest {
 public:
  class DeviceSyncObserverImpl : public device_sync::mojom::DeviceSyncObserver {
   public:
    DeviceSyncObserverImpl(
        device_sync::mojom::DeviceSyncObserverRequest request)
        : binding_(this, std::move(request)) {}

    void OnEnrollmentFinished(bool success) override {
      if (success) {
        num_times_enrollment_finished_called_.success_count++;
      } else {
        num_times_enrollment_finished_called_.failure_count++;
      }
      on_callback_invoked_->Run();
    }

    void OnDevicesSynced(bool success) override {
      if (success) {
        num_times_device_synced_.success_count++;
      } else {
        num_times_device_synced_.failure_count++;
      }
      on_callback_invoked_->Run();
    }

    // Sets the necessary callback that will be invoked upon each interface
    // method call in order to return control to the test.
    void SetOnCallbackInvokedClosure(base::Closure* on_callback_invoked) {
      on_callback_invoked_ = on_callback_invoked;
    }

    int GetNumCalls(MultiDeviceServiceActionType type, bool success_count) {
      switch (type) {
        case MultiDeviceServiceActionType::FORCE_ENROLLMENT_NOW:
          return num_times_enrollment_finished_called_.CountType(success_count);
        case MultiDeviceServiceActionType::FORCE_SYNC_NOW:
          return num_times_device_synced_.CountType(success_count);
        default:
          NOTREACHED();
      }
      return 0;
    }

   private:
    struct ObserverCallbackCount {
      int success_count = 0;
      int failure_count = 0;
      int CountType(bool success) {
        return success ? success_count : failure_count;
      }
    };

    mojo::Binding<device_sync::mojom::DeviceSyncObserver> binding_;
    base::Closure* on_callback_invoked_ = nullptr;

    ObserverCallbackCount num_times_enrollment_finished_called_;
    ObserverCallbackCount num_times_device_synced_;
  };

  MultiDeviceServiceTest() : ServiceTest(kServiceTestName){};

  ~MultiDeviceServiceTest() override{};

  void SetUp() override {
    ServiceTest::SetUp();
    connector()->BindInterface(multidevice::mojom::kServiceName,
                               &device_sync_ptr_);
  }

  void AddDeviceSyncObservers(int num) {
    device_sync::mojom::DeviceSyncObserverPtr device_sync_observer_ptr;
    for (int i = 0; i < num; i++) {
      device_sync_observer_ptr = device_sync::mojom::DeviceSyncObserverPtr();
      observers_.emplace_back(base::MakeUnique<DeviceSyncObserverImpl>(
          mojo::MakeRequest(&device_sync_observer_ptr)));
      device_sync_ptr_->AddObserver(std::move(device_sync_observer_ptr));
    }
  }

  void MultDeviceServiceAction(MultiDeviceServiceActionType type) {
    base::RunLoop run_loop;
    base::Closure closure = base::BarrierClosure(
        static_cast<int>(observers_.size()), run_loop.QuitClosure());
    for (auto& observer : observers_) {
      observer->SetOnCallbackInvokedClosure(&closure);
    }

    switch (type) {
      case MultiDeviceServiceActionType::FORCE_ENROLLMENT_NOW:
        device_sync_ptr_->ForceEnrollmentNow();
        break;
      case MultiDeviceServiceActionType::FORCE_SYNC_NOW:
        device_sync_ptr_->ForceSyncNow();
        break;
      default:
        NOTREACHED();
    }

    run_loop.Run();
  }

  device_sync::mojom::DeviceSyncPtr device_sync_ptr_;
  std::vector<std::unique_ptr<DeviceSyncObserverImpl>> observers_;
};

TEST_F(MultiDeviceServiceTest, MultipleCallTest) {
  AddDeviceSyncObservers(2);

  MultDeviceServiceAction(MultiDeviceServiceActionType::FORCE_ENROLLMENT_NOW);
  EXPECT_EQ(1, observers_[0]->GetNumCalls(
                   MultiDeviceServiceActionType::FORCE_ENROLLMENT_NOW,
                   true /* success_count */));
  EXPECT_EQ(1, observers_[1]->GetNumCalls(
                   MultiDeviceServiceActionType::FORCE_ENROLLMENT_NOW,
                   true /* success_count */));
  EXPECT_EQ(0, observers_[0]->GetNumCalls(
                   MultiDeviceServiceActionType::FORCE_ENROLLMENT_NOW,
                   false /* success_count */));
  EXPECT_EQ(0, observers_[1]->GetNumCalls(
                   MultiDeviceServiceActionType::FORCE_ENROLLMENT_NOW,
                   false /* success_count */));

  MultDeviceServiceAction(MultiDeviceServiceActionType::FORCE_SYNC_NOW);
  EXPECT_EQ(1, observers_[0]->GetNumCalls(
                   MultiDeviceServiceActionType::FORCE_SYNC_NOW,
                   true /* success_count */));
  EXPECT_EQ(1, observers_[1]->GetNumCalls(
                   MultiDeviceServiceActionType::FORCE_SYNC_NOW,
                   true /* success_count */));
  EXPECT_EQ(0, observers_[0]->GetNumCalls(
                   MultiDeviceServiceActionType::FORCE_SYNC_NOW,
                   false /* success_count */));
  EXPECT_EQ(0, observers_[1]->GetNumCalls(
                   MultiDeviceServiceActionType::FORCE_SYNC_NOW,
                   false /* success_count */));

  MultDeviceServiceAction(MultiDeviceServiceActionType::FORCE_ENROLLMENT_NOW);
  EXPECT_EQ(2, observers_[0]->GetNumCalls(
                   MultiDeviceServiceActionType::FORCE_ENROLLMENT_NOW,
                   true /* success_count */));
  EXPECT_EQ(2, observers_[1]->GetNumCalls(
                   MultiDeviceServiceActionType::FORCE_ENROLLMENT_NOW,
                   true /* success_count */));
  EXPECT_EQ(0, observers_[0]->GetNumCalls(
                   MultiDeviceServiceActionType::FORCE_ENROLLMENT_NOW,
                   false /* success_count */));
  EXPECT_EQ(0, observers_[1]->GetNumCalls(
                   MultiDeviceServiceActionType::FORCE_ENROLLMENT_NOW,
                   false /* success_count */));

  MultDeviceServiceAction(MultiDeviceServiceActionType::FORCE_SYNC_NOW);
  EXPECT_EQ(2, observers_[0]->GetNumCalls(
                   MultiDeviceServiceActionType::FORCE_SYNC_NOW,
                   true /* success_count */));
  EXPECT_EQ(2, observers_[1]->GetNumCalls(
                   MultiDeviceServiceActionType::FORCE_SYNC_NOW,
                   true /* success_count */));
  EXPECT_EQ(0, observers_[0]->GetNumCalls(
                   MultiDeviceServiceActionType::FORCE_SYNC_NOW,
                   false /* success_count */));
  EXPECT_EQ(0, observers_[1]->GetNumCalls(
                   MultiDeviceServiceActionType::FORCE_SYNC_NOW,
                   false /* success_count */));
}

}  // namespace multidevice
