// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_EXTENSIONS_SHELL_ATHENA_SHELL_APP_WINDOW_CLIENT_H_
#define ATHENA_EXTENSIONS_SHELL_ATHENA_SHELL_APP_WINDOW_CLIENT_H_

#include "athena/extensions/athena_app_window_client_base.h"
#include "base/macros.h"

namespace athena {

class AthenaShellAppWindowClient : public AthenaAppWindowClientBase {
 public:
  AthenaShellAppWindowClient();
  ~AthenaShellAppWindowClient() override;

 private:
  // extensions::AppWindowClient
  extensions::AppWindow* CreateAppWindow(
      content::BrowserContext* context,
      const extensions::Extension* extension) override;
  void OpenDevToolsWindow(content::WebContents* web_contents,
                          const base::Closure& callback) override;
  bool IsCurrentChannelOlderThanDev() override;

  DISALLOW_COPY_AND_ASSIGN(AthenaShellAppWindowClient);
};

}  // namespace athena

#endif  // ATHENA_EXTENSIONS_SHELL_ATHENA_SHELL_APP_WINDOW_CLIENT_H_
