// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell/content_client/shell_browser_main_parts.h"

#include "ash/ash_switches.h"
#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/shell.h"
#include "ash/shell/shell_delegate_impl.h"
#include "ash/shell/window_watcher.h"
#include "ash/shell_init_params.h"
#include "ash/system/user/login_status.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/browser/context_factory.h"
#include "content/public/common/content_switches.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_net_log.h"
#include "net/base/net_module.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/screen.h"
#include "ui/message_center/message_center.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/wm/core/wm_state.h"

#if defined(USE_X11)
#include "ui/events/x/touch_factory_x11.h"
#endif

#if defined(OS_CHROMEOS)
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#endif

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
  virtual void OnBeforeWidgetInit(
      views::Widget::InitParams* params,
      views::internal::NativeWidgetDelegate* delegate) OVERRIDE {
    if (params->opacity == views::Widget::InitParams::INFER_OPACITY)
      params->opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;

    if (params->native_widget)
      return;

    if (!params->parent && !params->context && !params->child)
      params->context = Shell::GetPrimaryRootWindow();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellViewsDelegate);
};

}  // namespace

ShellBrowserMainParts::ShellBrowserMainParts(
    const content::MainFunctionParams& parameters)
    : BrowserMainParts(),
      delegate_(NULL) {
}

ShellBrowserMainParts::~ShellBrowserMainParts() {
}

void ShellBrowserMainParts::PreMainMessageLoopStart() {
#if defined(USE_X11)
  ui::TouchFactory::SetTouchDeviceListFromCommandLine();
#endif
}

void ShellBrowserMainParts::PostMainMessageLoopStart() {
#if defined(OS_CHROMEOS)
  chromeos::DBusThreadManager::Initialize();
#endif
}

void ShellBrowserMainParts::ToolkitInitialized() {
  wm_state_.reset(new wm::WMState);
}

void ShellBrowserMainParts::PreMainMessageLoopRun() {
  net_log_.reset(new content::ShellNetLog("ash_shell"));
  browser_context_.reset(new content::ShellBrowserContext(
      false, net_log_.get()));

  // A ViewsDelegate is required.
  if (!views::ViewsDelegate::views_delegate)
    views::ViewsDelegate::views_delegate = new ShellViewsDelegate;

  delegate_ = new ash::shell::ShellDelegateImpl;
  // The global message center state must be initialized absent
  // g_browser_process.
  message_center::MessageCenter::Initialize();

#if defined(OS_CHROMEOS)
  // Create CrasAudioHandler for testing since g_browser_process
  // is absent.
  chromeos::CrasAudioHandler::InitializeForTesting();
#endif

  ash::ShellInitParams init_params;
  init_params.delegate = delegate_;
  init_params.context_factory = content::GetContextFactory();
  ash::Shell::CreateInstance(init_params);
  delegate_->set_browser_context(browser_context_.get());
  ash::Shell::GetInstance()->CreateShelf();
  ash::Shell::GetInstance()->UpdateAfterLoginStatusChange(
      user::LOGGED_IN_USER);

  window_watcher_.reset(new ash::shell::WindowWatcher);
  gfx::Screen* screen = Shell::GetInstance()->GetScreen();
  screen->AddObserver(window_watcher_.get());
  delegate_->SetWatcher(window_watcher_.get());

  ash::shell::InitWindowTypeLauncher();

  ash::Shell::GetPrimaryRootWindow()->GetHost()->Show();
}

void ShellBrowserMainParts::PostMainMessageLoopRun() {
  gfx::Screen* screen = Shell::GetInstance()->GetScreen();
  screen->RemoveObserver(window_watcher_.get());

  window_watcher_.reset();
  delegate_->SetWatcher(NULL);
  delegate_ = NULL;
  ash::Shell::DeleteInstance();
  // The global message center state must be shutdown absent
  // g_browser_process.
  message_center::MessageCenter::Shutdown();

#if defined(OS_CHROMEOS)
  chromeos::CrasAudioHandler::Shutdown();
#endif

  aura::Env::DeleteInstance();

  // The keyboard may have created a WebContents. The WebContents is destroyed
  // with the UI, and it needs the BrowserContext to be alive during its
  // destruction. So destroy all of the UI elements before destroying the
  // browser context.
  browser_context_.reset();
}

bool ShellBrowserMainParts::MainMessageLoopRun(int* result_code) {
  base::MessageLoopForUI::current()->Run();
  return true;
}

}  // namespace shell
}  // namespace ash
