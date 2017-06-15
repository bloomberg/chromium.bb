// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/night_light/night_light_client.h"

#include "ash/public/interfaces/night_light_controller.mojom.h"
#include "base/test/scoped_task_environment.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ScheduleType = ash::mojom::NightLightController::ScheduleType;

// A fake implementation of NightLightController for testing.
class FakeNightLightController : public ash::mojom::NightLightController {
 public:
  FakeNightLightController() : binding_(this) {}
  ~FakeNightLightController() override = default;

  int position_pushes_num() const { return position_pushes_num_; }

  ash::mojom::NightLightControllerPtr CreateInterfacePtrAndBind() {
    ash::mojom::NightLightControllerPtr ptr;
    binding_.Bind(mojo::MakeRequest(&ptr));
    return ptr;
  }

  // ash::mojom::NightLightController:
  void SetCurrentGeoposition(
      ash::mojom::SimpleGeopositionPtr position) override {
    position_ = std::move(position);
    ++position_pushes_num_;
  }

  void SetClient(ash::mojom::NightLightClientPtr client) override {
    client_ = std::move(client);
  }

  void NotifyScheduleTypeChanged(ScheduleType type) {
    client_->OnScheduleTypeChanged(type);
    client_.FlushForTesting();
  }

 private:
  ash::mojom::SimpleGeopositionPtr position_;
  ash::mojom::NightLightClientPtr client_;
  mojo::Binding<ash::mojom::NightLightController> binding_;

  // The number of times a new position is pushed to this controller.
  int position_pushes_num_ = 0;

  DISALLOW_COPY_AND_ASSIGN(FakeNightLightController);
};

// A fake implementation of NightLightClient that doesn't perform any actual
// geoposition requests.
class FakeNightLightClient : public NightLightClient {
 public:
  FakeNightLightClient() : NightLightClient(nullptr /* url_context_getter */) {}
  ~FakeNightLightClient() override = default;

  void set_position_to_send(const chromeos::Geoposition& position) {
    position_to_send_ = position;
  }

 private:
  // night_light::NightLightClient:
  void RequestGeoposition() override {
    OnGeoposition(position_to_send_, false, base::TimeDelta());
  }

  // The position to send to the controller the next time OnGeoposition is
  // invoked.
  chromeos::Geoposition position_to_send_;

  DISALLOW_COPY_AND_ASSIGN(FakeNightLightClient);
};

// Base test fixture.
class NightLightClientTest : public testing::Test {
 public:
  NightLightClientTest() = default;
  ~NightLightClientTest() override = default;

  void SetUp() override {
    client_.SetNightLightControllerPtrForTesting(
        controller_.CreateInterfacePtrAndBind());
    client_.Start();
    client_.FlushNightLightControllerForTesting();
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;

  FakeNightLightController controller_;
  FakeNightLightClient client_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NightLightClientTest);
};

// Test that the client is retrieving geoposition periodically only when the
// schedule type is "sunset to sunrise".
TEST_F(NightLightClientTest, TestClientRunningOnlyWhenSunsetToSunriseSchedule) {
  EXPECT_FALSE(client_.using_geoposition());
  controller_.NotifyScheduleTypeChanged(ScheduleType::kNone);
  EXPECT_FALSE(client_.using_geoposition());
  controller_.NotifyScheduleTypeChanged(ScheduleType::kCustom);
  controller_.NotifyScheduleTypeChanged(ScheduleType::kSunsetToSunrise);
  scoped_task_environment_.RunUntilIdle();
  client_.FlushNightLightControllerForTesting();
  EXPECT_TRUE(client_.using_geoposition());

  // Client should stop retrieving geopositions when schedule type changes to
  // something else.
  controller_.NotifyScheduleTypeChanged(ScheduleType::kNone);
  EXPECT_FALSE(client_.using_geoposition());
}

// Test that client only pushes valid positions.
TEST_F(NightLightClientTest, TestPositionPushes) {
  // Start with a valid position, and expect it to be delivered to the
  // controller.
  EXPECT_EQ(0, controller_.position_pushes_num());
  chromeos::Geoposition position;
  position.latitude = 32.0;
  position.longitude = 31.0;
  position.status = chromeos::Geoposition::STATUS_OK;
  position.accuracy = 10;
  position.timestamp = base::Time::Now();
  client_.set_position_to_send(position);
  controller_.NotifyScheduleTypeChanged(ScheduleType::kSunsetToSunrise);
  scoped_task_environment_.RunUntilIdle();
  client_.FlushNightLightControllerForTesting();
  EXPECT_EQ(1, controller_.position_pushes_num());

  // Invalid positions should not be sent.
  position.status = chromeos::Geoposition::STATUS_TIMEOUT;
  client_.set_position_to_send(position);
  controller_.NotifyScheduleTypeChanged(ScheduleType::kSunsetToSunrise);
  scoped_task_environment_.RunUntilIdle();
  client_.FlushNightLightControllerForTesting();
  EXPECT_EQ(1, controller_.position_pushes_num());
}

}  // namespace
