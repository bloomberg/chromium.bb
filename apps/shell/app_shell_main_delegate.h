// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_SHELL_APP_SHELL_MAIN_DELEGATE_H_
#define APPS_SHELL_APP_SHELL_MAIN_DELEGATE_H_

#include "apps/shell/app_shell_content_client.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/app/content_main_delegate.h"

namespace apps {

class AppShellContentBrowserClient;

class AppShellMainDelegate : public content::ContentMainDelegate {
 public:
  AppShellMainDelegate();
  virtual ~AppShellMainDelegate();

  // ContentMainDelegate implementation:
  virtual bool BasicStartupComplete(int* exit_code) OVERRIDE;
  virtual void PreSandboxStartup() OVERRIDE;
  virtual content::ContentBrowserClient* CreateContentBrowserClient() OVERRIDE;

  static void InitializeResourceBundle();

 private:
  scoped_ptr<AppShellContentBrowserClient> browser_client_;
  AppShellContentClient content_client_;

  DISALLOW_COPY_AND_ASSIGN(AppShellMainDelegate);
};

}  // namespace apps

#endif  // APPS_SHELL_APP_SHELL_MAIN_DELEGATE_H_
