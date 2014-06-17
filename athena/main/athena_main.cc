// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shell/app/shell_main_delegate.h"
#include "apps/shell/browser/shell_browser_main_delegate.h"
#include "apps/shell/browser/shell_desktop_controller.h"
#include "apps/shell/browser/shell_extension_system.h"
#include "athena/content/public/content_activity_factory.h"
#include "athena/content/public/content_app_model_builder.h"
#include "athena/home/public/home_card.h"
#include "athena/main/athena_app_window_controller.h"
#include "athena/main/athena_launcher.h"
#include "athena/main/placeholder.h"
#include "athena/main/url_search_provider.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "content/public/app/content_main.h"
#include "ui/aura/window_tree_host.h"
#include "ui/wm/core/visibility_controller.h"

namespace {
const std::string kAppSwitch = "app";
}  // namespace

class AthenaBrowserMainDelegate : public apps::ShellBrowserMainDelegate {
 public:
  AthenaBrowserMainDelegate() {}
  virtual ~AthenaBrowserMainDelegate() {}

  // apps::ShellBrowserMainDelegate:
  virtual void Start(content::BrowserContext* context) OVERRIDE {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(kAppSwitch)) {
      base::FilePath app_dir(command_line->GetSwitchValueNative(kAppSwitch));
      base::FilePath app_absolute_dir = base::MakeAbsoluteFilePath(app_dir);
      extensions::ShellExtensionSystem* extension_system =
          static_cast<extensions::ShellExtensionSystem*>(
              extensions::ExtensionSystem::Get(context));
      extension_system->LoadApp(app_absolute_dir);
    }

    athena::StartAthena(
        apps::ShellDesktopController::instance()->host()->window(),
        new athena::ContentActivityFactory(),
        new athena::ContentAppModelBuilder(context));
    athena::HomeCard::Get()->RegisterSearchProvider(
        new athena::UrlSearchProvider(context));
    CreateTestPages(context);
  }

  virtual void Shutdown() OVERRIDE { athena::ShutdownAthena(); }

  virtual apps::ShellDesktopController* CreateDesktopController() OVERRIDE {
    // TODO(mukai): create Athena's own ShellDesktopController subclass so that
    // it can initialize its own window manager logic.
    apps::ShellDesktopController* desktop = new apps::ShellDesktopController();
    desktop->SetAppWindowController(new athena::AthenaAppWindowController());
    return desktop;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AthenaBrowserMainDelegate);
};

class AthenaMainDelegate : public apps::ShellMainDelegate {
 public:
  AthenaMainDelegate() {}
  virtual ~AthenaMainDelegate() {}

 private:
  // apps::ShellMainDelegate:
  virtual apps::ShellBrowserMainDelegate* CreateShellBrowserMainDelegate()
      OVERRIDE {
    return new AthenaBrowserMainDelegate();
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
