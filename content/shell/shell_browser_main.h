// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_SHELL_BROWSER_MAIN_H_
#define CONTENT_SHELL_SHELL_BROWSER_MAIN_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "content/browser/browser_main.h"

class ResourceDispatcherHost;
class TabContents;

namespace base {
class Thread;
}

namespace ui {
class Clipboard;
}

namespace content {

class ShellBrowserContext;

class ShellBrowserMainParts : public BrowserMainParts {
 public:
  explicit ShellBrowserMainParts(const MainFunctionParams& parameters);
  virtual ~ShellBrowserMainParts();

  virtual void PreMainMessageLoopRun();

  ResourceDispatcherHost* GetResourceDispatcherHost();
  ui::Clipboard* GetClipboard();

  base::Thread* io_thread() { return io_thread_.get(); }
  base::Thread* file_thread() { return file_thread_.get(); }

 private:
  scoped_ptr<ShellBrowserContext> browser_context_;
  scoped_ptr<TabContents> tab_contents_;

  scoped_ptr<ResourceDispatcherHost> resource_dispatcher_host_;
  scoped_ptr<ui::Clipboard> clipboard_;

  scoped_ptr<base::Thread> io_thread_;
  scoped_ptr<base::Thread> file_thread_;
  scoped_ptr<base::Thread> db_thread_;
  scoped_ptr<base::Thread> process_launcher_thread_;
  scoped_ptr<base::Thread> cache_thread_;

  DISALLOW_COPY_AND_ASSIGN(ShellBrowserMainParts);
};

}  // namespace content

#endif  // CONTENT_SHELL_SHELL_BROWSER_MAIN_H_
