// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard/ash_keyboard_controller.h"

#include <memory>
#include <vector>

#include "ash/public/interfaces/keyboard_controller.mojom.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/test/scoped_task_environment.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "services/service_manager/public/cpp/connector.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/keyboard/container_behavior.h"
#include "ui/keyboard/keyboard_controller.h"

using keyboard::mojom::KeyboardConfig;
using keyboard::mojom::KeyboardConfigPtr;
using keyboard::mojom::KeyboardEnableFlag;

namespace ash {

namespace {

class TestObserver : public mojom::KeyboardControllerObserver {
 public:
  explicit TestObserver(mojom::KeyboardController* controller) {
    mojom::KeyboardControllerObserverAssociatedPtrInfo ptr_info;
    keyboard_controller_observer_binding_.Bind(mojo::MakeRequest(&ptr_info));
    controller->AddObserver(std::move(ptr_info));
  }
  ~TestObserver() override = default;

  // mojom::KeyboardControllerObserver:
  void OnKeyboardEnableFlagsChanged(
      const std::vector<keyboard::mojom::KeyboardEnableFlag>& flags) override {
    enable_flags_ = flags;
  }
  void OnKeyboardEnabledChanged(bool enabled) override {
    if (!enabled)
      ++destroyed_count_;
  }
  void OnKeyboardVisibilityChanged(bool visible) override {}
  void OnKeyboardVisibleBoundsChanged(const gfx::Rect& bounds) override {}
  void OnKeyboardConfigChanged(KeyboardConfigPtr config) override {
    config_ = *config;
  }

  const KeyboardConfig& config() const { return config_; }
  void set_config(const KeyboardConfig& config) { config_ = config; }
  const std::vector<keyboard::mojom::KeyboardEnableFlag>& enable_flags() const {
    return enable_flags_;
  }
  int destroyed_count() const { return destroyed_count_; }

 private:
  std::vector<keyboard::mojom::KeyboardEnableFlag> enable_flags_;
  KeyboardConfig config_;
  int destroyed_count_ = 0;
  mojo::AssociatedBinding<mojom::KeyboardControllerObserver>
      keyboard_controller_observer_binding_{this};

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

class TestClient {
 public:
  explicit TestClient(service_manager::Connector* connector) {
    connector->BindInterface("test", &keyboard_controller_ptr_);

    test_observer_ =
        std::make_unique<TestObserver>(keyboard_controller_ptr_.get());
  }

  ~TestClient() = default;

  bool IsKeyboardEnabled() {
    keyboard_controller_ptr_->IsKeyboardEnabled(base::BindOnce(
        &TestClient::OnIsKeyboardEnabled, base::Unretained(this)));
    keyboard_controller_ptr_.FlushForTesting();
    return is_enabled_;
  }

  void GetKeyboardConfig() {
    keyboard_controller_ptr_->GetKeyboardConfig(base::BindOnce(
        &TestClient::OnGetKeyboardConfig, base::Unretained(this)));
    keyboard_controller_ptr_.FlushForTesting();
  }

  void SetKeyboardConfig(KeyboardConfigPtr config) {
    keyboard_controller_ptr_->SetKeyboardConfig(std::move(config));
    keyboard_controller_ptr_.FlushForTesting();
  }

  void SetEnableFlag(KeyboardEnableFlag flag) {
    keyboard_controller_ptr_->SetEnableFlag(flag);
    keyboard_controller_ptr_.FlushForTesting();
  }

  void ClearEnableFlag(KeyboardEnableFlag flag) {
    keyboard_controller_ptr_->ClearEnableFlag(flag);
    keyboard_controller_ptr_.FlushForTesting();
  }

  std::vector<keyboard::mojom::KeyboardEnableFlag> GetEnableFlags() {
    std::vector<keyboard::mojom::KeyboardEnableFlag> enable_flags;
    base::RunLoop run_loop;
    keyboard_controller_ptr_->GetEnableFlags(base::BindOnce(
        [](std::vector<keyboard::mojom::KeyboardEnableFlag>* enable_flags,
           base::OnceClosure callback,
           const std::vector<keyboard::mojom::KeyboardEnableFlag>& flags) {
          *enable_flags = flags;
          std::move(callback).Run();
        },
        &enable_flags, run_loop.QuitClosure()));
    run_loop.Run();
    return enable_flags;
  }

  void RebuildKeyboardIfEnabled() {
    keyboard_controller_ptr_->RebuildKeyboardIfEnabled();
    keyboard_controller_ptr_.FlushForTesting();
  }

  bool IsKeyboardVisible() {
    keyboard_controller_ptr_->IsKeyboardVisible(base::BindOnce(
        &TestClient::OnIsKeyboardVisible, base::Unretained(this)));
    keyboard_controller_ptr_.FlushForTesting();
    return is_visible_;
  }

  void ShowKeyboard() {
    keyboard_controller_ptr_->ShowKeyboard();
    keyboard_controller_ptr_.FlushForTesting();
  }

  void HideKeyboard() {
    keyboard_controller_ptr_->HideKeyboard(mojom::HideReason::kUser);
    keyboard_controller_ptr_.FlushForTesting();
  }

  bool SetContainerType(keyboard::mojom::ContainerType container_type,
                        const base::Optional<gfx::Rect>& target_bounds) {
    bool result;
    base::RunLoop run_loop;
    keyboard_controller_ptr_->SetContainerType(
        container_type, target_bounds,
        base::BindOnce(
            [](bool* result_ptr, base::OnceClosure callback, bool result) {
              *result_ptr = result;
              std::move(callback).Run();
            },
            &result, run_loop.QuitClosure()));
    run_loop.Run();
    return result;
  }

  void SetKeyboardLocked(bool locked) {
    keyboard_controller_ptr_->SetKeyboardLocked(locked);
    keyboard_controller_ptr_.FlushForTesting();
  }

  void SetOccludedBounds(const std::vector<gfx::Rect>& bounds) {
    keyboard_controller_ptr_->SetOccludedBounds(bounds);
    keyboard_controller_ptr_.FlushForTesting();
  }

  void SetHitTestBounds(const std::vector<gfx::Rect>& bounds) {
    keyboard_controller_ptr_->SetHitTestBounds(bounds);
    keyboard_controller_ptr_.FlushForTesting();
  }

  void SetDraggableArea(const gfx::Rect& bounds) {
    keyboard_controller_ptr_->SetDraggableArea(bounds);
    keyboard_controller_ptr_.FlushForTesting();
  }

  TestObserver* test_observer() const { return test_observer_.get(); }
  int got_keyboard_config_count() const { return got_keyboard_config_count_; }
  const KeyboardConfig& keyboard_config() const { return keyboard_config_; }

 private:
  void OnIsKeyboardEnabled(bool enabled) { is_enabled_ = enabled; }
  void OnIsKeyboardVisible(bool visible) { is_visible_ = visible; }

  void OnGetKeyboardConfig(KeyboardConfigPtr config) {
    ++got_keyboard_config_count_;
    keyboard_config_ = *config;
  }

  bool is_enabled_ = false;
  bool is_visible_ = false;
  int got_keyboard_config_count_ = 0;
  KeyboardConfig keyboard_config_;
  mojom::KeyboardControllerPtr keyboard_controller_ptr_;
  std::unique_ptr<TestObserver> test_observer_;
};

class TestContainerBehavior : public keyboard::ContainerBehavior {
 public:
  TestContainerBehavior() : keyboard::ContainerBehavior(nullptr){};
  ~TestContainerBehavior() override = default;

  // keyboard::ContainerBehavior
  void DoShowingAnimation(
      aura::Window* window,
      ui::ScopedLayerAnimationSettings* animation_settings) override {}

  void DoHidingAnimation(
      aura::Window* window,
      wm::ScopedHidingAnimationSettings* animation_settings) override {}

  void InitializeShowAnimationStartingState(aura::Window* container) override {}

  gfx::Rect AdjustSetBoundsRequest(
      const gfx::Rect& display_bounds,
      const gfx::Rect& requested_bounds_in_screen_coords) override {
    return gfx::Rect();
  }

  void SetCanonicalBounds(aura::Window* container,
                          const gfx::Rect& display_bounds) override {}

  bool IsOverscrollAllowed() const override { return true; }

  bool IsDragHandle(const gfx::Vector2d& offset,
                    const gfx::Size& keyboard_size) const override {
    return false;
  }

  void SavePosition(const gfx::Rect& keyboard_bounds_in_screen,
                    const gfx::Size& screen_size) override {}

  bool HandlePointerEvent(const ui::LocatedEvent& event,
                          const display::Display& current_display) override {
    return false;
  }

  keyboard::mojom::ContainerType GetType() const override { return type_; }

  bool TextBlurHidesKeyboard() const override { return false; }

  void SetOccludedBounds(const gfx::Rect& bounds) override {
    occluded_bounds_ = bounds;
  }

  gfx::Rect GetOccludedBounds(
      const gfx::Rect& visual_bounds_in_window) const override {
    return occluded_bounds_;
  }

  bool OccludedBoundsAffectWorkspaceLayout() const override { return false; }

  void SetDraggableArea(const gfx::Rect& rect) override {
    draggable_area_ = rect;
  }

  const gfx::Rect& occluded_bounds() const { return occluded_bounds_; }
  const gfx::Rect& draggable_area() const { return draggable_area_; }

 private:
  keyboard::mojom::ContainerType type_ =
      keyboard::mojom::ContainerType::kFullWidth;
  gfx::Rect occluded_bounds_;
  gfx::Rect draggable_area_;
};

class AshKeyboardControllerTest : public AshTestBase {
 public:
  AshKeyboardControllerTest() = default;
  ~AshKeyboardControllerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    // Create a local service manager connector to handle requests to
    // mojom::KeyboardController.
    service_manager::mojom::ConnectorRequest request;
    connector_ = service_manager::Connector::Create(&request);

    service_manager::Connector::TestApi test_api(connector_.get());
    test_api.OverrideBinderForTesting(
        service_manager::ServiceFilter::ByName("test"),
        mojom::KeyboardController::Name_,
        base::BindRepeating(
            &AshKeyboardControllerTest::AddKeyboardControllerBinding,
            base::Unretained(this)));
    base::RunLoop().RunUntilIdle();

    test_client_ = std::make_unique<TestClient>(connector_.get());

    // Set the initial observer config to the client (default) config.
    test_client_->test_observer()->set_config(test_client()->keyboard_config());
  }

  void TearDown() override {
    test_client_.reset();
    AshTestBase::TearDown();
  }

  void AddKeyboardControllerBinding(mojo::ScopedMessagePipeHandle handle) {
    Shell::Get()->ash_keyboard_controller()->BindRequest(
        mojom::KeyboardControllerRequest(std::move(handle)));
  }

  keyboard::KeyboardController* keyboard_controller() {
    return Shell::Get()->ash_keyboard_controller()->keyboard_controller();
  }
  TestClient* test_client() { return test_client_.get(); }

 private:
  std::unique_ptr<service_manager::Connector> connector_;
  std::unique_ptr<TestClient> test_client_;

  DISALLOW_COPY_AND_ASSIGN(AshKeyboardControllerTest);
};

}  // namespace

TEST_F(AshKeyboardControllerTest, GetKeyboardConfig) {
  test_client()->GetKeyboardConfig();
  EXPECT_EQ(1, test_client()->got_keyboard_config_count());
}

TEST_F(AshKeyboardControllerTest, SetKeyboardConfig) {
  // Enable the keyboard so that config changes trigger observer events.
  test_client()->SetEnableFlag(KeyboardEnableFlag::kExtensionEnabled);

  test_client()->GetKeyboardConfig();
  EXPECT_EQ(1, test_client()->got_keyboard_config_count());
  KeyboardConfigPtr config =
      KeyboardConfig::New(test_client()->keyboard_config());
  // Set the observer config to the client (default) config.
  test_client()->test_observer()->set_config(*config);

  // Change the keyboard config.
  bool old_auto_complete = config->auto_complete;
  config->auto_complete = !config->auto_complete;
  test_client()->SetKeyboardConfig(std::move(config));

  // Test that the config changes.
  test_client()->GetKeyboardConfig();
  EXPECT_NE(old_auto_complete, test_client()->keyboard_config().auto_complete);

  // Test that the test observer received the change.
  EXPECT_NE(old_auto_complete,
            test_client()->test_observer()->config().auto_complete);
}

TEST_F(AshKeyboardControllerTest, EnableFlags) {
  EXPECT_FALSE(test_client()->IsKeyboardEnabled());
  // Enable the keyboard.
  test_client()->SetEnableFlag(KeyboardEnableFlag::kExtensionEnabled);
  std::vector<keyboard::mojom::KeyboardEnableFlag> enable_flags =
      test_client()->GetEnableFlags();
  EXPECT_TRUE(
      base::ContainsValue(enable_flags, KeyboardEnableFlag::kExtensionEnabled));
  EXPECT_EQ(enable_flags, test_client()->test_observer()->enable_flags());
  EXPECT_TRUE(test_client()->IsKeyboardEnabled());

  // Set the enable override to disable the keyboard.
  test_client()->SetEnableFlag(KeyboardEnableFlag::kPolicyDisabled);
  enable_flags = test_client()->GetEnableFlags();
  EXPECT_TRUE(
      base::ContainsValue(enable_flags, KeyboardEnableFlag::kExtensionEnabled));
  EXPECT_TRUE(
      base::ContainsValue(enable_flags, KeyboardEnableFlag::kPolicyDisabled));
  EXPECT_EQ(enable_flags, test_client()->test_observer()->enable_flags());
  EXPECT_FALSE(test_client()->IsKeyboardEnabled());

  // Clear the enable override; should enable the keyboard.
  test_client()->ClearEnableFlag(KeyboardEnableFlag::kPolicyDisabled);
  enable_flags = test_client()->GetEnableFlags();
  EXPECT_TRUE(
      base::ContainsValue(enable_flags, KeyboardEnableFlag::kExtensionEnabled));
  EXPECT_FALSE(
      base::ContainsValue(enable_flags, KeyboardEnableFlag::kPolicyDisabled));
  EXPECT_EQ(enable_flags, test_client()->test_observer()->enable_flags());
  EXPECT_TRUE(test_client()->IsKeyboardEnabled());
}

TEST_F(AshKeyboardControllerTest, RebuildKeyboardIfEnabled) {
  EXPECT_EQ(0, test_client()->test_observer()->destroyed_count());

  // Enable the keyboard.
  test_client()->SetEnableFlag(KeyboardEnableFlag::kExtensionEnabled);
  EXPECT_EQ(0, test_client()->test_observer()->destroyed_count());

  // Enable the keyboard again; this should not reload the keyboard.
  test_client()->SetEnableFlag(KeyboardEnableFlag::kExtensionEnabled);
  EXPECT_EQ(0, test_client()->test_observer()->destroyed_count());

  // Rebuild the keyboard. This should destroy the previous keyboard window.
  test_client()->RebuildKeyboardIfEnabled();
  EXPECT_EQ(1, test_client()->test_observer()->destroyed_count());

  // Disable the keyboard. The keyboard window should be destroyed.
  test_client()->ClearEnableFlag(KeyboardEnableFlag::kExtensionEnabled);
  EXPECT_EQ(2, test_client()->test_observer()->destroyed_count());
}

TEST_F(AshKeyboardControllerTest, ShowAndHideKeyboard) {
  // Enable the keyboard. This will create the keyboard window but not show it.
  test_client()->SetEnableFlag(KeyboardEnableFlag::kExtensionEnabled);
  ASSERT_TRUE(keyboard_controller()->GetKeyboardWindow());
  EXPECT_FALSE(keyboard_controller()->GetKeyboardWindow()->IsVisible());

  test_client()->ShowKeyboard();
  EXPECT_TRUE(keyboard_controller()->GetKeyboardWindow()->IsVisible());

  test_client()->HideKeyboard();
  EXPECT_FALSE(keyboard_controller()->GetKeyboardWindow()->IsVisible());

  // TODO(stevenjb): Also use TestObserver and IsKeyboardVisible to test
  // visibility changes. https://crbug.com/849995.
}

TEST_F(AshKeyboardControllerTest, SetContainerType) {
  // Enable the keyboard.
  test_client()->SetEnableFlag(KeyboardEnableFlag::kExtensionEnabled);
  const auto default_behavior = keyboard::mojom::ContainerType::kFullWidth;
  EXPECT_EQ(default_behavior, keyboard_controller()->GetActiveContainerType());

  gfx::Rect target_bounds(0, 0, 10, 10);
  // Set the container type to kFloating.
  EXPECT_TRUE(test_client()->SetContainerType(
      keyboard::mojom::ContainerType::kFloating, target_bounds));
  EXPECT_EQ(keyboard::mojom::ContainerType::kFloating,
            keyboard_controller()->GetActiveContainerType());
  // Ensure that the window size is correct (position is determined by Ash).
  EXPECT_EQ(
      target_bounds.size(),
      keyboard_controller()->GetKeyboardWindow()->GetTargetBounds().size());

  // Set the container type to kFullscreen.
  EXPECT_TRUE(test_client()->SetContainerType(
      keyboard::mojom::ContainerType::kFullscreen, base::nullopt));
  EXPECT_EQ(keyboard::mojom::ContainerType::kFullscreen,
            keyboard_controller()->GetActiveContainerType());

  // Setting the container type to the current type should fail.
  EXPECT_FALSE(test_client()->SetContainerType(
      keyboard::mojom::ContainerType::kFullscreen, base::nullopt));
  EXPECT_EQ(keyboard::mojom::ContainerType::kFullscreen,
            keyboard_controller()->GetActiveContainerType());
}

TEST_F(AshKeyboardControllerTest, SetKeyboardLocked) {
  ASSERT_FALSE(keyboard_controller()->keyboard_locked());
  test_client()->SetKeyboardLocked(true);
  EXPECT_TRUE(keyboard_controller()->keyboard_locked());
  test_client()->SetKeyboardLocked(false);
  EXPECT_FALSE(keyboard_controller()->keyboard_locked());
}

TEST_F(AshKeyboardControllerTest, SetOccludedBounds) {
  // Enable the keyboard.
  test_client()->SetEnableFlag(KeyboardEnableFlag::kExtensionEnabled);

  // Override the container behavior.
  auto scoped_behavior = std::make_unique<TestContainerBehavior>();
  TestContainerBehavior* behavior = scoped_behavior.get();
  keyboard_controller()->set_container_behavior_for_test(
      std::move(scoped_behavior));

  gfx::Rect bounds(10, 20, 30, 40);
  test_client()->SetOccludedBounds({bounds});
  EXPECT_EQ(bounds, behavior->occluded_bounds());
}

TEST_F(AshKeyboardControllerTest, SetHitTestBounds) {
  // Enable the keyboard.
  test_client()->SetEnableFlag(KeyboardEnableFlag::kExtensionEnabled);
  ASSERT_FALSE(keyboard_controller()->GetKeyboardWindow()->targeter());

  // Setting the hit test bounds should set a WindowTargeter.
  test_client()->SetHitTestBounds({gfx::Rect(10, 20, 30, 40)});
  ASSERT_TRUE(keyboard_controller()->GetKeyboardWindow()->targeter());
}

TEST_F(AshKeyboardControllerTest, SetDraggableArea) {
  // Enable the keyboard.
  test_client()->SetEnableFlag(KeyboardEnableFlag::kExtensionEnabled);

  // Override the container behavior.
  auto scoped_behavior = std::make_unique<TestContainerBehavior>();
  TestContainerBehavior* behavior = scoped_behavior.get();
  keyboard_controller()->set_container_behavior_for_test(
      std::move(scoped_behavior));

  gfx::Rect bounds(10, 20, 30, 40);
  test_client()->SetDraggableArea(bounds);
  EXPECT_EQ(bounds, behavior->draggable_area());
}

}  // namespace ash
