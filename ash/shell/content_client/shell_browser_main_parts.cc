// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell/content_client/shell_browser_main_parts.h"

#include "ash/shell/shell_delegate_impl.h"
#include "ash/shell/window_watcher.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/common/content_switches.h"
#include "content/shell/shell.h"
#include "content/shell/shell_browser_context.h"
#include "content/shell/shell_devtools_delegate.h"
#include "content/shell/shell_switches.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_module.h"
#include "ui/aura/client/stacking_client.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/test/compositor_test_support.h"
#include "ui/views/focus/accelerator_handler.h"
#include "ui/views/test/test_views_delegate.h"

namespace ash {
namespace shell {
void InitWindowTypeLauncher();

namespace {
class ShellViewsDelegate : public views::TestViewsDelegate {
 public:
  ShellViewsDelegate() {}
  virtual ~ShellViewsDelegate() {}

  // Overridden from views::TestViewsDelegate:
  virtual views::NonClientFrameView* CreateDefaultNonClientFrameView(
      views::Widget* widget) OVERRIDE {
    return ash::Shell::GetInstance()->CreateDefaultNonClientFrameView(widget);
  }
  bool UseTransparentWindows() const OVERRIDE {
    // Ash uses transparent window frames.
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellViewsDelegate);
};

}  // namespace

ShellBrowserMainParts::ShellBrowserMainParts(
    const content::MainFunctionParams& parameters)
    : BrowserMainParts(),
      devtools_delegate_(NULL) {
}

ShellBrowserMainParts::~ShellBrowserMainParts() {
}

#if !defined(OS_MACOSX)
void ShellBrowserMainParts::PreMainMessageLoopStart() {
}
#endif

int ShellBrowserMainParts::PreCreateThreads() {
  return 0;
}

void ShellBrowserMainParts::PreMainMessageLoopRun() {
  browser_context_.reset(new content::ShellBrowserContext);

  // A ViewsDelegate is required.
  if (!views::ViewsDelegate::views_delegate)
    views::ViewsDelegate::views_delegate = new ShellViewsDelegate;

  ash::shell::ShellDelegateImpl* delegate = new ash::shell::ShellDelegateImpl;
  ash::Shell::CreateInstance(delegate);
  ash::Shell::GetInstance()->set_browser_context(browser_context_.get());

  window_watcher_.reset(new ash::shell::WindowWatcher);
  delegate->SetWatcher(window_watcher_.get());

  ash::shell::InitWindowTypeLauncher();

  ash::Shell::GetRootWindow()->ShowRootWindow();
}

void ShellBrowserMainParts::PostMainMessageLoopRun() {
  if (devtools_delegate_)
    devtools_delegate_->Stop();
  browser_context_.reset();

  window_watcher_.reset();
  ash::Shell::DeleteInstance();
  aura::Env::DeleteInstance();
}

bool ShellBrowserMainParts::MainMessageLoopRun(int* result_code) {
  MessageLoopForUI::current()->Run();
  return true;
}

ui::Clipboard* ShellBrowserMainParts::GetClipboard() {
  if (!clipboard_.get())
    clipboard_.reset(new ui::Clipboard());
  return clipboard_.get();
}

}  // namespace shell
}  // namespace ash
