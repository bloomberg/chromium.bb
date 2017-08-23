// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_desktop_controller_mac.h"

#include "base/run_loop.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/shell/browser/shell_app_window_client.h"
#include "ui/base/base_window.h"

namespace extensions {

ShellDesktopControllerMac::ShellDesktopControllerMac()
    : app_window_client_(new ShellAppWindowClient), app_window_(NULL) {
  AppWindowClient::Set(app_window_client_.get());
}

ShellDesktopControllerMac::~ShellDesktopControllerMac() {
  // TODO(yoz): This is actually too late to close app windows (for tests).
  // Maybe this is useful for non-tests.
  CloseAppWindows();
}

void ShellDesktopControllerMac::Run() {
  base::RunLoop run_loop;
  run_loop.Run();
}

void ShellDesktopControllerMac::AddAppWindow(AppWindow* app_window,
                                             gfx::NativeWindow window) {
  app_window_ = app_window;
}

void ShellDesktopControllerMac::CloseAppWindows() {
  if (app_window_) {
    ui::BaseWindow* window = app_window_->GetBaseWindow();
    window->Close();  // Close() deletes |app_window_|.
    app_window_ = NULL;
  }
}

}  // namespace extensions
