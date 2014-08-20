// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_SHELL_RENDER_VIEW_OBSERVER_H_
#define CONTENT_SHELL_SHELL_RENDER_VIEW_OBSERVER_H_

#include <string>
#include "base/memory/scoped_ptr.h"
#include "content/public/renderer/render_view_observer.h"

namespace blink {
class WebFrame;
}

namespace content {

class IPCEcho;
class RenderView;


class ShellRenderViewObserver : public RenderViewObserver {
 public:
  explicit ShellRenderViewObserver(RenderView* render_view);
  virtual ~ShellRenderViewObserver();

 private:
  // Message handlers.
  void OnEchoPong(int id, const std::string& body);

  // RenderViewObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void DidClearWindowObject(blink::WebLocalFrame* frame) OVERRIDE;

  scoped_ptr<IPCEcho> ipc_echo_;
  DISALLOW_COPY_AND_ASSIGN(ShellRenderViewObserver);
};

}  // namespace content

#endif  // CONTENT_SHELL_SHELL_RENDER_VIEW_OBSERVER_H_
