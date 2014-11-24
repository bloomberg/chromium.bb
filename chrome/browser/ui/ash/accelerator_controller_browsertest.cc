// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_controller.h"

#include "ash/shell.h"
#include "ash/wm/window_state.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "ui/events/test/event_generator.h"

#if defined(USE_X11)
#include "ui/events/test/events_test_utils_x11.h"
#endif

#if defined(OS_CHROMEOS) && defined(USE_X11)
typedef InProcessBrowserTest AcceleratorControllerBrowserTest;

// Test that pressing and holding Alt+ toggles the maximized state exactly once.
// This test is a browser test to test that the EF_IS_REPEAT flag is correctly
// passed down to AcceleratorController (via a conversion to WebInputEvent).
// See crbug.com/434743
IN_PROC_BROWSER_TEST_F(AcceleratorControllerBrowserTest,
                       RepeatedToggleMaximizeViaAccelerator) {
  ASSERT_TRUE(ash::Shell::HasInstance()) << "No Instance";
  AddTabAtIndex(0,
                GURL("data:text/html;base64,<html></html>"),
                ui::PAGE_TRANSITION_TYPED);
  browser()->window()->Show();

  ui::Accelerator accelerator(ui::VKEY_OEM_PLUS, ui::EF_ALT_DOWN);
  ash::AcceleratorController* accelerator_controller =
      ash::Shell::GetInstance()->accelerator_controller();
  ASSERT_TRUE(accelerator_controller->IsRegistered(accelerator));

  ash::wm::WindowState* window_state = ash::wm::GetActiveWindowState();
  EXPECT_FALSE(window_state->IsMaximized());

  // Construct the key events from XEvents because RenderWidgetHostViewAura
  // drops the ui::EF_IS_REPEAT flag otherwise.
  ui::ScopedXI2Event xi2_down1;
  xi2_down1.InitKeyEvent(ui::ET_KEY_PRESSED,
                         ui::VKEY_OEM_PLUS,
                         ui::EF_ALT_DOWN);
  ui::KeyEvent down1(xi2_down1);
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());
  generator.Dispatch(&down1);
  EXPECT_TRUE(window_state->IsMaximized());

  ui::ScopedXI2Event xi2_down2;
  xi2_down2.InitKeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_OEM_PLUS,
                         ui::EF_ALT_DOWN);
  ui::KeyEvent down2(xi2_down2);
  generator.Dispatch(&down2);

  ui::ScopedXI2Event xi2_up;
  xi2_up.InitKeyEvent(ui::ET_KEY_RELEASED, ui::VKEY_OEM_PLUS, ui::EF_ALT_DOWN);
  ui::KeyEvent up(xi2_up);
  generator.Dispatch(&up);
  EXPECT_TRUE(window_state->IsMaximized());
}
#endif
