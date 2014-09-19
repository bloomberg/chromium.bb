// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/activity/public/activity_factory.h"
#include "athena/activity/public/activity_manager.h"
#include "athena/content/public/web_contents_view_delegate_creator.h"
#include "athena/env/public/athena_env.h"
#include "athena/extensions/public/extensions_delegate.h"
#include "athena/main/athena_content_client.h"
#include "athena/main/athena_renderer_pdf_helper.h"
#include "athena/main/public/athena_launcher.h"
#include "athena/screen/public/screen_manager.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "components/pdf/renderer/ppb_pdf_impl.h"
#include "content/public/app/content_main.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_client.h"
#include "extensions/shell/app/shell_main_delegate.h"
#include "extensions/shell/browser/desktop_controller.h"
#include "extensions/shell/browser/shell_app_delegate.h"
#include "extensions/shell/browser/shell_browser_main_delegate.h"
#include "extensions/shell/browser/shell_content_browser_client.h"
#include "extensions/shell/browser/shell_extension_system.h"
#include "extensions/shell/browser/shell_native_app_window.h"
#include "extensions/shell/common/shell_content_client.h"
#include "extensions/shell/common/switches.h"
#include "extensions/shell/renderer/shell_content_renderer_client.h"
#include "ppapi/c/private/ppb_pdf.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/wm/core/visibility_controller.h"

namespace {

// We want to load the sample calculator app by default, for a while. Expecting
// to run athena_main at src/
const char kDefaultAppPath[] =
    "chrome/common/extensions/docs/examples/apps/calculator/app";

}  // namespace

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
  // TODO(jamescook|oshima): Is this function needed?
  virtual extensions::AppWindow* CreateAppWindow(
      content::BrowserContext* context,
      const extensions::Extension* extension) OVERRIDE {
    NOTIMPLEMENTED();
    return NULL;
  }

  // Adds the window to the desktop.
  virtual void AddAppWindow(aura::Window* window) OVERRIDE {
    NOTIMPLEMENTED();
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

    athena::StartAthenaEnv(content::BrowserThread::GetBlockingPool()->
        GetTaskRunnerWithShutdownBehavior(
            base::SequencedWorkerPool::SKIP_ON_SHUTDOWN));
    athena::ExtensionsDelegate::CreateExtensionsDelegateForShell(context);
    athena::CreateVirtualKeyboardWithContext(context);
    athena::StartAthenaSessionWithContext(context);
  }

  virtual void Shutdown() OVERRIDE {
    athena::AthenaEnv::Get()->OnTerminating();
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

class AthenaContentRendererClient
    : public extensions::ShellContentRendererClient {
 public:
  AthenaContentRendererClient() {}
  virtual ~AthenaContentRendererClient() {}

  // content::ContentRendererClient:
  virtual void RenderFrameCreated(content::RenderFrame* render_frame) OVERRIDE {
    new athena::AthenaRendererPDFHelper(render_frame);
    extensions::ShellContentRendererClient::RenderFrameCreated(render_frame);
  }

  virtual const void* CreatePPAPIInterface(
      const std::string& interface_name) OVERRIDE {
    if (interface_name == PPB_PDF_INTERFACE)
      return pdf::PPB_PDF_Impl::GetInterface();
    return extensions::ShellContentRendererClient::CreatePPAPIInterface(
        interface_name);
  }
};

class AthenaMainDelegate : public extensions::ShellMainDelegate {
 public:
  AthenaMainDelegate() {}
  virtual ~AthenaMainDelegate() {}

 private:
  // extensions::ShellMainDelegate:
  virtual content::ContentClient* CreateContentClient() OVERRIDE {
    return new athena::AthenaContentClient();
  }
  virtual content::ContentBrowserClient* CreateShellContentBrowserClient()
      OVERRIDE {
    return new AthenaContentBrowserClient();
  }

  virtual content::ContentRendererClient* CreateShellContentRendererClient()
      OVERRIDE {
    return new AthenaContentRendererClient();
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
