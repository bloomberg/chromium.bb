// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_CONTENT_SHELL_SHELL_APP_ACTIVITY_H_
#define ATHENA_CONTENT_SHELL_SHELL_APP_ACTIVITY_H_

#include "athena/content/app_activity.h"

#include "base/memory/scoped_ptr.h"

namespace extensions {
class ShellAppWindow;
}

namespace athena {

class ShellAppActivity : public AppActivity {
 public:
  explicit ShellAppActivity(extensions::ShellAppWindow* app_window);
  virtual ~ShellAppActivity();

 private:
  // AppActivity:
  virtual content::WebContents* GetWebContents() OVERRIDE;

  scoped_ptr<extensions::ShellAppWindow> shell_app_window_;

  DISALLOW_COPY_AND_ASSIGN(ShellAppActivity);
};

}  // namespace athena

#endif  // ATHENA_CONTENT_SHELL_SHELL_APP_ACTIVITY_H_
