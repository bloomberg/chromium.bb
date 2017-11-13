// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/accessibility/accessibility_controller_client.h"

#include "ash/public/interfaces/accessibility_controller.mojom.h"
#include "base/macros.h"
#include "base/test/scoped_task_environment.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

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

}  // namespace

class AccessibilityControllerClientTest : public testing::Test {
 public:
  AccessibilityControllerClientTest() = default;
  ~AccessibilityControllerClientTest() override = default;

 private:
  base::test::ScopedTaskEnvironment scoped_task_enviroment_;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityControllerClientTest);
};

TEST_F(AccessibilityControllerClientTest, TriggerAccessibilityAlert) {
  AccessibilityControllerClient client;
  TestAccessibilityController controller;
  client.InitForTesting(controller.CreateInterfacePtr());
  client.FlushForTesting();

  // Tests that singleton was initialized and client is set.
  EXPECT_EQ(&client, AccessibilityControllerClient::Get());
  EXPECT_TRUE(controller.was_client_set());

  // Tests TriggerAccessibilityAlert method call.
  const ash::mojom::AccessibilityAlert alert =
      ash::mojom::AccessibilityAlert::SCREEN_ON;
  client.TriggerAccessibilityAlert(alert);
  EXPECT_EQ(alert, client.last_a11y_alert_for_test());
}