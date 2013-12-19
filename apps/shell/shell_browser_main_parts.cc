// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shell/shell_browser_main_parts.h"

#include "apps/shell/shell_browser_context.h"
#include "apps/shell/shell_extension_system.h"
#include "apps/shell/shell_extensions_browser_client.h"
#include "apps/shell/shell_extensions_client.h"
#include "apps/shell/web_view_window.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "chromeos/chromeos_paths.h"
#include "content/public/common/result_codes.h"
#include "extensions/common/extension_paths.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/test_screen.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/screen.h"
#include "ui/wm/test/wm_test_helper.h"

using content::BrowserContext;
using extensions::Extension;

namespace apps {

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
  // Set up all the paths to load files.
  chrome::RegisterPathProvider();
  chromeos::RegisterPathProvider();
  extensions::RegisterPathProvider();

  // The extensions system needs manifest data from the Chrome PAK file.
  base::FilePath resources_pack_path;
  PathService::Get(chrome::FILE_RESOURCES_PACK, &resources_pack_path);
  ResourceBundle::GetSharedInstance().AddDataPackFromPath(
      resources_pack_path, ui::SCALE_FACTOR_NONE);

  // TODO(jamescook): Initialize chromeos::UserManager.

  // Initialize our "profile" equivalent.
  browser_context_.reset(new ShellBrowserContext);

  extensions_client_.reset(new ShellExtensionsClient());
  extensions::ExtensionsClient::Set(extensions_client_.get());

  extensions_browser_client_.reset(
      new ShellExtensionsBrowserClient(browser_context_.get()));
  extensions::ExtensionsBrowserClient::Set(extensions_browser_client_.get());

  // TODO(jamescook): Initialize policy::ProfilePolicyConnector.

  CreateExtensionSystem();

  // TODO(jamescook): Create the rest of the services using
  // BrowserContextDependencyManager::CreateBrowserContextServices.

  CreateRootWindow();

  const std::string kAppSwitch = "app";
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kAppSwitch)) {
    base::FilePath app_dir(command_line->GetSwitchValueNative(kAppSwitch));
    LoadAndLaunchApp(app_dir);
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
  // TODO(jamescook): Destroy the rest of the services with
  // BrowserContextDependencyManager::DestroyBrowserContextServices.
  extensions::ExtensionsBrowserClient::Set(NULL);
  extensions_browser_client_.reset();
  browser_context_.reset();
  aura::Env::DeleteInstance();

  LOG(WARNING) << "-----------------------------------";
  LOG(WARNING) << "app_shell is expected to crash now.";
  LOG(WARNING) << "-----------------------------------";
}

void ShellBrowserMainParts::OnRootWindowHostCloseRequested(
    const aura::RootWindow* root) {
  base::MessageLoop::current()->PostTask(FROM_HERE,
                                         base::MessageLoop::QuitClosure());
}

void ShellBrowserMainParts::CreateRootWindow() {
  // TODO(jamescook): Replace this with a real Screen implementation.
  gfx::Screen::SetScreenInstance(
      gfx::SCREEN_TYPE_NATIVE, aura::TestScreen::Create());
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
}

void ShellBrowserMainParts::CreateExtensionSystem() {
  DCHECK(browser_context_);
  extension_system_ =
      new extensions::ShellExtensionSystem(browser_context_.get());
  extensions::ExtensionSystemFactory::GetInstance()->SetCustomInstance(
      extension_system_);
  // Must occur after setting the instance above, as it will end up calling
  // ExtensionSystem::Get().
  extension_system_->InitForRegularProfile(true);
}

bool ShellBrowserMainParts::LoadAndLaunchApp(const base::FilePath& app_dir) {
  DCHECK(extension_system_);
  std::string load_error;
  scoped_refptr<Extension> extension =
      extension_file_util::LoadExtension(app_dir,
                                         extensions::Manifest::COMMAND_LINE,
                                         Extension::NO_FLAGS,
                                         &load_error);
  if (!extension) {
    LOG(ERROR) << "Loading extension at " << app_dir.value()
        << " failed with: " << load_error;
    return false;
  }

  // TODO(jamescook): Add to ExtensionRegistry.
  // TODO(jamescook): Set ExtensionSystem ready.
  // TODO(jamescook): Send NOTIFICATION_EXTENSION_LOADED.
  return true;
}

}  // namespace apps
