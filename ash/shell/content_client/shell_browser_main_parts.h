// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_CONTENT_CLIENT_EXAMPLES_BROWSER_MAIN_PARTS_H_
#define ASH_SHELL_CONTENT_CLIENT_EXAMPLES_BROWSER_MAIN_PARTS_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/browser_main_parts.h"

namespace base {
class Thread;
}

namespace content {
class ShellBrowserContext;
struct MainFunctionParams;
}

namespace net {
class NetLog;
}

namespace wm {
class WMState;
}

namespace ash {
namespace shell {

class ShellDelegateImpl;
class WindowWatcher;

class ShellBrowserMainParts : public content::BrowserMainParts {
 public:
  explicit ShellBrowserMainParts(
      const content::MainFunctionParams& parameters);
  virtual ~ShellBrowserMainParts();

  // Overridden from content::BrowserMainParts:
  virtual void PreMainMessageLoopStart() OVERRIDE;
  virtual void PostMainMessageLoopStart() OVERRIDE;
  virtual void ToolkitInitialized() OVERRIDE;
  virtual void PreMainMessageLoopRun() OVERRIDE;
  virtual bool MainMessageLoopRun(int* result_code) OVERRIDE;
  virtual void PostMainMessageLoopRun() OVERRIDE;

  content::ShellBrowserContext* browser_context() {
    return browser_context_.get();
  }

 private:
  scoped_ptr<net::NetLog> net_log_;
  scoped_ptr<content::ShellBrowserContext> browser_context_;
  scoped_ptr<ash::shell::WindowWatcher> window_watcher_;
  ShellDelegateImpl* delegate_;  // owned by Shell
  scoped_ptr<wm::WMState> wm_state_;

  DISALLOW_COPY_AND_ASSIGN(ShellBrowserMainParts);
};

}  // namespace shell
}  // namespace ash

#endif  // ASH_SHELL_CONTENT_CLIENT_EXAMPLES_BROWSER_MAIN_PARTS_H_
