// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_EXTENSIONS_ATHENA_APPS_CLIENT_BASE_H_
#define ATHENA_EXTENSIONS_ATHENA_APPS_CLIENT_BASE_H_

#include "extensions/browser/app_window/apps_client.h"
#include "base/macros.h"

namespace athena {

// Athena's base impl of AppsClient.
class AthenaAppsClientBase : public extensions::AppsClient {
 public:
  AthenaAppsClientBase();
  virtual ~AthenaAppsClientBase();

 private:
  // extensions::AppsClient
  virtual extensions::NativeAppWindow* CreateNativeAppWindow(
      extensions::AppWindow* window,
      const extensions::AppWindow::CreateParams& params) OVERRIDE;
  virtual void IncrementKeepAliveCount() OVERRIDE;
  virtual void DecrementKeepAliveCount() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(AthenaAppsClientBase);
};

}  // namespace athena

#endif  // ATHENA_EXTENSIONS_ATHENA_APPS_CLIENT_BASE_H_
