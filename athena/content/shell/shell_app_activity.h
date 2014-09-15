// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_CONTENT_SHELL_SHELL_APP_ACTIVITY_H_
#define ATHENA_CONTENT_SHELL_SHELL_APP_ACTIVITY_H_

#include "athena/content/app_activity.h"

#include "base/memory/scoped_ptr.h"

namespace extensions {
class AppWindow;
}

namespace athena {

class ShellAppActivity : public AppActivity {
 public:
  explicit ShellAppActivity(extensions::AppWindow* app_window);
  virtual ~ShellAppActivity();

 private:
  // ActivityViewModel:
  virtual views::Widget* CreateWidget() OVERRIDE;

  // AppActivity:
  virtual views::WebView* GetWebView() OVERRIDE;

  extensions::AppWindow* app_window_;

  DISALLOW_COPY_AND_ASSIGN(ShellAppActivity);
};

}  // namespace athena

#endif  // ATHENA_CONTENT_SHELL_SHELL_APP_ACTIVITY_H_
