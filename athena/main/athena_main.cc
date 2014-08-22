// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/content/public/web_contents_view_delegate_creator.h"
#include "athena/main/athena_app_window_controller.h"
#include "athena/main/athena_launcher.h"
#include "athena/screen/public/screen_manager.h"
#include "athena/screen/public/screen_manager_delegate.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "content/public/app/content_main.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/shell/app/shell_main_delegate.h"
#include "extensions/shell/browser/shell_browser_main_delegate.h"
#include "extensions/shell/browser/shell_content_browser_client.h"
#include "extensions/shell/browser/shell_desktop_controller.h"
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

class AthenaDesktopController : public extensions::ShellDesktopController {
 public:
  AthenaDesktopController() {}
  virtual ~AthenaDesktopController() {}

 private:
  // extensions::ShellDesktopController:
  virtual wm::FocusRules* CreateFocusRules() OVERRIDE {
    return athena::ScreenManager::CreateFocusRules();
  }

  DISALLOW_COPY_AND_ASSIGN(AthenaDesktopController);
};

class AthenaScreenManagerDelegate : public athena::ScreenManagerDelegate {
 public:
  explicit AthenaScreenManagerDelegate(
      extensions::ShellDesktopController* controller)
      : controller_(controller) {
  }

  virtual ~AthenaScreenManagerDelegate() {
  }

 private:
  // athena::ScreenManagerDelegate:
  virtual void SetWorkAreaInsets(const gfx::Insets& insets) OVERRIDE {
    controller_->SetDisplayWorkAreaInsets(insets);
  }

  // Not owned.
  extensions::ShellDesktopController* controller_;

  DISALLOW_COPY_AND_ASSIGN(AthenaScreenManagerDelegate);
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

    extensions::ShellDesktopController* desktop_controller =
        extensions::ShellDesktopController::instance();
    screen_manager_delegate_.reset(
        new AthenaScreenManagerDelegate(desktop_controller));
    athena::StartAthenaEnv(desktop_controller->host()->window(),
                           screen_manager_delegate_.get(),
                           content::BrowserThread::GetMessageLoopProxyForThread(
                               content::BrowserThread::FILE));
    athena::StartAthenaSessionWithContext(context);
  }

  virtual void Shutdown() OVERRIDE {
    athena::ShutdownAthena();
  }

  virtual extensions::ShellDesktopController* CreateDesktopController()
      OVERRIDE {
    // TODO(mukai): create Athena's own ShellDesktopController subclass so that
    // it can initialize its own window manager logic.
    extensions::ShellDesktopController* desktop = new AthenaDesktopController();
    desktop->SetAppWindowController(new athena::AthenaAppWindowController());
    return desktop;
  }

 private:
  scoped_ptr<AthenaScreenManagerDelegate> screen_manager_delegate_;

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
