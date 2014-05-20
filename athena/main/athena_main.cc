// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shell/app/shell_main_delegate.h"
#include "apps/shell/browser/shell_browser_main_delegate.h"
#include "apps/shell/browser/shell_desktop_controller.h"
#include "athena/main/placeholder.h"
#include "athena/screen/public/screen_manager.h"
#include "athena/wm/public/window_manager.h"
#include "content/public/app/content_main.h"
#include "ui/aura/window_tree_host.h"

class AthenaBrowserMainDelegate : public apps::ShellBrowserMainDelegate {
 public:
  AthenaBrowserMainDelegate() {}
  virtual ~AthenaBrowserMainDelegate() {}

  // apps::ShellBrowserMainDelegate:
  virtual void Start(content::BrowserContext* context) OVERRIDE {
    athena::ScreenManager::Create(apps::ShellDesktopController::instance()
                                      ->GetWindowTreeHost()
                                      ->window());
    athena::WindowManager::Create();

    SetupBackgroundImage();
    CreateTestWindows();
  }

  virtual void Shutdown() OVERRIDE {
    athena::WindowManager::Shutdown();
    athena::ScreenManager::Shutdown();
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
