// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shell/app/shell_main_delegate.h"
#include "apps/shell/browser/shell_browser_main_delegate.h"
#include "apps/shell/browser/shell_desktop_controller.h"
#include "athena/home/public/home_card.h"
#include "athena/main/placeholder.h"
#include "athena/screen/public/screen_manager.h"
#include "athena/wm/public/window_manager.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/app/content_main.h"
#include "ui/aura/window_tree_host.h"
#include "ui/wm/core/visibility_controller.h"

class AthenaBrowserMainDelegate : public apps::ShellBrowserMainDelegate {
 public:
  AthenaBrowserMainDelegate() {}
  virtual ~AthenaBrowserMainDelegate() {}

  // apps::ShellBrowserMainDelegate:
  virtual void Start(content::BrowserContext* context) OVERRIDE {
    aura::Window* root_window =
        apps::ShellDesktopController::instance()->GetWindowTreeHost()->window();
    visibility_controller_.reset(new ::wm::VisibilityController);
    aura::client::SetVisibilityClient(root_window,
                                      visibility_controller_.get());

    athena::ScreenManager::Create(root_window);
    athena::WindowManager::Create();
    athena::HomeCard::Create();

    SetupBackgroundImage();
    CreateTestWindows();
  }

  virtual void Shutdown() OVERRIDE {
    athena::HomeCard::Shutdown();
    athena::WindowManager::Shutdown();
    athena::ScreenManager::Shutdown();
    visibility_controller_.reset();
  }

  scoped_ptr< ::wm::VisibilityController> visibility_controller_;

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
