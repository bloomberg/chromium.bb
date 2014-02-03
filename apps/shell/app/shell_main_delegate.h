// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_SHELL_APP_SHELL_MAIN_DELEGATE_H_
#define APPS_SHELL_APP_SHELL_MAIN_DELEGATE_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/app/content_main_delegate.h"

namespace apps {

class ShellContentBrowserClient;
class ShellContentClient;
class ShellContentRendererClient;

class ShellMainDelegate : public content::ContentMainDelegate {
 public:
  ShellMainDelegate();
  virtual ~ShellMainDelegate();

  // ContentMainDelegate implementation:
  virtual bool BasicStartupComplete(int* exit_code) OVERRIDE;
  virtual void PreSandboxStartup() OVERRIDE;
  virtual content::ContentBrowserClient* CreateContentBrowserClient() OVERRIDE;
  virtual content::ContentRendererClient* CreateContentRendererClient()
      OVERRIDE;

 private:
  // |process_type| is zygote, renderer, utility, etc. Returns true if the
  // process needs data from resources.pak.
  static bool ProcessNeedsResourceBundle(const std::string& process_type);

  // Initializes the resource bundle and resources.pak.
  static void InitializeResourceBundle();

  scoped_ptr<ShellContentClient> content_client_;
  scoped_ptr<ShellContentBrowserClient> browser_client_;
  scoped_ptr<ShellContentRendererClient> renderer_client_;

  DISALLOW_COPY_AND_ASSIGN(ShellMainDelegate);
};

}  // namespace apps

#endif  // APPS_SHELL_APP_SHELL_MAIN_DELEGATE_H_
