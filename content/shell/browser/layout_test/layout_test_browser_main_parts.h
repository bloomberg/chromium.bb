// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_BROWSER_MAIN_PARTS_H_
#define CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_BROWSER_MAIN_PARTS_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "content/shell/browser/shell_browser_main_parts.h"

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

class ShellPluginServiceFilter;

class LayoutTestBrowserMainParts : public ShellBrowserMainParts {
 public:
  explicit LayoutTestBrowserMainParts(const MainFunctionParams& parameters);
  ~LayoutTestBrowserMainParts() override;

 private:
  void InitializeBrowserContexts() override;
  void InitializeMessageLoopContext() override;

#if defined(ENABLE_PLUGINS)
  scoped_ptr<ShellPluginServiceFilter> plugin_service_filter_;
#endif

  DISALLOW_COPY_AND_ASSIGN(LayoutTestBrowserMainParts);
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_BROWSER_MAIN_PARTS_H_
