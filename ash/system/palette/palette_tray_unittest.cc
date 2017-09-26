// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/palette/palette_tray.h"

#include "ash/ash_switches.h"
#include "ash/highlighter/highlighter_controller.h"
#include "ash/highlighter/highlighter_controller_test_api.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/config.h"
#include "ash/public/cpp/voice_interaction_state.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/shell_test_api.h"
#include "ash/system/palette/palette_utils.h"
#include "ash/system/palette/test_palette_delegate.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test_shell_delegate.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/test/simple_test_tick_clock.h"
#include "chromeos/chromeos_switches.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/touchscreen_device.h"
#include "ui/events/event.h"
#include "ui/events/test/device_data_manager_test_api.h"
#include "ui/events/test/event_generator.h"

namespace ash {

class PaletteTrayTest : public AshTestBase {
 public:
  PaletteTrayTest() {}
  ~PaletteTrayTest() override {}

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kAshEnablePaletteOnAllDisplays);

    palette_utils::SetHasStylusInputForTesting();

    AshTestBase::SetUp();

    palette_tray_ =
        StatusAreaWidgetTestHelper::GetStatusAreaWidget()->palette_tray();
    test_api_ = base::MakeUnique<PaletteTray::TestApi>(palette_tray_);

    // Set the test palette delegate here, since this requires an instance of
    // shell to be available.
    ShellTestApi().SetPaletteDelegate(base::MakeUnique<TestPaletteDelegate>());
    // Initialize the palette tray again since this test requires information
    // from the palette delegate. (It was initialized without the delegate in
    // AshTestBase::SetUp()).
    palette_tray_->Initialize();
  }

  // Performs a tap on the palette tray button.
  void PerformTap() {
    ui::GestureEvent tap(0, 0, 0, base::TimeTicks(),
                         ui::GestureEventDetails(ui::ET_GESTURE_TAP));
    palette_tray_->PerformAction(tap);
  }

 protected:
  TestPaletteDelegate* test_palette_delegate() {
    return static_cast<TestPaletteDelegate*>(Shell::Get()->palette_delegate());
  }

  PrefService* pref_service() {
    return Shell::Get()->GetLocalStatePrefService();
  }

  PaletteTray* palette_tray_ = nullptr;  // not owned

  std::unique_ptr<PaletteTray::TestApi> test_api_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PaletteTrayTest);
};

// Verify the palette tray button exists and but is not visible initially.
TEST_F(PaletteTrayTest, PaletteTrayIsInvisible) {
  ASSERT_TRUE(palette_tray_);
  EXPECT_FALSE(palette_tray_->visible());
}

// Verify that if the has seen stylus pref is not set initially, the palette
// tray's touch event watcher should be active.
TEST_F(PaletteTrayTest, PaletteTrayStylusWatcherAlive) {
  ASSERT_FALSE(palette_tray_->visible());

  EXPECT_TRUE(test_api_->IsStylusWatcherActive());
}

// Verify if the has seen stylus pref is not set initially, the palette tray
// should become visible after seeing a stylus event.
TEST_F(PaletteTrayTest, PaletteTrayVisibleAfterStylusSeen) {
  ASSERT_FALSE(palette_tray_->visible());
  ASSERT_FALSE(pref_service()->GetBoolean(prefs::kHasSeenStylus));
  ASSERT_TRUE(test_api_->IsStylusWatcherActive());

  // Send a stylus event.
  GetEventGenerator().EnterPenPointerMode();
  GetEventGenerator().PressTouch();
  GetEventGenerator().ReleaseTouch();
  GetEventGenerator().ExitPenPointerMode();

  // Verify that the palette tray is now visible, the stylus event watcher is
  // inactive and that the has seen stylus pref is now set to true.
  EXPECT_TRUE(palette_tray_->visible());
  EXPECT_FALSE(test_api_->IsStylusWatcherActive());
}

// Verify if the has seen stylus pref is initially set, the palette tray is
// visible.
TEST_F(PaletteTrayTest, StylusSeenPrefInitiallySet) {
  ASSERT_FALSE(palette_tray_->visible());
  pref_service()->SetBoolean(prefs::kHasSeenStylus, true);

  EXPECT_TRUE(palette_tray_->visible());
  EXPECT_FALSE(test_api_->IsStylusWatcherActive());
}

// Verify taps on the palette tray button results in expected behaviour.
TEST_F(PaletteTrayTest, PaletteTrayWorkflow) {
  // Verify the palette tray button is not active, and the palette tray bubble
  // is not shown initially.
  EXPECT_FALSE(palette_tray_->is_active());
  EXPECT_FALSE(test_api_->GetTrayBubbleWrapper());

  // Verify that by tapping the palette tray button, the button will become
  // active and the palette tray bubble will be open.
  PerformTap();
  EXPECT_TRUE(palette_tray_->is_active());
  EXPECT_TRUE(test_api_->GetTrayBubbleWrapper());

  // Verify that activating a mode tool will close the palette tray bubble, but
  // leave the palette tray button active.
  test_api_->GetPaletteToolManager()->ActivateTool(
      PaletteToolId::LASER_POINTER);
  EXPECT_TRUE(test_api_->GetPaletteToolManager()->IsToolActive(
      PaletteToolId::LASER_POINTER));
  EXPECT_TRUE(palette_tray_->is_active());
  EXPECT_FALSE(test_api_->GetTrayBubbleWrapper());

  // Verify that tapping the palette tray while a tool is active will deactivate
  // the tool, and the palette tray button will not be active.
  PerformTap();
  EXPECT_FALSE(palette_tray_->is_active());
  EXPECT_FALSE(test_api_->GetPaletteToolManager()->IsToolActive(
      PaletteToolId::LASER_POINTER));

  // Verify that activating a action tool will close the palette tray bubble and
  // the palette tray button is will not be active.
  PerformTap();
  ASSERT_TRUE(test_api_->GetTrayBubbleWrapper());
  test_api_->GetPaletteToolManager()->ActivateTool(
      PaletteToolId::CAPTURE_SCREEN);
  EXPECT_FALSE(test_api_->GetPaletteToolManager()->IsToolActive(
      PaletteToolId::CAPTURE_SCREEN));
  // Wait for the tray bubble widget to close.
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(test_api_->GetTrayBubbleWrapper());
  EXPECT_FALSE(palette_tray_->is_active());
}

// Verify that the palette tray button and bubble are as expected when modes
// that can be deactivated without pressing the palette tray button (such as
// capture region) are deactivated.
TEST_F(PaletteTrayTest, ModeToolDeactivatedAutomatically) {
  // Open the palette tray with a tap.
  PerformTap();
  ASSERT_TRUE(palette_tray_->is_active());
  ASSERT_TRUE(test_api_->GetTrayBubbleWrapper());

  // Activate and deactivate the capture region tool.
  test_api_->GetPaletteToolManager()->ActivateTool(
      PaletteToolId::CAPTURE_REGION);
  ASSERT_TRUE(test_api_->GetPaletteToolManager()->IsToolActive(
      PaletteToolId::CAPTURE_REGION));
  test_api_->GetPaletteToolManager()->DeactivateTool(
      PaletteToolId::CAPTURE_REGION);

  // Verify the bubble is hidden and the button is inactive after deactivating
  // the capture region tool.
  EXPECT_FALSE(test_api_->GetTrayBubbleWrapper());
  EXPECT_FALSE(palette_tray_->is_active());
}

TEST_F(PaletteTrayTest, NoMetalayerToolViewCreated) {
  EXPECT_FALSE(
      test_api_->GetPaletteToolManager()->HasTool(PaletteToolId::METALAYER));
}

// Base class for tests that rely on voice interaction enabled.
class PaletteTrayTestWithVoiceInteraction : public PaletteTrayTest {
 public:
  PaletteTrayTestWithVoiceInteraction() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        chromeos::switches::kEnableVoiceInteraction);
  }
  ~PaletteTrayTestWithVoiceInteraction() override = default;

  // PaletteTrayTest:
  void SetUp() override {
    PaletteTrayTest::SetUp();

    // Instantiate EventGenerator now so that its constructor does not overwrite
    // the simulated clock that is being installed below.
    GetEventGenerator();

    simulated_clock_ = new base::SimpleTestTickClock();
    // Tests fail if event time is ever 0.
    simulated_clock_->Advance(base::TimeDelta::FromMilliseconds(10));
    // ui takes ownership of the tick clock.
    ui::SetEventTickClockForTesting(base::WrapUnique(simulated_clock_));

    highlighter_controller_ = base::MakeUnique<HighlighterController>();
    highlighter_test_api_ = base::MakeUnique<HighlighterControllerTestApi>(
        highlighter_controller_.get());
    test_palette_delegate()->set_highlighter_test_api(
        highlighter_test_api_.get());
  }

  void TearDown() override {
    // This needs to be called first to remove the event handler before the
    // shell instance gets torn down.
    test_palette_delegate()->set_highlighter_test_api(nullptr);
    highlighter_test_api_.reset();
    highlighter_controller_.reset();
    PaletteTrayTest::TearDown();
  }

 protected:
  bool metalayer_enabled() const {
    return test_api_->GetPaletteToolManager()->IsToolActive(
        PaletteToolId::METALAYER);
  }

  bool highlighter_showing() const {
    return highlighter_test_api_->IsShowingHighlighter();
  }

  void DragAndAssertMetalayer(const std::string& context,
                              const gfx::Point& origin,
                              int event_flags,
                              bool expected,
                              bool expected_on_press) {
    SCOPED_TRACE(context);

    ui::test::EventGenerator& generator = GetEventGenerator();
    gfx::Point pos = origin;
    generator.MoveTouch(pos);
    generator.set_flags(event_flags);
    generator.PressTouch();
    // If this gesture is supposed to enable the tool, it should have done it by
    // now.
    EXPECT_EQ(expected, metalayer_enabled());
    // Unlike the tool, the highlighter might become visible only after the
    // first move, hence a separate parameter to check against.
    EXPECT_EQ(expected_on_press, highlighter_showing());
    pos += gfx::Vector2d(1, 1);
    generator.MoveTouch(pos);
    // If this gesture is supposed to show the highlighter, it should have done
    // it by now.
    EXPECT_EQ(expected, highlighter_showing());
    EXPECT_EQ(expected, metalayer_enabled());
    generator.set_flags(ui::EF_NONE);
    pos += gfx::Vector2d(1, 1);
    generator.MoveTouch(pos);
    EXPECT_EQ(expected, highlighter_showing());
    EXPECT_EQ(expected, metalayer_enabled());
    generator.ReleaseTouch();
  }

  void WaitDragAndAssertMetalayer(const std::string& context,
                                  const gfx::Point& origin,
                                  int event_flags,
                                  bool expected,
                                  bool expected_on_press) {
    const int kStrokeGap = 1000;
    simulated_clock_->Advance(base::TimeDelta::FromMilliseconds(kStrokeGap));
    DragAndAssertMetalayer(context, origin, event_flags, expected,
                           expected_on_press);
  }

  void DestroyPointerView() { highlighter_test_api_->DestroyPointerView(); }

 private:
  std::unique_ptr<HighlighterController> highlighter_controller_;
  std::unique_ptr<HighlighterControllerTestApi> highlighter_test_api_;

  // Owned by |ui|.
  base::SimpleTestTickClock* simulated_clock_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(PaletteTrayTestWithVoiceInteraction);
};

TEST_F(PaletteTrayTestWithVoiceInteraction, MetalayerToolViewCreated) {
  EXPECT_TRUE(
      test_api_->GetPaletteToolManager()->HasTool(PaletteToolId::METALAYER));
}

TEST_F(PaletteTrayTestWithVoiceInteraction, MetalayerToolActivatesHighlighter) {
  Shell::Get()->NotifyVoiceInteractionStatusChanged(
      VoiceInteractionState::RUNNING);
  Shell::Get()->NotifyVoiceInteractionEnabled(true);
  Shell::Get()->NotifyVoiceInteractionContextEnabled(true);

  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.EnterPenPointerMode();

  const gfx::Point origin(1, 1);
  const gfx::Vector2d step(1, 1);
  EXPECT_FALSE(palette_utils::PaletteContainsPointInScreen(origin + step));
  EXPECT_FALSE(
      palette_utils::PaletteContainsPointInScreen(origin + step + step));

  // Press/drag does not activate the highlighter unless the palette tool is
  // activated.
  DragAndAssertMetalayer("tool disabled", origin, ui::EF_NONE,
                         false /* no metalayer */,
                         false /* no highlighter on press */);

  // Activate the palette tool, still no highlighter.
  test_api_->GetPaletteToolManager()->ActivateTool(PaletteToolId::METALAYER);
  EXPECT_FALSE(highlighter_showing());

  // Press/drag over a regular (non-palette) location. This should activate the
  // highlighter.
  DragAndAssertMetalayer("tool enabled", origin, ui::EF_NONE,
                         true /* enables metalayer */,
                         true /* highlighter shown on press */);

  SCOPED_TRACE("drag over palette");
  DestroyPointerView();
  // Press/drag over the palette button. This should not activate the
  // highlighter, but should disable the palette tool instead.
  gfx::Point palette_point = palette_tray_->GetBoundsInScreen().CenterPoint();
  EXPECT_TRUE(palette_utils::PaletteContainsPointInScreen(palette_point));
  generator.MoveTouch(palette_point);
  generator.PressTouch();
  EXPECT_FALSE(highlighter_showing());
  palette_point += gfx::Vector2d(1, 1);
  EXPECT_TRUE(palette_utils::PaletteContainsPointInScreen(palette_point));
  generator.MoveTouch(palette_point);
  EXPECT_FALSE(highlighter_showing());
  generator.ReleaseTouch();
  EXPECT_FALSE(metalayer_enabled());

  // Disabling metalayer support in the delegate should disable the palette
  // tool.
  test_api_->GetPaletteToolManager()->ActivateTool(PaletteToolId::METALAYER);
  Shell::Get()->NotifyVoiceInteractionContextEnabled(false);
  EXPECT_FALSE(metalayer_enabled());

  // With the metalayer disabled again, press/drag does not activate the
  // highlighter.
  DragAndAssertMetalayer("tool disabled again", origin, ui::EF_NONE,
                         false /* no metalayer */,
                         false /* no highlighter on press */);
}

TEST_F(PaletteTrayTestWithVoiceInteraction,
       StylusBarrelButtonActivatesHighlighter) {
  Shell::Get()->NotifyVoiceInteractionStatusChanged(
      VoiceInteractionState::NOT_READY);
  Shell::Get()->NotifyVoiceInteractionEnabled(false);
  Shell::Get()->NotifyVoiceInteractionContextEnabled(false);

  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.EnterPenPointerMode();

  const gfx::Point origin(1, 1);
  const gfx::Vector2d step(1, 1);

  EXPECT_FALSE(palette_utils::PaletteContainsPointInScreen(origin));
  EXPECT_FALSE(palette_utils::PaletteContainsPointInScreen(origin + step));
  EXPECT_FALSE(
      palette_utils::PaletteContainsPointInScreen(origin + step + step));

  // Press and drag while holding down the stylus button, no highlighter unless
  // the metalayer support is fully enabled and the the framework is ready.
  WaitDragAndAssertMetalayer("nothing enabled", origin,
                             ui::EF_LEFT_MOUSE_BUTTON, false /* no metalayer */,
                             false /* no highlighter on press */);

  // Enable one of the two user prefs, should not be sufficient.
  Shell::Get()->NotifyVoiceInteractionContextEnabled(true);
  WaitDragAndAssertMetalayer("one pref enabled", origin,
                             ui::EF_LEFT_MOUSE_BUTTON, false /* no metalayer */,
                             false /* no highlighter on press */);

  // Enable the other user pref, still not sufficient.
  Shell::Get()->NotifyVoiceInteractionEnabled(true);
  WaitDragAndAssertMetalayer("two prefs enabled", origin,
                             ui::EF_LEFT_MOUSE_BUTTON, false /* no metalayer */,
                             false /* no highlighter on press */);

  // Once the service is ready, the button should start working.
  Shell::Get()->NotifyVoiceInteractionStatusChanged(
      VoiceInteractionState::RUNNING);

  // Press and drag with no button, still no highlighter.
  WaitDragAndAssertMetalayer("all enabled, no button ", origin, ui::EF_NONE,
                             false /* no metalayer */,
                             false /* no highlighter on press */);

  // Press/drag with while holding down the stylus button, but over the palette
  // tray. This should activate neither the palette tool nor the highlighter.
  gfx::Point palette_point = palette_tray_->GetBoundsInScreen().CenterPoint();
  EXPECT_TRUE(palette_utils::PaletteContainsPointInScreen(palette_point));
  EXPECT_TRUE(
      palette_utils::PaletteContainsPointInScreen(palette_point + step));
  EXPECT_TRUE(
      palette_utils::PaletteContainsPointInScreen(palette_point + step + step));
  WaitDragAndAssertMetalayer("drag over palette", palette_point,
                             ui::EF_LEFT_MOUSE_BUTTON, false /* no metalayer */,
                             false /* no highlighter on press */);

  // Perform a regular stroke (no button), followed by a button-down stroke
  // without a pause. This should not trigger metalayer.
  DragAndAssertMetalayer("writing, no button", origin, ui::EF_NONE,
                         false /* no metalayer */,
                         false /* no highlighter on press */);
  DragAndAssertMetalayer("writing, with button ", origin,
                         ui::EF_LEFT_MOUSE_BUTTON, false /* no metalayer */,
                         false /* no highlighter on press */);

  // Wait, then press/drag while holding down the stylus button over a regular
  // location. This should activate the palette tool and the highlighter.
  WaitDragAndAssertMetalayer("with button", origin, ui::EF_LEFT_MOUSE_BUTTON,
                             true /* enables metalayer */,
                             false /* no highlighter on press */);

  // Repeat the previous step without a pause, make sure that the palette tool
  // is not toggled, and the highlighter is enabled immediately.
  DragAndAssertMetalayer("with button, again", origin, ui::EF_LEFT_MOUSE_BUTTON,
                         true /* enables metalayer */,
                         true /* highlighter shown on press */);

  // Same after a pause.
  WaitDragAndAssertMetalayer(
      "with button, after a pause", origin, ui::EF_LEFT_MOUSE_BUTTON,
      true /* enables metalayer */, true /* highlighter shown on press */);

  // The barrel button should not work on the lock screen.
  DestroyPointerView();
  GetSessionControllerClient()->RequestLockScreen();
  EXPECT_FALSE(test_api_->GetPaletteToolManager()->IsToolActive(
      PaletteToolId::METALAYER));
  WaitDragAndAssertMetalayer("screen locked", origin, ui::EF_LEFT_MOUSE_BUTTON,
                             false /* no metalayer */,
                             false /* no highlighter on press */);

  // Unlock the screen, the barrel button should work again.
  GetSessionControllerClient()->UnlockScreen();
  WaitDragAndAssertMetalayer(
      "screen unlocked", origin, ui::EF_LEFT_MOUSE_BUTTON,
      true /* enables metalayer */, false /* no highlighter on press */);

  // Disable the metalayer support.
  // This should deactivate both the palette tool and the highlighter.
  Shell::Get()->NotifyVoiceInteractionContextEnabled(false);
  EXPECT_FALSE(test_api_->GetPaletteToolManager()->IsToolActive(
      PaletteToolId::METALAYER));

  DestroyPointerView();
  EXPECT_FALSE(highlighter_showing());
  DragAndAssertMetalayer("disabled", origin, ui::EF_LEFT_MOUSE_BUTTON,
                         false /* no metalayer */,
                         false /* no highlighter on press */);
}

// Base class for tests that need to simulate an internal stylus.
class PaletteTrayTestWithInternalStylus : public PaletteTrayTest {
 public:
  PaletteTrayTestWithInternalStylus() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kHasInternalStylus);
  }
  ~PaletteTrayTestWithInternalStylus() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(PaletteTrayTestWithInternalStylus);
};

// Verify the palette tray button exists and is visible if the device has an
// internal stylus.
TEST_F(PaletteTrayTestWithInternalStylus, Visible) {
  ASSERT_TRUE(palette_tray_);
  EXPECT_TRUE(palette_tray_->visible());
}

// Verify that when entering or exiting the lock screen, the behavior of the
// palette tray button is as expected.
TEST_F(PaletteTrayTestWithInternalStylus, PaletteTrayOnLockScreenBehavior) {
  ASSERT_TRUE(palette_tray_->visible());

  PaletteToolManager* manager = test_api_->GetPaletteToolManager();
  manager->ActivateTool(PaletteToolId::LASER_POINTER);
  EXPECT_TRUE(manager->IsToolActive(PaletteToolId::LASER_POINTER));

  // Verify that when entering the lock screen, the palette tray button is
  // hidden, and the tool that was active is no longer active.
  GetSessionControllerClient()->RequestLockScreen();
  EXPECT_FALSE(manager->IsToolActive(PaletteToolId::LASER_POINTER));
  EXPECT_FALSE(palette_tray_->visible());

  // Verify that when logging back in the tray is visible, but the tool that was
  // active before locking the screen is still inactive.
  GetSessionControllerClient()->UnlockScreen();
  EXPECT_TRUE(palette_tray_->visible());
  EXPECT_FALSE(manager->IsToolActive(PaletteToolId::LASER_POINTER));
}

// Verify a tool deactivates when the palette bubble is opened while the tool
// is active.
TEST_F(PaletteTrayTestWithInternalStylus, ToolDeactivatesWhenOpeningBubble) {
  ASSERT_TRUE(palette_tray_->visible());

  palette_tray_->ShowBubble();
  EXPECT_TRUE(test_api_->GetTrayBubbleWrapper());
  PaletteToolManager* manager = test_api_->GetPaletteToolManager();
  manager->ActivateTool(PaletteToolId::LASER_POINTER);
  EXPECT_TRUE(manager->IsToolActive(PaletteToolId::LASER_POINTER));
  EXPECT_FALSE(test_api_->GetTrayBubbleWrapper());

  palette_tray_->ShowBubble();
  EXPECT_TRUE(test_api_->GetTrayBubbleWrapper());
  EXPECT_FALSE(manager->IsToolActive(PaletteToolId::LASER_POINTER));
}

}  // namespace ash
