// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shell/browser/shell_browser_main_parts.h"

#include "apps/shell/browser/shell_browser_context.h"
#include "apps/shell/browser/shell_extension_system.h"
#include "apps/shell/browser/shell_extensions_browser_client.h"
#include "apps/shell/browser/web_view_window.h"
#include "apps/shell/common/shell_extensions_client.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"
#include "content/public/common/result_codes.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/renderer_startup_helper.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/test_screen.h"
#include "ui/base/ime/input_method_initializer.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/screen.h"
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
}

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

  // Initialize our "profile" equivalent.
  browser_context_.reset(new ShellBrowserContext);

  extensions_client_.reset(new ShellExtensionsClient());
  extensions::ExtensionsClient::Set(extensions_client_.get());

  extensions_browser_client_.reset(
      new extensions::ShellExtensionsBrowserClient(browser_context_.get()));
  extensions::ExtensionsBrowserClient::Set(extensions_browser_client_.get());

  // Create our custom ExtensionSystem first because other
  // BrowserContextKeyedServices depend on it.
  CreateExtensionSystem();

  EnsureBrowserContextKeyedServiceFactoriesBuilt();
  BrowserContextDependencyManager::GetInstance()->CreateBrowserContextServices(
      browser_context_.get());

  CreateRootWindow();

  const std::string kAppSwitch = "app";
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kAppSwitch)) {
    base::FilePath app_dir(command_line->GetSwitchValueNative(kAppSwitch));
    base::FilePath app_absolute_dir = base::MakeAbsoluteFilePath(app_dir);
    extension_system_->LoadAndLaunchApp(app_absolute_dir);
  } else {
    // TODO(jamescook): For demo purposes create a window with a WebView just
    // to ensure that the content module is properly initialized.
    ShowWebViewWindow(browser_context_.get(),
                      wm_test_helper_->root_window()->window());
  }
}

bool ShellBrowserMainParts::MainMessageLoopRun(int* result_code)  {
  base::RunLoop run_loop;
  run_loop.Run();
  *result_code = content::RESULT_CODE_NORMAL_EXIT;
  return true;
}

void ShellBrowserMainParts::PostMainMessageLoopRun() {
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
    const aura::RootWindow* root) {
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
  wm_test_helper_->root_window()->host()->Show();
  // Watch for the user clicking the close box.
  wm_test_helper_->root_window()->AddRootWindowObserver(this);
}

void ShellBrowserMainParts::DestroyRootWindow() {
  wm_test_helper_->root_window()->RemoveRootWindowObserver(this);
  wm_test_helper_->root_window()->PrepareForShutdown();
  wm_test_helper_.reset();
  ui::ShutdownInputMethodForTesting();
}

void ShellBrowserMainParts::CreateExtensionSystem() {
  DCHECK(browser_context_);
  extension_system_ = static_cast<ShellExtensionSystem*>(
      ExtensionSystem::Get(browser_context_.get()));
  extension_system_->InitForRegularProfile(true);
}

}  // namespace apps
