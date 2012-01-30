// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_SHELL_RENDER_VIEW_HOST_OBSERVER_H_
#define CONTENT_SHELL_SHELL_RENDER_VIEW_HOST_OBSERVER_H_
#pragma once

#include <string>

#include "content/public/browser/render_view_host_observer.h"

namespace content {

class ShellRenderViewHostObserver : public RenderViewHostObserver {
 public:
  explicit ShellRenderViewHostObserver(RenderViewHost* render_view_host);
  virtual ~ShellRenderViewHostObserver();

  // RenderViewHostObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  // Message handlers.
  void OnTextDump(const std::string& dump);

  // layoutTestController handlers.
  void OnNotifyDone();
  void OnDumpAsText();
  void OnDumpChildFramesAsText();
  void OnWaitUntilDone();

  bool dump_child_frames_;

  DISALLOW_COPY_AND_ASSIGN(ShellRenderViewHostObserver);
};

}  // namespace content

#endif  // CONTENT_SHELL_SHELL_RENDER_VIEW_HOST_OBSERVER_H_
