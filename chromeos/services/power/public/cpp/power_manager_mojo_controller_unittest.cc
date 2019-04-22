// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/power/public/cpp/power_manager_mojo_controller.h"

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/services/power/public/cpp/power_manager_mojo_client.h"
#include "chromeos/services/power/public/mojom/power_manager.mojom.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

class TestObserver : public PowerManagerClient::Observer {
 public:
  TestObserver() = default;
  ~TestObserver() override = default;

  void WaitForChange() { run_loop_.Run(); }

  // PowerManagerClient::Observer:
  void ScreenBrightnessChanged(
      const power_manager::BacklightBrightnessChange& change) override {
    last_change_ = change;
    run_loop_.Quit();
  }

  const base::Optional<power_manager::BacklightBrightnessChange>&
  last_change() {
    return last_change_;
  }

 private:
  base::Optional<power_manager::BacklightBrightnessChange> last_change_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

class PowerManagerMojoTest : public testing::Test {
 public:
  PowerManagerMojoTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kMojoDBusRelay);
    PowerManagerClient::InitializeFake();

    controller_ = std::make_unique<PowerManagerMojoController>();
    client_ = std::make_unique<PowerManagerMojoClient>();

    power::mojom::PowerManagerObserverAssociatedPtr observer_ptr;
    binding_ = std::make_unique<
        mojo::AssociatedBinding<power::mojom::PowerManagerObserver>>(
        client_.get(),
        mojo::MakeRequestAssociatedWithDedicatedPipe(&observer_ptr));
    controller_->SetObserver(observer_ptr.PassInterface());
    base::RunLoop().RunUntilIdle();
  }

  ~PowerManagerMojoTest() override {
    binding_.reset();
    client_.reset();
    controller_.reset();
    PowerManagerClient::Shutdown();
  }

 protected:
  std::unique_ptr<PowerManagerMojoController> controller_;
  std::unique_ptr<PowerManagerMojoClient> client_;

 private:
  std::unique_ptr<mojo::AssociatedBinding<power::mojom::PowerManagerObserver>>
      binding_;

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(PowerManagerMojoTest);
};

TEST_F(PowerManagerMojoTest, SetScreenBrightness) {
  TestObserver observer;
  client_->AddObserver(&observer);

  // TODO(estade): also verify |cause|.
  power_manager::SetBacklightBrightnessRequest brightness_request;
  brightness_request.set_percent(0.6);
  chromeos::FakePowerManagerClient::Get()->SetScreenBrightness(
      brightness_request);
  observer.WaitForChange();
  EXPECT_EQ(0.6, observer.last_change()->percent());

  // TODO(estade): add SetScreenBrightness to PowerManagerMojoController and
  // test it here.
}

}  // namespace

}  // namespace chromeos
