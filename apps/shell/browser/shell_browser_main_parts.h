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
class ShellBrowserContext;
class ShellDevToolsDelegate;
struct MainFunctionParams;
}

namespace extensions {
class ShellExtensionsBrowserClient;
class ShellExtensionSystem;
}

namespace views {
class Widget;
}

namespace net {
class NetLog;
}

namespace apps {

class ShellBrowserContext;
class ShellDesktopController;
class ShellExtensionsClient;

// Handles initialization of AppShell.
class ShellBrowserMainParts : public content::BrowserMainParts,
                              public aura::WindowTreeHostObserver {
 public:
  explicit ShellBrowserMainParts(
      const content::MainFunctionParams& parameters);
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

  // aura::WindowTreeHostObserver overrides:
  virtual void OnHostCloseRequested(const aura::WindowTreeHost* host) OVERRIDE;

 private:
  // Creates and initializes the ExtensionSystem.
  void CreateExtensionSystem();

  scoped_ptr<ShellDesktopController> desktop_controller_;
  scoped_ptr<ShellBrowserContext> browser_context_;
  scoped_ptr<ShellExtensionsClient> extensions_client_;
  scoped_ptr<extensions::ShellExtensionsBrowserClient>
      extensions_browser_client_;
  scoped_ptr<net::NetLog> net_log_;

  scoped_ptr<content::ShellDevToolsDelegate> devtools_delegate_;

  // Owned by the KeyedService system.
  extensions::ShellExtensionSystem* extension_system_;

  // For running app browsertests.
  const content::MainFunctionParams parameters_;

  // If true, indicates the main message loop should be run
  // in MainMessageLoopRun. If false, it has already been run.
  bool run_message_loop_;

  DISALLOW_COPY_AND_ASSIGN(ShellBrowserMainParts);
};

}  // namespace apps

#endif  // APPS_SHELL_BROWSER_SHELL_BROWSER_MAIN_PARTS_H_
