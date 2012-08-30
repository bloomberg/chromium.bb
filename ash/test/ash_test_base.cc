// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_test_base.h"

#include <string>
#include <vector>

#include "ash/display/display_controller.h"
#include "ash/shell.h"
#include "ash/test/test_shell_delegate.h"
#include "base/run_loop.h"
#include "base/string_split.h"
#include "content/public/test/web_contents_tester.h"
#include "ui/aura/env.h"
#include "ui/aura/display_manager.h"
#include "ui/aura/root_window.h"
#include "ui/base/ime/text_input_test_support.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"

namespace ash {
namespace test {
namespace {

std::vector<gfx::Display> CreateDisplaysFromString(
    const std::string specs) {
  std::vector<gfx::Display> displays;
  std::vector<std::string> parts;
  base::SplitString(specs, ',', &parts);
  for (std::vector<std::string>::const_iterator iter = parts.begin();
       iter != parts.end(); ++iter) {
    displays.push_back(aura::DisplayManager::CreateDisplayFromSpec(*iter));
  }
  return displays;
}

}  // namespace

content::WebContents* AshTestViewsDelegate::CreateWebContents(
    content::BrowserContext* browser_context,
    content::SiteInstance* site_instance) {
  return content::WebContentsTester::CreateTestWebContents(browser_context,
                                                           site_instance);
}

AshTestBase::AshTestBase() {
}

AshTestBase::~AshTestBase() {
}

void AshTestBase::SetUp() {
  ui::TextInputTestSupport::Initilaize();
  // Creates Shell and hook with Desktop.
  TestShellDelegate* delegate = new TestShellDelegate;
  ash::Shell::CreateInstance(delegate);
  Shell::GetPrimaryRootWindow()->Show();
  Shell::GetPrimaryRootWindow()->SetHostSize(gfx::Size(800, 600));
  Shell::GetInstance()->cursor_manager()->ShowCursor(true);

  // Disable animations during tests.
  ui::LayerAnimator::set_disable_animations_for_test(true);
}

void AshTestBase::TearDown() {
  // Flush the message loop to finish pending release tasks.
  RunAllPendingInMessageLoop();

  // Tear down the shell.
  Shell::DeleteInstance();
  aura::Env::DeleteInstance();
  ui::TextInputTestSupport::Shutdown();
}

void AshTestBase::ChangeDisplayConfig(float scale,
                                      const gfx::Rect& bounds_in_pixel) {
  gfx::Display display = gfx::Display(gfx::Screen::GetPrimaryDisplay().id());
  display.SetScaleAndBounds(scale, bounds_in_pixel);
  std::vector<gfx::Display> displays;
  displays.push_back(display);
  aura::Env::GetInstance()->display_manager()->OnNativeDisplaysChanged(
      displays);
}

void AshTestBase::UpdateDisplay(const std::string& display_specs) {
  std::vector<gfx::Display> displays = CreateDisplaysFromString(display_specs);
  aura::Env::GetInstance()->display_manager()->
      OnNativeDisplaysChanged(displays);

  // On non-testing environment, when a secondary display is connected, a new
  // native (i.e. X) window for the display is always created below the previous
  // one for GPU performance reasons. Try to emulate the behavior.
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  DCHECK_EQ(displays.size(), root_windows.size());
  size_t next_y = 0;
  for (size_t i = 0; i < root_windows.size(); ++i) {
    const gfx::Size size = root_windows[i]->GetHostSize();
    root_windows[i]->SetHostBounds(gfx::Rect(gfx::Point(0, next_y), size));
    next_y += size.height();
  }
}

void AshTestBase::RunAllPendingInMessageLoop() {
#if !defined(OS_MACOSX)
  DCHECK(MessageLoopForUI::current() == &message_loop_);
  base::RunLoop run_loop(aura::Env::GetInstance()->GetDispatcher());
  run_loop.RunUntilIdle();
#endif
}

}  // namespace test
}  // namespace ash
