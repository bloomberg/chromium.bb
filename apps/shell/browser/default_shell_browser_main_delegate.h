// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_SHELL_BROWSER_DEFAULT_SHELL_BROWSER_MAIN_DELEGATE_H_
#define APPS_SHELL_BROWSER_DEFAULT_SHELL_BROWSER_MAIN_DELEGATE_H_

#include "apps/shell/browser/shell_browser_main_delegate.h"
#include "base/compiler_specific.h"
#include "base/macros.h"

namespace apps {

// A ShellBrowserMainDelegate that starts an application specified
// by the "--app" command line. This is used only in the browser process.
class DefaultShellBrowserMainDelegate : public ShellBrowserMainDelegate {
 public:
  DefaultShellBrowserMainDelegate();
  virtual ~DefaultShellBrowserMainDelegate();

  // ShellBrowserMainDelegate:
  virtual void Start(content::BrowserContext* context) OVERRIDE;
  virtual void Shutdown() OVERRIDE;
  virtual ShellDesktopController* CreateDesktopController() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultShellBrowserMainDelegate);
};

}  // namespace apps

#endif  // DEFAULT_SHELL_BROWSER_MAIN_DELEGATE_H_
