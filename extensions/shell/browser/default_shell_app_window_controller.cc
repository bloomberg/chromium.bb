// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/default_shell_app_window_controller.h"

#include "base/logging.h"
#include "extensions/shell/browser/shell_app_window.h"
#include "extensions/shell/browser/shell_desktop_controller.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"

namespace extensions {

DefaultShellAppWindowController::DefaultShellAppWindowController(
    ShellDesktopController* shell_desktop_controller)
    : shell_desktop_controller_(shell_desktop_controller) {
  DCHECK(shell_desktop_controller_);
}

DefaultShellAppWindowController::~DefaultShellAppWindowController() {
  // The app window must be explicitly closed before desktop teardown.
  DCHECK(!app_window_);
}

ShellAppWindow* DefaultShellAppWindowController::CreateAppWindow(
    content::BrowserContext* context,
    const Extension* extension) {
  aura::Window* root_window = shell_desktop_controller_->host()->window();

  app_window_.reset(new ShellAppWindow);
  app_window_->Init(context, extension, root_window->bounds().size());

  // Attach the web contents view to our window hierarchy.
  aura::Window* content = app_window_->GetNativeWindow();
  root_window->AddChild(content);
  content->Show();

  return app_window_.get();
}

void DefaultShellAppWindowController::CloseAppWindows() {
  app_window_.reset();
}

}  // namespace extensions
