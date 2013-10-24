// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_SHELL_BROWSER_MAIN_PARTS_H_
#define CONTENT_SHELL_BROWSER_SHELL_BROWSER_MAIN_PARTS_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/browser_main_parts.h"
#include "content/public/common/main_function_params.h"

#if defined(OS_ANDROID)
namespace breakpad {
class CrashDumpManager;
}
#endif

namespace base {
class Thread;
}

namespace net {
class NetLog;
}

namespace content {

class ShellBrowserContext;
class ShellDevToolsDelegate;
class ShellPluginServiceFilter;

class ShellBrowserMainParts : public BrowserMainParts {
 public:
  explicit ShellBrowserMainParts(const MainFunctionParams& parameters);
  virtual ~ShellBrowserMainParts();

  // BrowserMainParts overrides.
  virtual void PreEarlyInitialization() OVERRIDE;
  virtual void PreMainMessageLoopStart() OVERRIDE;
  virtual void PostMainMessageLoopStart() OVERRIDE;
  virtual void PreMainMessageLoopRun() OVERRIDE;
  virtual bool MainMessageLoopRun(int* result_code) OVERRIDE;
  virtual void PostMainMessageLoopRun() OVERRIDE;

  ShellDevToolsDelegate* devtools_delegate() {
    return devtools_delegate_.get();
  }

  ShellBrowserContext* browser_context() { return browser_context_.get(); }
  ShellBrowserContext* off_the_record_browser_context() {
    return off_the_record_browser_context_.get();
  }

  net::NetLog* net_log() { return net_log_.get(); }

 private:
#if defined(OS_ANDROID)
  scoped_ptr<breakpad::CrashDumpManager> crash_dump_manager_;
#endif
  scoped_ptr<net::NetLog> net_log_;
  scoped_ptr<ShellBrowserContext> browser_context_;
  scoped_ptr<ShellBrowserContext> off_the_record_browser_context_;

  // For running content_browsertests.
  const MainFunctionParams parameters_;
  bool run_message_loop_;

  scoped_ptr<ShellDevToolsDelegate> devtools_delegate_;
#if defined(ENABLE_PLUGINS)
  scoped_ptr<ShellPluginServiceFilter> plugin_service_filter_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ShellBrowserMainParts);
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_SHELL_BROWSER_MAIN_PARTS_H_
