// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/magnifier/docked_magnifier_controller.h"

#include <memory>

#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/interfaces/docked_magnifier_controller.mojom.h"
#include "ash/session/session_controller.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace ash {

namespace {

constexpr char kUser1Email[] = "user1@dockedmagnifier";
constexpr char kUser2Email[] = "user2@dockedmagnifier";

// Mock mojo client of the Docked Magnifier.
class DockedMagnifierTestClient : public mojom::DockedMagnifierClient {
 public:
  DockedMagnifierTestClient() : binding_(this) {}
  ~DockedMagnifierTestClient() override = default;

  bool docked_magnifier_enabled() const { return docked_magnifier_enabled_; }

  // Connects to the DockedMagnifierController.
  void Start() {
    ash::mojom::DockedMagnifierClientPtr client;
    binding_.Bind(mojo::MakeRequest(&client));
    Shell::Get()->docked_magnifier_controller()->SetClient(std::move(client));
  }

  // ash::mojom::DockedMagnifierClient:
  void OnEnabledStatusChanged(bool enabled) override {
    docked_magnifier_enabled_ = enabled;
  }

 private:
  mojo::Binding<ash::mojom::DockedMagnifierClient> binding_;

  bool docked_magnifier_enabled_ = false;

  DISALLOW_COPY_AND_ASSIGN(DockedMagnifierTestClient);
};

class DockedMagnifierTest : public NoSessionAshTestBase {
 public:
  DockedMagnifierTest() = default;
  ~DockedMagnifierTest() override = default;

  DockedMagnifierController* controller() const {
    return Shell::Get()->docked_magnifier_controller();
  }

  DockedMagnifierTestClient* test_client() { return &test_client_; }

  PrefService* user1_pref_service() {
    return Shell::Get()->session_controller()->GetUserPrefServiceForUser(
        AccountId::FromUserEmail(kUser1Email));
  }

  PrefService* user2_pref_service() {
    return Shell::Get()->session_controller()->GetUserPrefServiceForUser(
        AccountId::FromUserEmail(kUser2Email));
  }

  // AshTestBase:
  void SetUp() override {
    // Explicitly enable the Docked Magnifier feature for the tests.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ash::switches::kAshEnableDockedMagnifier);

    NoSessionAshTestBase::SetUp();
    CreateTestUserSessions();

    // Simulate user 1 login.
    SwitchActiveUser(kUser1Email);

    test_client_.Start();
  }

  void CreateTestUserSessions() {
    GetSessionControllerClient()->Reset();
    GetSessionControllerClient()->AddUserSession(kUser1Email);
    GetSessionControllerClient()->AddUserSession(kUser2Email);
  }

  void SwitchActiveUser(const std::string& email) {
    GetSessionControllerClient()->SwitchActiveUser(
        AccountId::FromUserEmail(email));
  }

 private:
  DockedMagnifierTestClient test_client_;

  DISALLOW_COPY_AND_ASSIGN(DockedMagnifierTest);
};

// Tests the changes in the magnifier's status, user switches and the
// interaction with the client.
TEST_F(DockedMagnifierTest, TestEnableAndDisable) {
  // Client should receive status updates.
  EXPECT_FALSE(controller()->GetEnabled());
  EXPECT_FALSE(test_client()->docked_magnifier_enabled());
  controller()->SetEnabled(true);
  controller()->FlushClientPtrForTesting();
  EXPECT_TRUE(controller()->GetEnabled());
  EXPECT_TRUE(test_client()->docked_magnifier_enabled());
  controller()->SetEnabled(false);
  controller()->FlushClientPtrForTesting();
  EXPECT_FALSE(controller()->GetEnabled());
  EXPECT_FALSE(test_client()->docked_magnifier_enabled());

  // Enable again for user 1, and switch to user 2. User 2 should have it
  // disabled, the client should be updated accordingly.
  controller()->SetEnabled(true);
  controller()->FlushClientPtrForTesting();
  EXPECT_TRUE(controller()->GetEnabled());
  EXPECT_TRUE(test_client()->docked_magnifier_enabled());
  SwitchActiveUser(kUser2Email);
  controller()->FlushClientPtrForTesting();
  EXPECT_FALSE(controller()->GetEnabled());
  EXPECT_FALSE(test_client()->docked_magnifier_enabled());

  // Switch back to user 1, expect it to be enabled.
  SwitchActiveUser(kUser1Email);
  controller()->FlushClientPtrForTesting();
  EXPECT_TRUE(controller()->GetEnabled());
  EXPECT_TRUE(test_client()->docked_magnifier_enabled());
}

// Tests the magnifier's scale changes.
TEST_F(DockedMagnifierTest, TestScale) {
  // Scale changes are persisted even when the Docked Magnifier is disabled.
  EXPECT_FALSE(controller()->GetEnabled());
  controller()->SetScale(5.0f);
  EXPECT_FLOAT_EQ(5.0f, controller()->GetScale());

  // Scale values are clamped to a minimum of 1.0f (which means no scale).
  controller()->SetScale(0.0f);
  EXPECT_FLOAT_EQ(1.0f, controller()->GetScale());

  // Switch to user 2, change the scale, then switch back to user 1. User 1's
  // scale should not change.
  SwitchActiveUser(kUser2Email);
  controller()->SetScale(6.5f);
  EXPECT_FLOAT_EQ(6.5f, controller()->GetScale());
  SwitchActiveUser(kUser1Email);
  EXPECT_FLOAT_EQ(1.0f, controller()->GetScale());
}

// Tests that updates of the Docked Magnifier user prefs from outside the
// DockedMagnifierController (such as Settings UI) are observed and applied.
TEST_F(DockedMagnifierTest, TestOutsidePrefsUpdates) {
  EXPECT_FALSE(controller()->GetEnabled());
  EXPECT_FALSE(test_client()->docked_magnifier_enabled());
  user1_pref_service()->SetBoolean(prefs::kDockedMagnifierEnabled, true);
  controller()->FlushClientPtrForTesting();
  EXPECT_TRUE(controller()->GetEnabled());
  EXPECT_TRUE(test_client()->docked_magnifier_enabled());
  user1_pref_service()->SetDouble(prefs::kDockedMagnifierScale, 7.3f);
  EXPECT_FLOAT_EQ(7.3f, controller()->GetScale());
  user1_pref_service()->SetBoolean(prefs::kDockedMagnifierEnabled, false);
  controller()->FlushClientPtrForTesting();
  EXPECT_FALSE(controller()->GetEnabled());
  EXPECT_FALSE(test_client()->docked_magnifier_enabled());
}

// TODO(afakhry): Expand tests:
// - Test magnifier viewport's layer transforms.
// - Test display workarea changes.
// - Test with screen rotation, multi display, and unified mode.
// - Test point of interest changes.
// - Test following text input caret.
// - Test adjust scale using shortcut.
// - Test add/remove displays.

}  // namespace

}  // namespace ash
