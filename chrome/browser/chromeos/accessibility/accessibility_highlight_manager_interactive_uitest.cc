// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/accessibility_highlight_manager.h"

#include "ash/accessibility/accessibility_focus_ring_controller.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/chromeos_switches.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/focused_node_details.h"
#include "content/public/browser/notification_types.h"
#include "ui/aura/window.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/image/image.h"
#include "ui/snapshot/snapshot.h"

namespace chromeos {
namespace {

// Update the display configuration as given in |display_specs|.  See
// display::test::DisplayManagerTestApi::UpdateDisplay for more details.
void UpdateDisplay(const std::string& display_specs) {
  display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
      .UpdateDisplay(display_specs);
}

class MockTextInputClient : public ui::DummyTextInputClient {
 public:
  MockTextInputClient() : ui::DummyTextInputClient(ui::TEXT_INPUT_TYPE_TEXT) {}

  void SetCaretBounds(const gfx::Rect& bounds) { caret_bounds_ = bounds; }

 private:
  gfx::Rect GetCaretBounds() const override { return caret_bounds_; }

  gfx::Rect caret_bounds_;

  DISALLOW_COPY_AND_ASSIGN(MockTextInputClient);
};

class TestAccessibilityHighlightManager : public AccessibilityHighlightManager {
 public:
  TestAccessibilityHighlightManager() {}

  void OnCaretBoundsChanged(const ui::TextInputClient* client) override {
    AccessibilityHighlightManager::OnCaretBoundsChanged(client);
  }
  void OnMouseEvent(ui::MouseEvent* event) override {
    AccessibilityHighlightManager::OnMouseEvent(event);
  }
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    AccessibilityHighlightManager::Observe(type, source, details);
  }

 private:
  bool IsCursorVisible() override { return true; }

  DISALLOW_COPY_AND_ASSIGN(TestAccessibilityHighlightManager);
};

}  // namespace

class AccessibilityHighlightManagerTest : public InProcessBrowserTest {
 protected:
  AccessibilityHighlightManagerTest() {}
  ~AccessibilityHighlightManagerTest() override {}

  void SetUp() override {
    ash::AccessibilityFocusRingController::GetInstance()->SetNoFadeForTesting();
    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(::switches::kEnablePixelOutputInTests);
  }

  void CaptureBeforeImage(const gfx::Rect& bounds) {
    Capture(bounds);
    if (before_bmp_.tryAllocPixels(image_.AsBitmap().info())) {
      image_.AsBitmap().readPixels(before_bmp_.info(), before_bmp_.getPixels(),
                                   before_bmp_.rowBytes(), 0, 0);
    }
  }

  void CaptureAfterImage(const gfx::Rect& bounds) {
    Capture(bounds);
    if (after_bmp_.tryAllocPixels(image_.AsBitmap().info())) {
      image_.AsBitmap().readPixels(after_bmp_.info(), after_bmp_.getPixels(),
                                   after_bmp_.rowBytes(), 0, 0);
    }
  }

  void ComputeImageStats() {
    diff_count_ = 0;
    double accum[4] = {0, 0, 0, 0};
    for (int x = 0; x < before_bmp_.width(); ++x) {
      for (int y = 0; y < before_bmp_.height(); ++y) {
        SkColor before_color = before_bmp_.getColor(x, y);
        SkColor after_color = after_bmp_.getColor(x, y);
        if (before_color != after_color) {
          diff_count_++;
          accum[0] += SkColorGetB(after_color);
          accum[1] += SkColorGetG(after_color);
          accum[2] += SkColorGetR(after_color);
          accum[3] += SkColorGetA(after_color);
        }
      }
    }
    average_diff_color_ =
        SkColorSetARGB(static_cast<unsigned char>(accum[3] / diff_count_),
                       static_cast<unsigned char>(accum[2] / diff_count_),
                       static_cast<unsigned char>(accum[1] / diff_count_),
                       static_cast<unsigned char>(accum[0] / diff_count_));
  }

  int diff_count() { return diff_count_; }
  SkColor average_diff_color() { return average_diff_color_; }

 private:
  void GotSnapshot(const gfx::Image& image) {
    image_ = image;
    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                     run_loop_quitter_);
  }

  void Capture(const gfx::Rect& bounds) {
    // Occasionally we don't get any pixels the first try.
    // Keep trying until we get the correct size bitmap and
    // the first pixel is not transparent.
    while (true) {
      aura::Window* window = ash::Shell::GetPrimaryRootWindow();
      ui::GrabWindowSnapshotAndScaleAsync(
          window, bounds, bounds.size(),
          base::Bind(&AccessibilityHighlightManagerTest::GotSnapshot,
                     base::Unretained(this)));
      base::RunLoop run_loop;
      run_loop_quitter_ = run_loop.QuitClosure();
      run_loop.Run();
      SkBitmap bitmap = image_.AsBitmap();
      if (bitmap.width() != bounds.width() ||
          bitmap.height() != bounds.height()) {
        LOG(INFO) << "Bitmap not correct size, trying to capture again";
        continue;
      }
      if (255 == SkColorGetA(bitmap.getColor(0, 0))) {
        LOG(INFO) << "Bitmap is transparent, trying to capture again";
        break;
      }
    }
  }

  base::Closure run_loop_quitter_;
  gfx::Image image_;
  SkBitmap before_bmp_;
  SkBitmap after_bmp_;
  int diff_count_ = 0;
  SkColor average_diff_color_ = 0;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityHighlightManagerTest);
};

IN_PROC_BROWSER_TEST_F(AccessibilityHighlightManagerTest,
                       TestCaretRingDrawsBluePixels) {
  gfx::Rect capture_bounds(200, 300, 100, 100);
  gfx::Rect caret_bounds(230, 330, 1, 25);

  CaptureBeforeImage(capture_bounds);

  TestAccessibilityHighlightManager manager;
  manager.HighlightCaret(true);
  MockTextInputClient text_input_client;
  text_input_client.SetCaretBounds(caret_bounds);
  manager.OnCaretBoundsChanged(&text_input_client);

  CaptureAfterImage(capture_bounds);
  ComputeImageStats();

  // This is a smoke test to assert that something is drawn in the right
  // part of the screen of approximately the right size and color.
  // There's deliberately some tolerance for tiny errors.
  EXPECT_NEAR(1487, diff_count(), 50);
  EXPECT_NEAR(175, SkColorGetR(average_diff_color()), 5);
  EXPECT_NEAR(175, SkColorGetG(average_diff_color()), 5);
  EXPECT_NEAR(255, SkColorGetB(average_diff_color()), 5);
}

IN_PROC_BROWSER_TEST_F(AccessibilityHighlightManagerTest,
                       TestCursorRingDrawsRedPixels) {
  gfx::Rect capture_bounds(200, 300, 100, 100);
  gfx::Point cursor_point(250, 350);

  CaptureBeforeImage(capture_bounds);

  TestAccessibilityHighlightManager manager;
  manager.HighlightCursor(true);
  ui::MouseEvent mouse_move(ui::ET_MOUSE_MOVED, cursor_point, cursor_point,
                            ui::EventTimeForNow(), 0, 0);
  manager.OnMouseEvent(&mouse_move);
  CaptureAfterImage(capture_bounds);
  ComputeImageStats();

  // This is a smoke test to assert that something is drawn in the right
  // part of the screen of approximately the right size and color.
  // There's deliberately some tolerance for tiny errors.
  EXPECT_NEAR(1521, diff_count(), 50);
  EXPECT_NEAR(255, SkColorGetR(average_diff_color()), 5);
  EXPECT_NEAR(176, SkColorGetG(average_diff_color()), 5);
  EXPECT_NEAR(176, SkColorGetB(average_diff_color()), 5);
}

IN_PROC_BROWSER_TEST_F(AccessibilityHighlightManagerTest,
                       TestFocusRingDrawsOrangePixels) {
  gfx::Rect capture_bounds(200, 300, 100, 100);
  gfx::Rect focus_bounds(230, 330, 40, 40);

  CaptureBeforeImage(capture_bounds);

  TestAccessibilityHighlightManager manager;
  manager.HighlightFocus(true);
  content::FocusedNodeDetails details{false, focus_bounds};
  manager.Observe(content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE,
                  content::Source<AccessibilityHighlightManagerTest>(this),
                  content::Details<content::FocusedNodeDetails>(&details));
  CaptureAfterImage(capture_bounds);
  ComputeImageStats();

  // This is a smoke test to assert that something is drawn in the right
  // part of the screen of approximately the right size and color.
  // There's deliberately some tolerance for tiny errors.
  EXPECT_NEAR(1608, diff_count(), 50);
  EXPECT_NEAR(255, SkColorGetR(average_diff_color()), 5);
  EXPECT_NEAR(201, SkColorGetG(average_diff_color()), 5);
  EXPECT_NEAR(152, SkColorGetB(average_diff_color()), 5);
}

// Integration test of cursor handling between AccessibilityHighlightManager and
// AccessibilityFocusRingController.
IN_PROC_BROWSER_TEST_F(AccessibilityHighlightManagerTest,
                       CursorWorksOnMultipleDisplays) {
  UpdateDisplay("400x400,500x500");
  aura::Window::Windows root_windows = ash::Shell::Get()->GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());

  TestAccessibilityHighlightManager highlight_manager;
  highlight_manager.HighlightCursor(true);
  gfx::Point location(90, 90);
  ui::MouseEvent event0(ui::ET_MOUSE_MOVED, location, location,
                        ui::EventTimeForNow(), 0, 0);
  ui::Event::DispatcherApi event_mod(&event0);
  event_mod.set_target(root_windows[0]);
  highlight_manager.OnMouseEvent(&event0);

  ash::AccessibilityFocusRingController* controller =
      ash::AccessibilityFocusRingController::GetInstance();
  ash::AccessibilityCursorRingLayer* cursor_layer =
      controller->cursor_layer_for_testing();
  EXPECT_EQ(root_windows[0], cursor_layer->root_window());
  EXPECT_LT(abs(cursor_layer->layer()->GetTargetBounds().x() - location.x()),
            50);
  EXPECT_LT(abs(cursor_layer->layer()->GetTargetBounds().y() - location.y()),
            50);

  ui::MouseEvent event1(ui::ET_MOUSE_MOVED, location, location,
                        ui::EventTimeForNow(), 0, 0);
  ui::Event::DispatcherApi event_mod1(&event1);
  event_mod1.set_target(root_windows[1]);
  highlight_manager.OnMouseEvent(&event1);

  cursor_layer = controller->cursor_layer_for_testing();
  EXPECT_EQ(root_windows[1], cursor_layer->root_window());
  EXPECT_LT(abs(cursor_layer->layer()->GetTargetBounds().x() - location.x()),
            50);
  EXPECT_LT(abs(cursor_layer->layer()->GetTargetBounds().y() - location.y()),
            50);
}

// Integration test of caret handling between AccessibilityHighlightManager and
// AccessibilityFocusRingController.
IN_PROC_BROWSER_TEST_F(AccessibilityHighlightManagerTest,
                       CaretRingDrawnOnlyWithinBounds) {
  // Given caret bounds that are not within the active window, expect that
  // the caret ring highlight is not drawn.
  browser()->window()->SetBounds(gfx::Rect(5, 5, 300, 300));
  browser()->window()->Activate();

  TestAccessibilityHighlightManager highlight_manager;
  MockTextInputClient text_input_client;
  highlight_manager.HighlightCaret(true);
  gfx::Rect caret_bounds(10, 10, 40, 40);
  text_input_client.SetCaretBounds(caret_bounds);
  highlight_manager.OnCaretBoundsChanged(&text_input_client);

  ash::AccessibilityFocusRingController* controller =
      ash::AccessibilityFocusRingController::GetInstance();
  ash::AccessibilityCursorRingLayer* caret_layer =
      controller->caret_layer_for_testing();
  EXPECT_EQ(abs(caret_layer->layer()->GetTargetBounds().x() - caret_bounds.x()),
            20);
  EXPECT_EQ(abs(caret_layer->layer()->GetTargetBounds().y() - caret_bounds.y()),
            20);

  gfx::Rect not_visible_bounds(301, 301, 10, 10);
  text_input_client.SetCaretBounds(not_visible_bounds);
  highlight_manager.OnCaretBoundsChanged(&text_input_client);

  EXPECT_FALSE(controller->caret_layer_for_testing());
}

}  // namespace chromeos
