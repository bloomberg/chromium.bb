// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shell/browser/shell_browser_main_parts.h"

#include "apps/shell/browser/shell_browser_context.h"
#include "apps/shell/browser/shell_desktop_controller.h"
#include "apps/shell/browser/shell_extension_system.h"
#include "apps/shell/browser/shell_extension_system_factory.h"
#include "apps/shell/browser/shell_extensions_browser_client.h"
#include "apps/shell/common/shell_extensions_client.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/common/result_codes.h"
#include "content/shell/browser/shell_devtools_delegate.h"
#include "content/shell/browser/shell_net_log.h"
#include "extensions/browser/browser_context_keyed_service_factories.h"
#include "extensions/browser/extension_system.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/resource/resource_bundle.h"

using content::BrowserContext;
using extensions::Extension;
using extensions::ExtensionSystem;
using extensions::ShellExtensionSystem;

namespace {

// Register additional KeyedService factories here. See
// ChromeBrowserMainExtraPartsProfiles for details.
void EnsureBrowserContextKeyedServiceFactoriesBuilt() {
  extensions::EnsureBrowserContextKeyedServiceFactoriesBuilt();
  extensions::ShellExtensionSystemFactory::GetInstance();
}

}  // namespace

namespace apps {

ShellBrowserMainParts::ShellBrowserMainParts(
    const content::MainFunctionParams& parameters)
    : extension_system_(NULL),
      parameters_(parameters),
      run_message_loop_(true) {}

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
  // Initialize our "profile" equivalent.
  browser_context_.reset(new ShellBrowserContext);

  desktop_controller_.reset(new ShellDesktopController);
  desktop_controller_->GetWindowTreeHost()->AddObserver(this);

  // NOTE: Much of this is culled from chrome/test/base/chrome_test_suite.cc
  // TODO(jamescook): Initialize chromeos::UserManager.
  net_log_.reset(new content::ShellNetLog("app_shell"));

  extensions_client_.reset(new ShellExtensionsClient());
  extensions::ExtensionsClient::Set(extensions_client_.get());

  extensions_browser_client_.reset(
      new extensions::ShellExtensionsBrowserClient(browser_context_.get()));
  extensions::ExtensionsBrowserClient::Set(extensions_browser_client_.get());

  // Create our custom ExtensionSystem first because other
  // KeyedServices depend on it.
  // TODO(yoz): Move this after EnsureBrowserContextKeyedServiceFactoriesBuilt.
  CreateExtensionSystem();

  ::EnsureBrowserContextKeyedServiceFactoriesBuilt();
  BrowserContextDependencyManager::GetInstance()->CreateBrowserContextServices(
      browser_context_.get());

  devtools_delegate_.reset(
      new content::ShellDevToolsDelegate(browser_context_.get()));

  const std::string kAppSwitch = "app";
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kAppSwitch)) {
    base::FilePath app_dir(command_line->GetSwitchValueNative(kAppSwitch));
    base::FilePath app_absolute_dir = base::MakeAbsoluteFilePath(app_dir);
    extension_system_->LoadAndLaunchApp(app_absolute_dir);
  } else if (parameters_.ui_task) {
    // For running browser tests.
    parameters_.ui_task->Run();
    delete parameters_.ui_task;
    run_message_loop_ = false;
  } else {
    LOG(ERROR) << "--" << kAppSwitch << " unset; boredom is in your future";
  }
}

bool ShellBrowserMainParts::MainMessageLoopRun(int* result_code)  {
  if (!run_message_loop_)
    return true;
  // TODO(yoz): just return false here?
  base::RunLoop run_loop;
  run_loop.Run();
  *result_code = content::RESULT_CODE_NORMAL_EXIT;
  return true;
}

void ShellBrowserMainParts::PostMainMessageLoopRun() {
  BrowserContextDependencyManager::GetInstance()->DestroyBrowserContextServices(
      browser_context_.get());
  extension_system_ = NULL;
  extensions::ExtensionsBrowserClient::Set(NULL);
  extensions_browser_client_.reset();
  browser_context_.reset();

  desktop_controller_->GetWindowTreeHost()->RemoveObserver(this);
  desktop_controller_.reset();
}

void ShellBrowserMainParts::OnHostCloseRequested(
    const aura::WindowTreeHost* host) {
  desktop_controller_->CloseAppWindow();
  base::MessageLoop::current()->PostTask(FROM_HERE,
                                         base::MessageLoop::QuitClosure());
}

void ShellBrowserMainParts::CreateExtensionSystem() {
  DCHECK(browser_context_);
  extension_system_ = static_cast<ShellExtensionSystem*>(
      ExtensionSystem::Get(browser_context_.get()));
  extension_system_->InitForRegularProfile(true);
}

}  // namespace apps
