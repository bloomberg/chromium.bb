// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/accessibility/accessibility_controller_client.h"

#include "ash/public/interfaces/accessibility_controller.mojom.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/time/time.h"
#include "chromeos/audio/chromeos_sounds.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr base::TimeDelta kShutdownSoundDuration =
    base::TimeDelta::FromMilliseconds(1000);

void CopyResult(base::TimeDelta* dest, base::TimeDelta src) {
  *dest = src;
}

class TestAccessibilityController : ash::mojom::AccessibilityController {
 public:
  TestAccessibilityController() : binding_(this) {}
  ~TestAccessibilityController() override = default;

  ash::mojom::AccessibilityControllerPtr CreateInterfacePtr() {
    ash::mojom::AccessibilityControllerPtr ptr;
    binding_.Bind(mojo::MakeRequest(&ptr));
    return ptr;
  }

  // ash::mojom::AccessibilityController:
  void SetClient(ash::mojom::AccessibilityControllerClientPtr client) override {
    was_client_set_ = true;
  }

  bool was_client_set() const { return was_client_set_; }

 private:
  mojo::Binding<ash::mojom::AccessibilityController> binding_;
  bool was_client_set_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestAccessibilityController);
};

class FakeAccessibilityControllerClient : public AccessibilityControllerClient {
 public:
  FakeAccessibilityControllerClient() = default;
  ~FakeAccessibilityControllerClient() override = default;

  // AccessibilityControllerClient:
  void TriggerAccessibilityAlert(
      ash::mojom::AccessibilityAlert alert) override {
    last_a11y_alert_ = alert;
  }

  void PlayEarcon(int32_t sound_key) override { last_sound_key_ = sound_key; }

  void PlayShutdownSound(PlayShutdownSoundCallback callback) override {
    std::move(callback).Run(kShutdownSoundDuration);
  }

  ash::mojom::AccessibilityAlert last_a11y_alert() const {
    return last_a11y_alert_;
  }
  int32_t last_sound_key() const { return last_sound_key_; }

 private:
  ash::mojom::AccessibilityAlert last_a11y_alert_ =
      ash::mojom::AccessibilityAlert::NONE;
  int32_t last_sound_key_ = -1;

  DISALLOW_COPY_AND_ASSIGN(FakeAccessibilityControllerClient);
};

}  // namespace

class AccessibilityControllerClientTest : public testing::Test {
 public:
  AccessibilityControllerClientTest() = default;
  ~AccessibilityControllerClientTest() override = default;

 private:
  base::test::ScopedTaskEnvironment scoped_task_enviroment_;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityControllerClientTest);
};

TEST_F(AccessibilityControllerClientTest, MethodCalls) {
  FakeAccessibilityControllerClient client;
  TestAccessibilityController controller;
  client.InitForTesting(controller.CreateInterfacePtr());
  client.FlushForTesting();

  // Tests client is set.
  EXPECT_TRUE(controller.was_client_set());

  // Tests TriggerAccessibilityAlert method call.
  const ash::mojom::AccessibilityAlert alert =
      ash::mojom::AccessibilityAlert::SCREEN_ON;
  client.TriggerAccessibilityAlert(alert);
  EXPECT_EQ(alert, client.last_a11y_alert());

  // Tests PlayEarcon method call.
  const int32_t sound_key = chromeos::SOUND_SHUTDOWN;
  client.PlayEarcon(sound_key);
  EXPECT_EQ(sound_key, client.last_sound_key());

  // Tests PlayShutdownSound method call.
  base::TimeDelta sound_duration;
  client.PlayShutdownSound(
      base::Bind(&CopyResult, base::Unretained(&sound_duration)));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kShutdownSoundDuration, sound_duration);
}
