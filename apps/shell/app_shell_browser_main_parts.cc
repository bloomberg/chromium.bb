// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shell/app_shell_browser_main_parts.h"

#include "apps/shell/web_view_window.h"
#include "base/run_loop.h"
#include "content/public/common/result_codes.h"
#include "content/shell/browser/shell_browser_context.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/test_screen.h"
#include "ui/wm/test/minimal_shell.h"

namespace apps {

AppShellBrowserMainParts::AppShellBrowserMainParts(
    const content::MainFunctionParams& parameters) {
}

AppShellBrowserMainParts::~AppShellBrowserMainParts() {
}

void AppShellBrowserMainParts::PreMainMessageLoopStart() {
  // TODO(jamescook): Initialize touch here?
}

void AppShellBrowserMainParts::PostMainMessageLoopStart() {
}

void AppShellBrowserMainParts::PreEarlyInitialization() {
}

void AppShellBrowserMainParts::PreMainMessageLoopRun() {
  // TODO(jamescook): Could initialize NetLog here to get logs from the
  // networking stack.
  // TODO(jamescook): Should this be an off-the-record context?
  browser_context_.reset(new content::ShellBrowserContext(false, NULL));

  // TODO(jamescook): Replace this with a real Screen implementation.
  gfx::Screen::SetScreenInstance(
      gfx::SCREEN_TYPE_NATIVE, aura::TestScreen::Create());
  // Set up basic pieces of views::corewm.
  minimal_shell_.reset(new wm::MinimalShell(gfx::Size(800, 600)));
  // Ensure the X window gets mapped.
  minimal_shell_->root_window()->host()->Show();

  // TODO(jamescook): Create an apps::ShellWindow here. For now, create a
  // window with a WebView just to ensure that the content module is properly
  // initialized.
  ShowWebViewWindow(browser_context_.get(),
                    minimal_shell_->root_window()->window());
}

bool AppShellBrowserMainParts::MainMessageLoopRun(int* result_code)  {
  base::RunLoop run_loop;
  run_loop.Run();
  *result_code = content::RESULT_CODE_NORMAL_EXIT;
  return true;
}

void AppShellBrowserMainParts::PostMainMessageLoopRun() {
  browser_context_.reset();
  minimal_shell_.reset();
  aura::Env::DeleteInstance();
}

}  // namespace apps
