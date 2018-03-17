// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "chromeos/services/multidevice_setup/fake_multidevice_setup_observer.h"
#include "chromeos/services/multidevice_setup/multidevice_setup_service.h"
#include "chromeos/services/multidevice_setup/public/mojom/constants.mojom.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "services/service_manager/public/cpp/test/test_connector_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace multidevice {

using EventType = multidevice_setup::mojom::EventTypeForDebugging;

class MultiDeviceSetupServiceTest : public testing::Test {
 protected:
  MultiDeviceSetupServiceTest() = default;
  ~MultiDeviceSetupServiceTest() override = default;

  void SetUp() override {
    fake_multidevice_setup_observer_ =
        std::make_unique<FakeMultiDeviceSetupObserver>();
    connector_factory_ =
        service_manager::TestConnectorFactory::CreateForUniqueService(
            std::make_unique<MultiDeviceSetupService>());
  }

  multidevice_setup::mojom::MultiDeviceSetup* GetMultiDeviceSetup() {
    if (!multidevice_setup_) {
      EXPECT_EQ(nullptr, connector_);

      // Create the Connector and bind it to |multidevice_setup_|.
      connector_ = connector_factory_->CreateConnector();
      connector_->BindInterface(multidevice_setup::mojom::kServiceName,
                                &multidevice_setup_);

      // Set |fake_multidevice_setup_observer_|.
      CallSetObserver();
    }

    return multidevice_setup_.get();
  }

  FakeMultiDeviceSetupObserver* fake_multidevice_setup_observer() {
    return fake_multidevice_setup_observer_.get();
  }

  void CallSetObserver() {
    base::RunLoop run_loop;
    multidevice_setup_->SetObserver(
        fake_multidevice_setup_observer_->GenerateInterfacePtr(),
        base::BindRepeating(
            &MultiDeviceSetupServiceTest::OnNotificationPresenterRegistered,
            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  void CallTriggerEventForDebugging(EventType type) {
    base::RunLoop run_loop;
    GetMultiDeviceSetup()->TriggerEventForDebugging(
        type, base::BindRepeating(
                  &MultiDeviceSetupServiceTest::OnNotificationTriggered,
                  base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

 private:
  void OnNotificationPresenterRegistered(
      const base::RepeatingClosure& quit_closure) {
    quit_closure.Run();
  }

  void OnNotificationTriggered(const base::RepeatingClosure& quit_closure,
                               bool success) {
    // NotificationPresenter is set in GetMultiDeviceSetup().
    EXPECT_TRUE(success);
    quit_closure.Run();
  }

  const base::test::ScopedTaskEnvironment scoped_task_environment_;

  std::unique_ptr<service_manager::TestConnectorFactory> connector_factory_;
  std::unique_ptr<service_manager::Connector> connector_;

  std::unique_ptr<FakeMultiDeviceSetupObserver>
      fake_multidevice_setup_observer_;
  multidevice_setup::mojom::MultiDeviceSetupPtr multidevice_setup_;

  DISALLOW_COPY_AND_ASSIGN(MultiDeviceSetupServiceTest);
};

TEST_F(MultiDeviceSetupServiceTest,
       TriggerEventForDebugging_kNewUserPotentialHostExists) {
  CallTriggerEventForDebugging(EventType::kNewUserPotentialHostExists);

  EXPECT_EQ(1u,
            fake_multidevice_setup_observer()->num_new_user_events_handled());
}

TEST_F(MultiDeviceSetupServiceTest,
       TriggerEventForDebugging_kExistingUserConnectedHostSwitched) {
  CallTriggerEventForDebugging(EventType::kExistingUserConnectedHostSwitched);

  EXPECT_EQ(1u, fake_multidevice_setup_observer()
                    ->num_existing_user_host_switched_events_handled());
}

TEST_F(MultiDeviceSetupServiceTest,
       TriggerEventForDebugging_kExistingUserNewChromebookAdded) {
  CallTriggerEventForDebugging(EventType::kExistingUserNewChromebookAdded);

  EXPECT_EQ(1u, fake_multidevice_setup_observer()
                    ->num_existing_user_chromebook_added_events_handled());
}

}  // namespace multidevice

}  // namespace chromeos
