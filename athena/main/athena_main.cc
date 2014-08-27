// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/activity/public/activity_factory.h"
#include "athena/activity/public/activity_manager.h"
#include "athena/content/public/web_contents_view_delegate_creator.h"
#include "athena/env/public/athena_env.h"
#include "athena/extensions/public/extensions_delegate.h"
#include "athena/main/athena_launcher.h"
#include "athena/screen/public/screen_manager.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "content/public/app/content_main.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/shell/app/shell_main_delegate.h"
#include "extensions/shell/browser/desktop_controller.h"
#include "extensions/shell/browser/shell_app_window.h"
#include "extensions/shell/browser/shell_browser_main_delegate.h"
#include "extensions/shell/browser/shell_content_browser_client.h"
#include "extensions/shell/browser/shell_extension_system.h"
#include "extensions/shell/common/switches.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/wm/core/visibility_controller.h"

namespace {

// We want to load the sample calculator app by default, for a while. Expecting
// to run athena_main at src/
const char kDefaultAppPath[] =
    "chrome/common/extensions/docs/examples/apps/calculator/app";

}  // namespace athena

class AthenaDesktopController : public extensions::DesktopController {
 public:
  AthenaDesktopController() {}
  virtual ~AthenaDesktopController() {}

 private:
  // extensions::DesktopController:
  virtual aura::WindowTreeHost* GetHost() OVERRIDE {
    return athena::AthenaEnv::Get()->GetHost();
  }

  // Creates a new app window and adds it to the desktop. The desktop maintains
  // ownership of the window.
  virtual extensions::ShellAppWindow* CreateAppWindow(
      content::BrowserContext* context,
      const extensions::Extension* extension) OVERRIDE {
    extensions::ShellAppWindow* app_window = new extensions::ShellAppWindow();
    app_window->Init(context, extension, gfx::Size(100, 100));
    athena::ActivityManager::Get()->AddActivity(
        athena::ActivityFactory::Get()->CreateAppActivity(app_window,
                                                          extension->id()));
    return app_window;
  }

  // Closes and destroys the app windows.
  virtual void CloseAppWindows() OVERRIDE {}

  DISALLOW_COPY_AND_ASSIGN(AthenaDesktopController);
};

class AthenaBrowserMainDelegate : public extensions::ShellBrowserMainDelegate {
 public:
  AthenaBrowserMainDelegate() {}
  virtual ~AthenaBrowserMainDelegate() {}

  // extensions::ShellBrowserMainDelegate:
  virtual void Start(content::BrowserContext* context) OVERRIDE {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

    base::FilePath app_dir = base::FilePath::FromUTF8Unsafe(
        command_line->HasSwitch(extensions::switches::kAppShellAppPath)
            ? command_line->GetSwitchValueNative(
                  extensions::switches::kAppShellAppPath)
            : kDefaultAppPath);

    base::FilePath app_absolute_dir = base::MakeAbsoluteFilePath(app_dir);
    if (base::DirectoryExists(app_absolute_dir)) {
      extensions::ShellExtensionSystem* extension_system =
          static_cast<extensions::ShellExtensionSystem*>(
              extensions::ExtensionSystem::Get(context));
      extension_system->LoadApp(app_absolute_dir);
    }

    athena::StartAthenaEnv(content::BrowserThread::GetMessageLoopProxyForThread(
        content::BrowserThread::FILE));
    athena::ExtensionsDelegate::CreateExtensionsDelegateForShell(context);
    athena::StartAthenaSessionWithContext(context);
  }

  virtual void Shutdown() OVERRIDE {
    athena::ShutdownAthena();
  }

  virtual extensions::DesktopController* CreateDesktopController() OVERRIDE {
    return new AthenaDesktopController();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AthenaBrowserMainDelegate);
};

class AthenaContentBrowserClient
    : public extensions::ShellContentBrowserClient {
 public:
  AthenaContentBrowserClient()
      : extensions::ShellContentBrowserClient(new AthenaBrowserMainDelegate()) {
  }
  virtual ~AthenaContentBrowserClient() {}

  // content::ContentBrowserClient:
  virtual content::WebContentsViewDelegate* GetWebContentsViewDelegate(
      content::WebContents* web_contents) OVERRIDE {
    return athena::CreateWebContentsViewDelegate(web_contents);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AthenaContentBrowserClient);
};

class AthenaMainDelegate : public extensions::ShellMainDelegate {
 public:
  AthenaMainDelegate() {}
  virtual ~AthenaMainDelegate() {}

 private:
  // extensions::ShellMainDelegate:
  virtual content::ContentBrowserClient* CreateShellContentBrowserClient()
      OVERRIDE {
    return new AthenaContentBrowserClient();
  }

  virtual void InitializeResourceBundle() OVERRIDE {
    base::FilePath pak_dir;
    PathService::Get(base::DIR_MODULE, &pak_dir);
    base::FilePath pak_file =
        pak_dir.Append(FILE_PATH_LITERAL("athena_resources.pak"));
    ui::ResourceBundle::InitSharedInstanceWithPakPath(pak_file);
  }

  DISALLOW_COPY_AND_ASSIGN(AthenaMainDelegate);
};

int main(int argc, const char** argv) {
  AthenaMainDelegate delegate;
  content::ContentMainParams params(&delegate);

  params.argc = argc;
  params.argv = argv;

  return content::ContentMain(params);
}
