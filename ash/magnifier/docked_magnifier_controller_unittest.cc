// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/magnifier/docked_magnifier_controller.h"

#include <memory>
#include <vector>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/display/display_util.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/host/ash_window_tree_host.h"
#include "ash/magnifier/magnifier_test_utils.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/config.h"
#include "ash/public/interfaces/docked_magnifier_controller.mojom.h"
#include "ash/session/session_controller.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/session_manager_types.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/display/display.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/widget/widget.h"

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
    scoped_feature_list_.InitAndEnableFeature(features::kDockedMagnifier);

    // Explicitly enable --ash-constrain-pointer-to-root to be able to test
    // mouse cursor confinement outside the magnifier viewport.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kAshConstrainPointerToRoot);

    NoSessionAshTestBase::SetUp();

    // Create user 1 session and simulate its login.
    SimulateUserLogin(kUser1Email);

    // Create user 2 session.
    GetSessionControllerClient()->AddUserSession(kUser2Email);

    test_client_.Start();

    // Place the cursor in the first display.
    GetEventGenerator().MoveMouseTo(gfx::Point(0, 0));
  }

  void SwitchActiveUser(const std::string& email) {
    GetSessionControllerClient()->SwitchActiveUser(
        AccountId::FromUserEmail(email));
  }

 protected:
  MagnifierTextInputTestHelper text_input_helper_;

 private:
  DockedMagnifierTestClient test_client_;

  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(DockedMagnifierTest);
};

// Tests that the Fullscreen and Docked Magnifiers are mutually exclusive.
// TODO(afakhry): Update this test to use ash::MagnificationController once
// refactored. https://crbug.com/817157.
TEST_F(DockedMagnifierTest, MutuallyExclusiveMagnifiers) {
  // Start with both magnifiers disabled.
  EXPECT_FALSE(controller()->GetEnabled());
  EXPECT_FALSE(controller()->GetFullscreenMagnifierEnabled());

  // Enabling one disables the other.
  controller()->SetEnabled(true);
  EXPECT_TRUE(controller()->GetEnabled());
  EXPECT_FALSE(controller()->GetFullscreenMagnifierEnabled());

  controller()->SetFullscreenMagnifierEnabled(true);
  EXPECT_FALSE(controller()->GetEnabled());
  EXPECT_TRUE(controller()->GetFullscreenMagnifierEnabled());

  controller()->SetEnabled(true);
  EXPECT_TRUE(controller()->GetEnabled());
  EXPECT_FALSE(controller()->GetFullscreenMagnifierEnabled());

  controller()->SetEnabled(false);
  EXPECT_FALSE(controller()->GetEnabled());
  EXPECT_FALSE(controller()->GetFullscreenMagnifierEnabled());
}

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

// Tests that the workareas of displays are adjusted properly when the Docked
// Magnifier's viewport moves from one display to the next.
TEST_F(DockedMagnifierTest, DisplaysWorkAreas) {
  UpdateDisplay("800x600,800+0-400x300");
  const auto root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());

  // Place the cursor in the first display.
  GetEventGenerator().MoveMouseTo(gfx::Point(0, 0));

  // Before the magnifier is enabled, the work areas of both displays are their
  // full size minus the shelf height.
  const display::Display& display_1 = display_manager()->GetDisplayAt(0);
  const gfx::Rect disp_1_bounds(0, 0, 800, 600);
  EXPECT_EQ(disp_1_bounds, display_1.bounds());
  gfx::Rect disp_1_workarea_no_magnifier = disp_1_bounds;
  disp_1_workarea_no_magnifier.Inset(0, 0, 0, kShelfSize);
  EXPECT_EQ(disp_1_workarea_no_magnifier, display_1.work_area());
  // At this point, normal mouse cursor confinement should be used.
  AshWindowTreeHost* host1 =
      Shell::Get()
          ->window_tree_host_manager()
          ->GetAshWindowTreeHostForDisplayId(display_1.id());
  EXPECT_EQ(host1->GetLastCursorConfineBoundsInPixels(),
            gfx::Rect(gfx::Point(0, 0), disp_1_bounds.size()));

  const display::Display& display_2 = display_manager()->GetDisplayAt(1);
  const gfx::Rect disp_2_bounds(800, 0, 400, 300);
  EXPECT_EQ(disp_2_bounds, display_2.bounds());
  gfx::Rect disp_2_workarea_no_magnifier = disp_2_bounds;
  disp_2_workarea_no_magnifier.Inset(0, 0, 0, kShelfSize);
  EXPECT_EQ(disp_2_workarea_no_magnifier, display_2.work_area());
  AshWindowTreeHost* host2 =
      Shell::Get()
          ->window_tree_host_manager()
          ->GetAshWindowTreeHostForDisplayId(display_2.id());
  EXPECT_EQ(host2->GetLastCursorConfineBoundsInPixels(),
            gfx::Rect(gfx::Point(0, 0), disp_2_bounds.size()));

  // Enable the magnifier and the check the workareas.
  controller()->SetEnabled(true);
  EXPECT_TRUE(controller()->GetEnabled());
  const views::Widget* viewport_1_widget =
      controller()->GetViewportWidgetForTesting();
  ASSERT_NE(nullptr, viewport_1_widget);
  EXPECT_EQ(root_windows[0],
            viewport_1_widget->GetNativeView()->GetRootWindow());
  // Since the cursor is in the first display, the height of its workarea will
  // be further shrunk from the top by 1/4th of its full height + the height of
  // the separator layer.
  gfx::Rect disp_1_workspace_with_magnifier = disp_1_workarea_no_magnifier;
  const int disp_1_magnifier_height =
      (disp_1_bounds.height() /
       DockedMagnifierController::kScreenHeightDevisor) +
      DockedMagnifierController::kSeparatorHeight;
  disp_1_workspace_with_magnifier.Inset(0, disp_1_magnifier_height, 0, 0);
  EXPECT_EQ(disp_1_bounds, display_1.bounds());
  EXPECT_EQ(disp_1_workspace_with_magnifier, display_1.work_area());
  // The first display should confine the mouse movement outside of the
  // viewport.
  const gfx::Rect disp_1_confine_bounds(
      0, disp_1_magnifier_height, disp_1_bounds.width(),
      disp_1_bounds.height() - disp_1_magnifier_height);
  EXPECT_EQ(host1->GetLastCursorConfineBoundsInPixels(), disp_1_confine_bounds);

  // The second display should remain unaffected.
  EXPECT_EQ(disp_2_bounds, display_2.bounds());
  EXPECT_EQ(disp_2_workarea_no_magnifier, display_2.work_area());
  EXPECT_EQ(host2->GetLastCursorConfineBoundsInPixels(),
            gfx::Rect(gfx::Point(0, 0), disp_2_bounds.size()));

  // Now, move mouse cursor to display 2, and expect that the workarea of
  // display 1 is restored to its original value, while that of display 2 is
  // shrunk to fit the Docked Magnifier's viewport.
  GetEventGenerator().MoveMouseTo(gfx::Point(800, 0));
  const views::Widget* viewport_2_widget =
      controller()->GetViewportWidgetForTesting();
  ASSERT_NE(nullptr, viewport_2_widget);
  EXPECT_NE(viewport_1_widget, viewport_2_widget);  // It's a different widget.
  EXPECT_EQ(root_windows[1],
            viewport_2_widget->GetNativeView()->GetRootWindow());
  EXPECT_EQ(disp_1_bounds, display_1.bounds());
  EXPECT_EQ(disp_1_workarea_no_magnifier, display_1.work_area());
  // Display 1 goes back to the normal mouse confinement.
  EXPECT_EQ(host1->GetLastCursorConfineBoundsInPixels(),
            gfx::Rect(gfx::Point(0, 0), disp_1_bounds.size()));
  EXPECT_EQ(disp_2_bounds, display_2.bounds());
  gfx::Rect disp_2_workspace_with_magnifier = disp_2_workarea_no_magnifier;
  const int disp_2_magnifier_height =
      (disp_2_bounds.height() /
       DockedMagnifierController::kScreenHeightDevisor) +
      DockedMagnifierController::kSeparatorHeight;
  disp_2_workspace_with_magnifier.Inset(0, disp_2_magnifier_height, 0, 0);
  EXPECT_EQ(disp_2_workspace_with_magnifier, display_2.work_area());
  // Display 2's mouse is confined outside the viewport.
  const gfx::Rect disp_2_confine_bounds(
      0, disp_2_magnifier_height, disp_2_bounds.width(),
      disp_2_bounds.height() - disp_2_magnifier_height);
  EXPECT_EQ(host2->GetLastCursorConfineBoundsInPixels(), disp_2_confine_bounds);

  // Now, disable the magnifier, and expect both displays to return back to
  // their original state.
  controller()->SetEnabled(false);
  EXPECT_FALSE(controller()->GetEnabled());
  EXPECT_EQ(disp_1_bounds, display_1.bounds());
  EXPECT_EQ(disp_1_workarea_no_magnifier, display_1.work_area());
  EXPECT_EQ(disp_2_bounds, display_2.bounds());
  EXPECT_EQ(disp_2_workarea_no_magnifier, display_2.work_area());
  // Normal mouse confinement for both displays.
  EXPECT_EQ(host1->GetLastCursorConfineBoundsInPixels(),
            gfx::Rect(gfx::Point(0, 0), disp_1_bounds.size()));
  EXPECT_EQ(host2->GetLastCursorConfineBoundsInPixels(),
            gfx::Rect(gfx::Point(0, 0), disp_2_bounds.size()));
}

// Tests the behavior of the magnifier when displays are added or removed.
TEST_F(DockedMagnifierTest, AddRemoveDisplays) {
  // Start with a single display.
  const auto disp_1_info = display::ManagedDisplayInfo::CreateFromSpecWithID(
      "0+0-600x800", 101 /* id */);
  std::vector<display::ManagedDisplayInfo> info_list;
  info_list.push_back(disp_1_info);
  display_manager()->OnNativeDisplaysChanged(info_list);
  auto root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(1u, root_windows.size());

  // Enable the magnifier, and validate the state of the viewport widget.
  controller()->SetEnabled(true);
  EXPECT_TRUE(controller()->GetEnabled());
  const views::Widget* viewport_widget =
      controller()->GetViewportWidgetForTesting();
  ASSERT_NE(nullptr, viewport_widget);
  EXPECT_EQ(root_windows[0], viewport_widget->GetNativeView()->GetRootWindow());
  const int viewport_1_height =
      800 / DockedMagnifierController::kScreenHeightDevisor;
  EXPECT_EQ(gfx::Rect(0, 0, 600, viewport_1_height),
            viewport_widget->GetWindowBoundsInScreen());

  // Adding a new display should not affect where the viewport currently is.
  const auto disp_2_info = display::ManagedDisplayInfo::CreateFromSpecWithID(
      "600+0-400x600", 102 /* id */);
  info_list.push_back(disp_2_info);
  display_manager()->OnNativeDisplaysChanged(info_list);
  root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());
  // Same viewport widget in same root window.
  EXPECT_EQ(viewport_widget, controller()->GetViewportWidgetForTesting());
  EXPECT_EQ(root_windows[0], viewport_widget->GetNativeView()->GetRootWindow());
  EXPECT_EQ(gfx::Rect(0, 0, 600, viewport_1_height),
            viewport_widget->GetWindowBoundsInScreen());

  // Move the cursor to the second display, expect the viewport widget to get
  // updated accordingly.
  GetEventGenerator().MoveMouseTo(gfx::Point(800, 0));
  // New viewport widget is created.
  ASSERT_NE(viewport_widget, controller()->GetViewportWidgetForTesting());
  viewport_widget = controller()->GetViewportWidgetForTesting();
  EXPECT_EQ(root_windows[1], viewport_widget->GetNativeView()->GetRootWindow());
  const int viewport_2_height =
      600 / DockedMagnifierController::kScreenHeightDevisor;
  EXPECT_EQ(gfx::Rect(600, 0, 400, viewport_2_height),
            viewport_widget->GetWindowBoundsInScreen());

  // Now, remove display 2 ** while ** the magnifier viewport is there. This
  // should cause no crashes, the viewport widget should be recreated in
  // display 1.
  info_list.clear();
  info_list.push_back(disp_1_info);
  display_manager()->OnNativeDisplaysChanged(info_list);
  // We need to spin this run loop to wait for a new mouse event to be
  // dispatched so that the viewport widget is re-created.
  base::RunLoop().RunUntilIdle();
  root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(1u, root_windows.size());
  if (Shell::GetAshConfig() == Config::MUS) {
    // TODO: Figure out why do we need to manually simulate the mouse cursor
    // being teleported to the first display. https://crbug.com/814590.
    GetEventGenerator().MoveMouseTo(gfx::Point(300, 400));
  }
  viewport_widget = controller()->GetViewportWidgetForTesting();
  ASSERT_NE(nullptr, viewport_widget);
  EXPECT_EQ(root_windows[0], viewport_widget->GetNativeView()->GetRootWindow());
  EXPECT_EQ(gfx::Rect(0, 0, 600, viewport_1_height),
            viewport_widget->GetWindowBoundsInScreen());
}

// Tests various magnifier layer transform in the simple cases (i.e. no device
// scale factors or screen rotations).
TEST_F(DockedMagnifierTest, TransformSimple) {
  UpdateDisplay("800x800");
  const auto root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(1u, root_windows.size());

  controller()->SetEnabled(true);
  const float scale1 = 2.0f;
  controller()->SetScale(scale1);
  EXPECT_TRUE(controller()->GetEnabled());
  EXPECT_FLOAT_EQ(scale1, controller()->GetScale());
  const views::Widget* viewport_widget =
      controller()->GetViewportWidgetForTesting();
  ASSERT_NE(nullptr, viewport_widget);
  EXPECT_EQ(root_windows[0], viewport_widget->GetNativeView()->GetRootWindow());
  const int viewport_height =
      800 / DockedMagnifierController::kScreenHeightDevisor;
  EXPECT_EQ(gfx::Rect(0, 0, 800, viewport_height),
            viewport_widget->GetWindowBoundsInScreen());

  // Move the cursor to the center of the screen.
  gfx::Point point_of_interest(400, 400);
  GetEventGenerator().MoveMouseTo(point_of_interest);
  const ui::Layer* magnifier_layer =
      controller()->GetViewportMagnifierLayerForTesting();
  // The magnifier layer's transform, when applied to the point of interest,
  // should take it to the point at the center of the viewport widget.
  magnifier_layer->transform().TransformPoint(&point_of_interest);
  const gfx::Point viewport_center(
      viewport_widget->GetWindowBoundsInScreen().CenterPoint());
  EXPECT_EQ(viewport_center, point_of_interest);

  // Move the cursor to the bottom right corner.
  point_of_interest = gfx::Point(799, 799);
  GetEventGenerator().MoveMouseTo(point_of_interest);
  // Similar to the above case the expected transform should still take the
  // point to the center of the viewport widget.
  magnifier_layer->transform().TransformPoint(&point_of_interest);
  EXPECT_EQ(viewport_center, point_of_interest);

  // Tricky: Move the cursor to the top right corner, such that the cursor is
  // over the magnifier viewport. The transform should be such that the viewport
  // doesn't show itself.
  point_of_interest = gfx::Point(799, 0);
  GetEventGenerator().MoveMouseTo(point_of_interest);
  // In this case, our point of interest is changed to be at the bottom of the
  // separator, and it should go to the center of the top *edge* of the viewport
  // widget.
  point_of_interest.set_y(viewport_height +
                          DockedMagnifierController::kSeparatorHeight);
  gfx::Point viewport_top_edge_center = viewport_center;
  viewport_top_edge_center.set_y(0);
  magnifier_layer->transform().TransformPoint(&point_of_interest);
  EXPECT_EQ(viewport_top_edge_center, point_of_interest);
  // The minimum height for the point of interest is the bottom of the viewport
  // + the height of the separator + half the height of the viewport when scaled
  // back to the non-magnified space.
  EXPECT_FLOAT_EQ(viewport_height +
                      DockedMagnifierController::kSeparatorHeight +
                      (viewport_center.y() / scale1),
                  controller()->GetMinimumPointOfInterestHeightForTesting());

  // Leave the mouse cursor where it is, and only change the magnifier's scale.
  const float scale2 = 5.3f;
  controller()->SetScale(scale2);
  EXPECT_FLOAT_EQ(scale2, controller()->GetScale());
  // The transform behaves exactly as above even with a different scale.
  point_of_interest = gfx::Point(799, 0);
  point_of_interest.set_y(viewport_height +
                          DockedMagnifierController::kSeparatorHeight);
  magnifier_layer->transform().TransformPoint(&point_of_interest);
  EXPECT_EQ(viewport_top_edge_center, point_of_interest);

  EXPECT_FLOAT_EQ(viewport_height +
                      DockedMagnifierController::kSeparatorHeight +
                      (viewport_center.y() / scale2),
                  controller()->GetMinimumPointOfInterestHeightForTesting());
}

// Tests that the magnifier viewport follows text fields focus and input caret
// bounds changes events.
TEST_F(DockedMagnifierTest, TextInputFieldEvents) {
  UpdateDisplay("600x900");
  const auto root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(1u, root_windows.size());

  text_input_helper_.CreateAndShowTextInputView(gfx::Rect(500, 400, 80, 80));

  // Enable the docked magnifier.
  controller()->SetEnabled(true);
  const float scale1 = 2.0f;
  controller()->SetScale(scale1);
  EXPECT_TRUE(controller()->GetEnabled());
  EXPECT_FLOAT_EQ(scale1, controller()->GetScale());

  // Focus on the text input field.
  text_input_helper_.FocusOnTextInputView();

  // The text input caret center point will be our point of interest. When it
  // goes through the magnifier layer transform, it should end up being in the
  // center of the viewport.
  const ui::Layer* magnifier_layer =
      controller()->GetViewportMagnifierLayerForTesting();
  gfx::Point caret_center(text_input_helper_.GetCaretBounds().CenterPoint());
  magnifier_layer->transform().TransformPoint(&caret_center);
  const views::Widget* viewport_widget =
      controller()->GetViewportWidgetForTesting();
  const gfx::Point viewport_center(
      viewport_widget->GetWindowBoundsInScreen().CenterPoint());
  EXPECT_EQ(caret_center, viewport_center);

  // Simulate typing by pressing some keys while focus is in the text field. The
  // transformed caret center should always go to the viewport center.
  GetEventGenerator().PressKey(ui::VKEY_A, 0);
  GetEventGenerator().ReleaseKey(ui::VKEY_A, 0);
  gfx::Point new_caret_center(
      text_input_helper_.GetCaretBounds().CenterPoint());
  magnifier_layer->transform().TransformPoint(&new_caret_center);
  EXPECT_EQ(new_caret_center, viewport_center);
}

// Tests that viewport layer is inverted properly when the status of the High
// Contrast mode changes.
TEST_F(DockedMagnifierTest, HighContrastMode) {
  UpdateDisplay("600x900");

  // Enable the docked magnifier.
  DockedMagnifierController* magnifier = controller();
  magnifier->SetEnabled(true);
  EXPECT_TRUE(magnifier->GetEnabled());

  // Expect that the magnifier layer is not inverted.
  const ui::Layer* viewport_layer =
      magnifier->GetViewportMagnifierLayerForTesting();
  ASSERT_TRUE(viewport_layer);
  EXPECT_FALSE(viewport_layer->layer_inverted());

  // Enable High Contrast mode, and expect the viewport layer to be inverted.
  Shell::Get()->accessibility_controller()->SetHighContrastEnabled(true);
  EXPECT_TRUE(
      Shell::Get()->accessibility_controller()->IsHighContrastEnabled());
  EXPECT_TRUE(viewport_layer->layer_inverted());

  // Disable High Contrast, the layer should be updated accordingly.
  Shell::Get()->accessibility_controller()->SetHighContrastEnabled(false);
  EXPECT_FALSE(
      Shell::Get()->accessibility_controller()->IsHighContrastEnabled());
  EXPECT_FALSE(viewport_layer->layer_inverted());

  // Now, disable the Docked Magnifier, enable High Contrast, and then re-enable
  // the Docked Magnifier. The newly created viewport layer should be inverted.
  magnifier->SetEnabled(false);
  EXPECT_FALSE(magnifier->GetEnabled());
  Shell::Get()->accessibility_controller()->SetHighContrastEnabled(true);
  EXPECT_TRUE(
      Shell::Get()->accessibility_controller()->IsHighContrastEnabled());
  magnifier->SetEnabled(true);
  EXPECT_TRUE(magnifier->GetEnabled());
  const ui::Layer* new_viewport_layer =
      magnifier->GetViewportMagnifierLayerForTesting();
  ASSERT_TRUE(new_viewport_layer);
  EXPECT_NE(new_viewport_layer, viewport_layer);
  EXPECT_TRUE(new_viewport_layer->layer_inverted());
}

// TODO(afakhry): Expand tests:
// - Test magnifier viewport's layer transforms with screen rotation,
//   multi display, and unified mode.
// - Test adjust scale using scroll events.

}  // namespace

}  // namespace ash
