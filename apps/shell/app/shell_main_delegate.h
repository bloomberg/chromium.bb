// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_SHELL_APP_SHELL_MAIN_DELEGATE_H_
#define APPS_SHELL_APP_SHELL_MAIN_DELEGATE_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/app/content_main_delegate.h"

namespace content {
class BrowserContext;
class ContentBrowserClient;
class ContentClient;
class ContentRendererClient;
}

namespace apps {
class ShellBrowserMainDelegate;
class ShellRendererMainDelegate;

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

 protected:
  // The created object is owned by this object.
  virtual content::ContentBrowserClient* CreateShellContentBrowserClient();

  // The returned object is owned by ShellContentRendererClient.
  virtual scoped_ptr<ShellRendererMainDelegate>
      CreateShellRendererMainDelegate();

  // Initializes the resource bundle and resources.pak.
  virtual void InitializeResourceBundle();

 private:
  // |process_type| is zygote, renderer, utility, etc. Returns true if the
  // process needs data from resources.pak.
  static bool ProcessNeedsResourceBundle(const std::string& process_type);

  scoped_ptr<content::ContentClient> content_client_;
  scoped_ptr<content::ContentBrowserClient> browser_client_;
  scoped_ptr<content::ContentRendererClient> renderer_client_;

  DISALLOW_COPY_AND_ASSIGN(ShellMainDelegate);
};

}  // namespace apps

#endif  // APPS_SHELL_APP_SHELL_MAIN_DELEGATE_H_
