// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_EXTENSIONS_ATHENA_APP_WINDOW_CLIENT_BASE_H_
#define ATHENA_EXTENSIONS_ATHENA_APP_WINDOW_CLIENT_BASE_H_

#include "base/macros.h"
#include "extensions/browser/app_window/app_window_client.h"

namespace athena {

// Athena's base impl of AppWindowClient.
class AthenaAppWindowClientBase : public extensions::AppWindowClient {
 public:
  AthenaAppWindowClientBase();
  virtual ~AthenaAppWindowClientBase();

 private:
  // extensions::AppWindowClient
  virtual extensions::NativeAppWindow* CreateNativeAppWindow(
      extensions::AppWindow* window,
      const extensions::AppWindow::CreateParams& params) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(AthenaAppWindowClientBase);
};

}  // namespace athena

#endif  // ATHENA_EXTENSIONS_ATHENA_APP_WINDOW_CLIENT_BASE_H_
