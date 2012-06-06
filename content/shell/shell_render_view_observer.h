// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_SHELL_RENDER_VIEW_OBSERVER_H_
#define CONTENT_SHELL_SHELL_RENDER_VIEW_OBSERVER_H_
#pragma once

#include "content/public/renderer/render_view_observer.h"

namespace content {

// This class holds the content_shell specific parts of RenderView, and has the
// same lifetime.
class ShellRenderViewObserver : public RenderViewObserver {
 public:
  explicit ShellRenderViewObserver(RenderView* render_view);
  virtual ~ShellRenderViewObserver();

  // RenderViewObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void DidFinishLoad(WebKit::WebFrame* frame) OVERRIDE;

 private:
  // Message handlers.
  void OnCaptureTextDump(bool as_text, bool printing, bool recursive);

  DISALLOW_COPY_AND_ASSIGN(ShellRenderViewObserver);
};

}  // namespace content

#endif  // CONTENT_SHELL_SHELL_RENDER_VIEW_OBSERVER_H_
