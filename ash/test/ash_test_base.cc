// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_test_base.h"

#include <vector>

#include "ash/shell.h"
#include "ash/test/test_shell_delegate.h"
#include "content/public/test/web_contents_tester.h"
#include "ui/aura/env.h"
#include "ui/aura/monitor_manager.h"
#include "ui/aura/root_window.h"
#include "ui/base/ime/text_input_test_support.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"

namespace ash {
namespace test {

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

void AshTestBase::ChangeMonitorConfig(float scale,
                                      const gfx::Rect& bounds_in_pixel) {
  gfx::Display display = gfx::Display(gfx::Screen::GetPrimaryMonitor().id());
  display.SetScaleAndBounds(scale, bounds_in_pixel);
  std::vector<gfx::Display> displays;
  displays.push_back(display);
  aura::Env::GetInstance()->monitor_manager()->OnNativeMonitorsChanged(
      displays);
}

void AshTestBase::RunAllPendingInMessageLoop() {
#if !defined(OS_MACOSX)
  message_loop_.RunAllPendingWithDispatcher(
      aura::Env::GetInstance()->GetDispatcher());
#endif
}

}  // namespace test
}  // namespace ash
