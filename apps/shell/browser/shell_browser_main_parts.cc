// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shell/browser/shell_browser_main_parts.h"

#include "apps/shell/browser/shell_browser_context.h"
#include "apps/shell/browser/shell_extension_system.h"
#include "apps/shell/browser/shell_extension_system_factory.h"
#include "apps/shell/browser/shell_extensions_browser_client.h"
#include "apps/shell/browser/web_view_window.h"
#include "apps/shell/common/shell_extensions_client.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"
#include "content/public/common/result_codes.h"
#include "content/shell/browser/shell_devtools_delegate.h"
#include "content/shell/browser/shell_net_log.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/renderer_startup_helper.h"
#include "ui/aura/env.h"
#include "ui/aura/test/test_screen.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/ime/input_method_initializer.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/screen.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/test/wm_test_helper.h"

using content::BrowserContext;
using extensions::Extension;
using extensions::ExtensionSystem;
using extensions::ShellExtensionSystem;

namespace apps {
namespace {

// Register additional BrowserContextKeyedService factories here. See
// ChromeBrowserMainExtraPartsProfiles for details.
void EnsureBrowserContextKeyedServiceFactoriesBuilt() {
  extensions::RendererStartupHelperFactory::GetInstance();
  extensions::ShellExtensionSystemFactory::GetInstance();
}

// A ViewsDelegate to attach new unparented windows to app_shell's root window.
class ShellViewsDelegate : public views::TestViewsDelegate {
 public:
  explicit ShellViewsDelegate(aura::Window* root_window)
      : root_window_(root_window) {}
  virtual ~ShellViewsDelegate() {}

  // views::ViewsDelegate implementation.
  virtual void OnBeforeWidgetInit(
      views::Widget::InitParams* params,
      views::internal::NativeWidgetDelegate* delegate) OVERRIDE {
    if (!params->parent)
      params->parent = root_window_;
  }

 private:
  aura::Window* root_window_;

  DISALLOW_COPY_AND_ASSIGN(ShellViewsDelegate);
};

}  // namespace

ShellBrowserMainParts::ShellBrowserMainParts(
    const content::MainFunctionParams& parameters)
    : extension_system_(NULL) {
}

ShellBrowserMainParts::~ShellBrowserMainParts() {
}

void ShellBrowserMainParts::PreMainMessageLoopStart() {
  // TODO(jamescook): Initialize touch here?
}

void ShellBrowserMainParts::PostMainMessageLoopStart() {
}

void ShellBrowserMainParts::PreEarlyInitialization() {
}

int ShellBrowserMainParts::PreCreateThreads() {
  // TODO(jamescook): Initialize chromeos::CrosSettings here?

  // Return no error.
  return 0;
}

void ShellBrowserMainParts::PreMainMessageLoopRun() {
  // NOTE: Much of this is culled from chrome/test/base/chrome_test_suite.cc

  // TODO(jamescook): Initialize chromeos::UserManager.

  net_log_.reset(new content::ShellNetLog("app_shell"));

  // Initialize our "profile" equivalent.
  browser_context_.reset(new ShellBrowserContext);

  extensions_client_.reset(new ShellExtensionsClient());
  extensions::ExtensionsClient::Set(extensions_client_.get());

  extensions_browser_client_.reset(
      new extensions::ShellExtensionsBrowserClient(browser_context_.get()));
  extensions::ExtensionsBrowserClient::Set(extensions_browser_client_.get());

  // Create our custom ExtensionSystem first because other
  // BrowserContextKeyedServices depend on it.
  // TODO(yoz): Move this after EnsureBrowserContextKeyedServiceFactoriesBuilt.
  CreateExtensionSystem();

  EnsureBrowserContextKeyedServiceFactoriesBuilt();
  BrowserContextDependencyManager::GetInstance()->CreateBrowserContextServices(
      browser_context_.get());

  devtools_delegate_.reset(
      new content::ShellDevToolsDelegate(browser_context_.get()));

  CreateRootWindow();
  CreateViewsDelegate();

  const std::string kAppSwitch = "app";
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kAppSwitch)) {
    base::FilePath app_dir(command_line->GetSwitchValueNative(kAppSwitch));
    base::FilePath app_absolute_dir = base::MakeAbsoluteFilePath(app_dir);
    extension_system_->LoadAndLaunchApp(app_absolute_dir);
  } else {
    // TODO(jamescook): For demo purposes create a window with a WebView just
    // to ensure that the content module is properly initialized.
    webview_window_.reset(CreateWebViewWindow(browser_context_.get(),
        wm_test_helper_->dispatcher()->window()));
    webview_window_->Show();
  }
}

bool ShellBrowserMainParts::MainMessageLoopRun(int* result_code)  {
  base::RunLoop run_loop;
  run_loop.Run();
  *result_code = content::RESULT_CODE_NORMAL_EXIT;
  return true;
}

void ShellBrowserMainParts::PostMainMessageLoopRun() {
  devtools_delegate_->Stop();
  DestroyViewsDelegate();
  DestroyRootWindow();
  BrowserContextDependencyManager::GetInstance()->DestroyBrowserContextServices(
      browser_context_.get());
  extension_system_ = NULL;
  extensions::ExtensionsBrowserClient::Set(NULL);
  extensions_browser_client_.reset();
  browser_context_.reset();
  aura::Env::DeleteInstance();
}

void ShellBrowserMainParts::OnWindowTreeHostCloseRequested(
    const aura::WindowEventDispatcher* dispatcher) {
  base::MessageLoop::current()->PostTask(FROM_HERE,
                                         base::MessageLoop::QuitClosure());
}

void ShellBrowserMainParts::CreateRootWindow() {
  test_screen_.reset(aura::TestScreen::Create());
  // TODO(jamescook): Replace this with a real Screen implementation.
  gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE, test_screen_.get());
  // TODO(jamescook): Initialize a real input method.
  ui::InitializeInputMethodForTesting();
  // Set up basic pieces of views::corewm.
  wm_test_helper_.reset(new wm::WMTestHelper(gfx::Size(800, 600)));
  // Ensure the X window gets mapped.
  wm_test_helper_->dispatcher()->host()->Show();
  // Watch for the user clicking the close box.
  wm_test_helper_->dispatcher()->AddRootWindowObserver(this);
}

void ShellBrowserMainParts::DestroyRootWindow() {
  // We should close widget before destroying root window.
  webview_window_.reset();
  wm_test_helper_->dispatcher()->RemoveRootWindowObserver(this);
  wm_test_helper_->dispatcher()->PrepareForShutdown();
  wm_test_helper_.reset();
  ui::ShutdownInputMethodForTesting();
}

void ShellBrowserMainParts::CreateViewsDelegate() {
  DCHECK(!views::ViewsDelegate::views_delegate);
  views::ViewsDelegate::views_delegate =
      new ShellViewsDelegate(wm_test_helper_->dispatcher()->window());
}

void ShellBrowserMainParts::DestroyViewsDelegate() {
  delete views::ViewsDelegate::views_delegate;
  views::ViewsDelegate::views_delegate = NULL;
}

void ShellBrowserMainParts::CreateExtensionSystem() {
  DCHECK(browser_context_);
  extension_system_ = static_cast<ShellExtensionSystem*>(
      ExtensionSystem::Get(browser_context_.get()));
  extension_system_->InitForRegularProfile(true);
}

}  // namespace apps
