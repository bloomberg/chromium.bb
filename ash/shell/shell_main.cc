// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/launcher_types.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_factory.h"
#include "ash/shell_window_ids.h"
#include "ash/shell/example_factory.h"
#include "ash/shell/toplevel_window.h"
#include "ash/wm/window_util.h"
#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "ui/aura/root_window.h"
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

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellViewsDelegate);
};

class ShellDelegateImpl : public ash::ShellDelegate {
 public:
  ShellDelegateImpl() {
  }

  virtual void CreateNewWindow() OVERRIDE {
    ash::shell::ToplevelWindow::CreateParams create_params;
    create_params.can_resize = true;
    create_params.can_maximize = true;
    ash::shell::ToplevelWindow::CreateToplevelWindow(create_params);
  }

  virtual views::Widget* CreateStatusArea() OVERRIDE {
    return ash::internal::CreateStatusArea();
  }

  virtual void BuildAppListModel(ash::AppListModel* model) OVERRIDE {
    ash::shell::BuildAppListModel(model);
  }

  virtual ash::AppListViewDelegate* CreateAppListViewDelegate() OVERRIDE {
    return ash::shell::CreateAppListViewDelegate();
  }

  std::vector<aura::Window*> GetCycleWindowList() const OVERRIDE {
    aura::Window* default_container = ash::Shell::GetInstance()->GetContainer(
        ash::internal::kShellWindowId_DefaultContainer);
    std::vector<aura::Window*> windows = default_container->children();
    // Window cycling expects the topmost window at the front of the list.
    std::reverse(windows.begin(), windows.end());
    return windows;
  }

  virtual void LauncherItemClicked(
      const ash::LauncherItem& item) OVERRIDE {
    ash::ActivateWindow(item.window);
  }

  virtual bool ConfigureLauncherItem(ash::LauncherItem* item) OVERRIDE {
    static int image_count = 0;
    item->tab_images.resize(image_count + 1);
    for (int i = 0; i < image_count + 1; ++i) {
      item->tab_images[i].image.setConfig(SkBitmap::kARGB_8888_Config, 16, 16);
      item->tab_images[i].image.allocPixels();
      item->tab_images[i].image.eraseARGB(255,
                                          i == 0 ? 255 : 0,
                                          i == 1 ? 255 : 0,
                                          i == 2 ? 255 : 0);
    }
    image_count = (image_count + 1) % 3;
    return true;  // Makes the entry show up in the launcher.
  }
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

  ui::RegisterPathProvider();
  icu_util::Initialize();
  ResourceBundle::InitSharedInstance("en-US");

  // Create the message-loop here before creating the root window.
  MessageLoop message_loop(MessageLoop::TYPE_UI);
  ui::CompositorTestSupport::Initialize();

  // A ViewsDelegate is required.
  if (!views::ViewsDelegate::views_delegate)
    views::ViewsDelegate::views_delegate = new ShellViewsDelegate;

  ash::Shell::CreateInstance(new ShellDelegateImpl);

  ash::shell::InitWindowTypeLauncher();

  aura::RootWindow::GetInstance()->Run();

  ash::Shell::DeleteInstance();

  aura::RootWindow::DeleteInstance();

  ui::CompositorTestSupport::Terminate();

  return 0;
}
