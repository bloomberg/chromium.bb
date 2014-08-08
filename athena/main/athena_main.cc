// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/content/public/content_activity_factory.h"
#include "athena/content/public/content_app_model_builder.h"
#include "athena/content/public/web_contents_view_delegate_creator.h"
#include "athena/home/public/home_card.h"
#include "athena/main/athena_app_window_controller.h"
#include "athena/main/athena_launcher.h"
#include "athena/main/debug/debug_window.h"
#include "athena/main/placeholder.h"
#include "athena/main/url_search_provider.h"
#include "athena/screen/public/screen_manager.h"
#include "athena/virtual_keyboard/public/virtual_keyboard_manager.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "content/public/app/content_main.h"
#include "extensions/shell/app/shell_main_delegate.h"
#include "extensions/shell/browser/shell_browser_main_delegate.h"
#include "extensions/shell/browser/shell_content_browser_client.h"
#include "extensions/shell/browser/shell_desktop_controller.h"
#include "extensions/shell/browser/shell_extension_system.h"
#include "extensions/shell/common/switches.h"
#include "extensions/shell/renderer/shell_renderer_main_delegate.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_controller_observer.h"
#include "ui/native_theme/native_theme_switches.h"
#include "ui/wm/core/visibility_controller.h"

namespace {

// We want to load the sample calculator app by default, for a while. Expecting
// to run athena_main at src/
const char kDefaultAppPath[] =
    "chrome/common/extensions/docs/examples/apps/calculator/app";
}  // namespace

// This class observes the change of the virtual keyboard and distribute the
// change to appropriate modules of athena.
// TODO(oshima): move the VK bounds logic to screen manager.
class VirtualKeyboardObserver : public keyboard::KeyboardControllerObserver {
 public:
  VirtualKeyboardObserver() {
    keyboard::KeyboardController::GetInstance()->AddObserver(this);
  }

  virtual ~VirtualKeyboardObserver() {
    keyboard::KeyboardController::GetInstance()->RemoveObserver(this);
  }

 private:
  virtual void OnKeyboardBoundsChanging(const gfx::Rect& new_bounds) OVERRIDE {
    athena::HomeCard::Get()->UpdateVirtualKeyboardBounds(new_bounds);
  }

  DISALLOW_COPY_AND_ASSIGN(VirtualKeyboardObserver);
};

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

class AthenaBrowserMainDelegate : public extensions::ShellBrowserMainDelegate {
 public:
  AthenaBrowserMainDelegate() {}
  virtual ~AthenaBrowserMainDelegate() {}

  // extensions::ShellBrowserMainDelegate:
  virtual void Start(content::BrowserContext* context) OVERRIDE {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

    // Force showing in the experimental app-list view.
    command_line->AppendSwitch(app_list::switches::kEnableExperimentalAppList);
    command_line->AppendSwitch(switches::kEnableOverlayScrollbar);

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

    athena::StartAthena(
        extensions::ShellDesktopController::instance()->host()->window(),
        new athena::ContentActivityFactory(),
        new athena::ContentAppModelBuilder(context));
    athena::HomeCard::Get()->RegisterSearchProvider(
        new athena::UrlSearchProvider(context));
    athena::VirtualKeyboardManager::Create(context);
    keyboard_observer_.reset(new VirtualKeyboardObserver());

    CreateTestPages(context);
    CreateDebugWindow();
  }

  virtual void Shutdown() OVERRIDE {
    keyboard_observer_.reset();
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
  scoped_ptr<VirtualKeyboardObserver> keyboard_observer_;

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

class AthenaRendererMainDelegate
    : public extensions::ShellRendererMainDelegate {
 public:
  AthenaRendererMainDelegate() {}
  virtual ~AthenaRendererMainDelegate() {}

 private:
  // extensions::ShellRendererMainDelegate:
  virtual void OnThreadStarted(content::RenderThread* thread) OVERRIDE {}

  virtual void OnViewCreated(content::RenderView* render_view) OVERRIDE {
  }

  DISALLOW_COPY_AND_ASSIGN(AthenaRendererMainDelegate);
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

  virtual scoped_ptr<extensions::ShellRendererMainDelegate>
  CreateShellRendererMainDelegate() OVERRIDE {
    return scoped_ptr<extensions::ShellRendererMainDelegate>(
        new AthenaRendererMainDelegate());
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
