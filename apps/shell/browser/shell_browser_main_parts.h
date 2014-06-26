// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_SHELL_BROWSER_SHELL_BROWSER_MAIN_PARTS_H_
#define APPS_SHELL_BROWSER_SHELL_BROWSER_MAIN_PARTS_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/browser_main_parts.h"
#include "content/public/common/main_function_params.h"
#include "ui/aura/window_tree_host_observer.h"

namespace content {
class ShellDevToolsDelegate;
struct MainFunctionParams;
}

namespace extensions {
class ShellExtensionsBrowserClient;
class ShellExtensionSystem;
class ShellOmahaQueryParamsDelegate;
}

namespace views {
class Widget;
}

namespace net {
class NetLog;
}

namespace apps {

class ShellBrowserContext;
class ShellBrowserMainDelegate;
class ShellDesktopController;
class ShellExtensionsClient;

#if defined(OS_CHROMEOS)
class ShellNetworkController;
#endif

// Handles initialization of AppShell.
class ShellBrowserMainParts : public content::BrowserMainParts {
 public:
  ShellBrowserMainParts(const content::MainFunctionParams& parameters,
                        ShellBrowserMainDelegate* browser_main_delegate);
  virtual ~ShellBrowserMainParts();

  ShellBrowserContext* browser_context() {
    return browser_context_.get();
  }

  extensions::ShellExtensionSystem* extension_system() {
    return extension_system_;
  }

  // BrowserMainParts overrides.
  virtual void PreEarlyInitialization() OVERRIDE;
  virtual void PreMainMessageLoopStart() OVERRIDE;
  virtual void PostMainMessageLoopStart() OVERRIDE;
  virtual int PreCreateThreads() OVERRIDE;
  virtual void PreMainMessageLoopRun() OVERRIDE;
  virtual bool MainMessageLoopRun(int* result_code) OVERRIDE;
  virtual void PostMainMessageLoopRun() OVERRIDE;
  virtual void PostDestroyThreads() OVERRIDE;

 private:
  // Creates and initializes the ExtensionSystem.
  void CreateExtensionSystem();

#if defined(OS_CHROMEOS)
  scoped_ptr<ShellNetworkController> network_controller_;
#endif
  scoped_ptr<ShellDesktopController> desktop_controller_;
  scoped_ptr<ShellBrowserContext> browser_context_;
  scoped_ptr<ShellExtensionsClient> extensions_client_;
  scoped_ptr<extensions::ShellExtensionsBrowserClient>
      extensions_browser_client_;
  scoped_ptr<net::NetLog> net_log_;
  scoped_ptr<content::ShellDevToolsDelegate> devtools_delegate_;
  scoped_ptr<extensions::ShellOmahaQueryParamsDelegate>
      omaha_query_params_delegate_;

  // Owned by the KeyedService system.
  extensions::ShellExtensionSystem* extension_system_;

  // For running app browsertests.
  const content::MainFunctionParams parameters_;

  // If true, indicates the main message loop should be run
  // in MainMessageLoopRun. If false, it has already been run.
  bool run_message_loop_;

  scoped_ptr<ShellBrowserMainDelegate> browser_main_delegate_;

  DISALLOW_COPY_AND_ASSIGN(ShellBrowserMainParts);
};

}  // namespace apps

#endif  // APPS_SHELL_BROWSER_SHELL_BROWSER_MAIN_PARTS_H_
