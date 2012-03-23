// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "ash/launcher/launcher.h"
#include "ash/launcher/launcher_delegate.h"
#include "ash/launcher/launcher_model.h"
#include "ash/launcher/launcher_types.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_factory.h"
#include "ash/shell_window_ids.h"
#include "ash/shell/launcher_delegate_impl.h"
#include "ash/shell/shell_delegate_impl.h"
#include "ash/shell/shell_main_parts.h"
#include "ash/shell/window_watcher.h"
#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "ui/aura/env.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/compositor/test/compositor_test_support.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

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

namespace ash {
namespace shell {

void InitWindowTypeLauncher();

}  // namespace shell
}  // namespace ash

int main(int argc, char** argv) {
  CommandLine::Init(argc, argv);

  // The exit manager is in charge of calling the dtors of singleton objects.
  base::AtExitManager exit_manager;

  ash::shell::PreMainMessageLoopStart();

  // Create the message-loop here before creating the root window.
  MessageLoop message_loop(MessageLoop::TYPE_UI);
  ui::CompositorTestSupport::Initialize();

  // A ViewsDelegate is required.
  if (!views::ViewsDelegate::views_delegate)
    views::ViewsDelegate::views_delegate = new ShellViewsDelegate;

  ash::shell::ShellDelegateImpl* delegate = new ash::shell::ShellDelegateImpl;
  ash::Shell::CreateInstance(delegate);

  scoped_ptr<ash::shell::WindowWatcher> window_watcher(
      new ash::shell::WindowWatcher);
  delegate->SetWatcher(window_watcher.get());

  ash::shell::InitWindowTypeLauncher();

  ash::Shell::GetRootWindow()->ShowRootWindow();
  MessageLoopForUI::current()->Run();

  window_watcher.reset();

  ash::Shell::DeleteInstance();

  aura::Env::DeleteInstance();

  ui::CompositorTestSupport::Terminate();

  return 0;
}
