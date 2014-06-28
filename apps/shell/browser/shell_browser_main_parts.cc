// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shell/browser/shell_browser_main_parts.h"

#include "apps/shell/browser/shell_browser_context.h"
#include "apps/shell/browser/shell_browser_main_delegate.h"
#include "apps/shell/browser/shell_desktop_controller.h"
#include "apps/shell/browser/shell_extension_system.h"
#include "apps/shell/browser/shell_extension_system_factory.h"
#include "apps/shell/browser/shell_extensions_browser_client.h"
#include "apps/shell/browser/shell_omaha_query_params_delegate.h"
#include "apps/shell/common/shell_extensions_client.h"
#include "apps/shell/common/switches.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/omaha_query_params/omaha_query_params.h"
#include "content/public/common/result_codes.h"
#include "content/shell/browser/shell_devtools_delegate.h"
#include "content/shell/browser/shell_net_log.h"
#include "extensions/browser/browser_context_keyed_service_factories.h"
#include "extensions/browser/extension_system.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/input_method_initializer.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_CHROMEOS)
#include "apps/shell/browser/shell_network_controller_chromeos.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#endif

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
    const content::MainFunctionParams& parameters,
    ShellBrowserMainDelegate* browser_main_delegate)
    : extension_system_(NULL),
      parameters_(parameters),
      run_message_loop_(true),
      browser_main_delegate_(browser_main_delegate) {
}

ShellBrowserMainParts::~ShellBrowserMainParts() {
}

void ShellBrowserMainParts::PreMainMessageLoopStart() {
  // TODO(jamescook): Initialize touch here?
}

void ShellBrowserMainParts::PostMainMessageLoopStart() {
#if defined(OS_CHROMEOS)
  chromeos::DBusThreadManager::Initialize();
  network_controller_.reset(new ShellNetworkController(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueNative(
          switches::kAppShellPreferredNetwork)));
#else
  // Non-Chrome OS platforms are for developer convenience, so use a test IME.
  ui::InitializeInputMethodForTesting();
#endif
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

  desktop_controller_.reset(browser_main_delegate_->CreateDesktopController());
  desktop_controller_->CreateRootWindow();

  // NOTE: Much of this is culled from chrome/test/base/chrome_test_suite.cc
  // TODO(jamescook): Initialize chromeos::UserManager.
  net_log_.reset(new content::ShellNetLog("app_shell"));

  extensions_client_.reset(new ShellExtensionsClient());
  extensions::ExtensionsClient::Set(extensions_client_.get());

  extensions_browser_client_.reset(
      new extensions::ShellExtensionsBrowserClient(browser_context_.get()));
  extensions::ExtensionsBrowserClient::Set(extensions_browser_client_.get());

  omaha_query_params_delegate_.reset(
      new extensions::ShellOmahaQueryParamsDelegate);
  omaha_query_params::OmahaQueryParams::SetDelegate(
      omaha_query_params_delegate_.get());

  // Create our custom ExtensionSystem first because other
  // KeyedServices depend on it.
  // TODO(yoz): Move this after EnsureBrowserContextKeyedServiceFactoriesBuilt.
  CreateExtensionSystem();

  ::EnsureBrowserContextKeyedServiceFactoriesBuilt();
  BrowserContextDependencyManager::GetInstance()->CreateBrowserContextServices(
      browser_context_.get());

  devtools_delegate_.reset(
      new content::ShellDevToolsDelegate(browser_context_.get()));
  if (parameters_.ui_task) {
    // For running browser tests.
    parameters_.ui_task->Run();
    delete parameters_.ui_task;
    run_message_loop_ = false;
  } else {
    browser_main_delegate_->Start(browser_context_.get());
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
  browser_main_delegate_->Shutdown();

  BrowserContextDependencyManager::GetInstance()->DestroyBrowserContextServices(
      browser_context_.get());
  extension_system_ = NULL;
  extensions::ExtensionsBrowserClient::Set(NULL);
  extensions_browser_client_.reset();
  browser_context_.reset();

  desktop_controller_.reset();
}

void ShellBrowserMainParts::PostDestroyThreads() {
#if defined(OS_CHROMEOS)
  network_controller_.reset();
  chromeos::DBusThreadManager::Shutdown();
#endif
}

void ShellBrowserMainParts::CreateExtensionSystem() {
  DCHECK(browser_context_);
  extension_system_ = static_cast<ShellExtensionSystem*>(
      ExtensionSystem::Get(browser_context_.get()));
  extension_system_->InitForRegularProfile(true);
}

}  // namespace apps
