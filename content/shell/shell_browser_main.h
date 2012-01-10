// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_SHELL_BROWSER_MAIN_H_
#define CONTENT_SHELL_SHELL_BROWSER_MAIN_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/browser_main_parts.h"

class ResourceDispatcherHost;

namespace base {
class Thread;
}

namespace ui {
class Clipboard;
}

namespace content {

class ShellBrowserContext;
struct MainFunctionParams;

class ShellBrowserMainParts : public BrowserMainParts {
 public:
  explicit ShellBrowserMainParts(const content::MainFunctionParams& parameters);
  virtual ~ShellBrowserMainParts();

  virtual void PreEarlyInitialization() OVERRIDE {}
  virtual void PostEarlyInitialization() OVERRIDE {}
  virtual void PreMainMessageLoopStart() OVERRIDE {}
  virtual void ToolkitInitialized() OVERRIDE {}
  virtual void PostMainMessageLoopStart() OVERRIDE {}
  virtual void PreCreateThreads() OVERRIDE {}
  virtual void PreStartThread(BrowserThread::ID id) OVERRIDE {}
  virtual void PostStartThread(BrowserThread::ID id) OVERRIDE {}
  virtual void PreMainMessageLoopRun() OVERRIDE;
  virtual bool MainMessageLoopRun(int* result_code) OVERRIDE;
  virtual void PostMainMessageLoopRun() OVERRIDE;
  virtual void PreStopThread(BrowserThread::ID id) OVERRIDE;
  virtual void PostStopThread(BrowserThread::ID) OVERRIDE {}
  virtual void PostDestroyThreads() OVERRIDE {}

  ResourceDispatcherHost* GetResourceDispatcherHost();
  ui::Clipboard* GetClipboard();

 private:
  scoped_ptr<ShellBrowserContext> browser_context_;

  scoped_ptr<ResourceDispatcherHost> resource_dispatcher_host_;
  scoped_ptr<ui::Clipboard> clipboard_;

  DISALLOW_COPY_AND_ASSIGN(ShellBrowserMainParts);
};

}  // namespace content

#endif  // CONTENT_SHELL_SHELL_BROWSER_MAIN_H_
