// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_MAIN_ATHENA_APP_WINDOW_CONTROLLER_H_
#define ATHENA_MAIN_ATHENA_APP_WINDOW_CONTROLLER_H_

#include "apps/shell/browser/shell_app_window_controller.h"
#include "base/macros.h"

namespace athena {

// The shell app window controller for athena. It embeds the web_contents of
// an app window into an Athena activity.
class AthenaAppWindowController : public apps::ShellAppWindowController {
 public:
  AthenaAppWindowController();
  virtual ~AthenaAppWindowController();

  // Overridden from apps::ShellAppWindowController:
  virtual apps::ShellAppWindow* CreateAppWindow(
      content::BrowserContext* context) OVERRIDE;
  virtual void CloseAppWindows() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(AthenaAppWindowController);
};

}  // namespace athena

#endif  // ATHENA_MAIN_ATHENA_APP_WINDOW_CONTROLLER_H_
