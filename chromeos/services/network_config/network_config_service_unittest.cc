// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/network_config/network_config_service.h"

#include "base/test/scoped_task_environment.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_test_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace network_config {

class NetworkConfigServiceTest : public testing::Test {
 public:
  NetworkConfigServiceTest() = default;
  ~NetworkConfigServiceTest() override = default;

  CrosNetworkConfigTestHelper& helper() { return helper_; }
  NetworkStateTestHelper& network_state_helper() {
    return helper_.network_state_helper();
  }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  CrosNetworkConfigTestHelper helper_;

  DISALLOW_COPY_AND_ASSIGN(NetworkConfigServiceTest);
};

TEST_F(NetworkConfigServiceTest, ServiceInterface) {
  helper().SetupServiceInterface();

  // Ensure that a call to helper().service_interface() works. The default
  // NetworkStateTestHelper configuration includes a single (wifi) device.
  base::RunLoop run_loop;
  helper().service_interface_ptr()->GetDeviceStateList(base::BindOnce(
      [](base::OnceClosure quit_closure,
         std::vector<mojom::DeviceStatePropertiesPtr> devices) {
        EXPECT_EQ(1u, devices.size());
        std::move(quit_closure).Run();
      },
      run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(NetworkConfigServiceTest, Observer) {
  helper().SetupServiceInterface();
  helper().SetupObserver();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, helper().observer()->device_state_list_changed());

  // Ensure that disabling the wifi device triggers the observer.
  network_state_helper().network_state_handler()->SetTechnologyEnabled(
      NetworkTypePattern::WiFi(), false, network_handler::ErrorCallback());
  base::RunLoop().RunUntilIdle();  // Complete NetworkHandler callbacks
  helper().FlushForTesting();
  EXPECT_EQ(1, helper().observer()->device_state_list_changed());
}

}  // namespace network_config
}  // namespace chromeos
