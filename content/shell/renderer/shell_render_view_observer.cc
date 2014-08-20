// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/shell_render_view_observer.h"

#include "base/command_line.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/render_view_observer.h"
#include "content/shell/common/shell_messages.h"
#include "content/shell/common/shell_switches.h"
#include "content/shell/renderer/ipc_echo.h"
#include "third_party/WebKit/public/web/WebTestingSupport.h"
#include "third_party/WebKit/public/web/WebView.h"

namespace content {

ShellRenderViewObserver::ShellRenderViewObserver(RenderView* render_view)
    : RenderViewObserver(render_view) {
}

ShellRenderViewObserver::~ShellRenderViewObserver() {
}

void ShellRenderViewObserver::DidClearWindowObject(
    blink::WebLocalFrame* frame) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kExposeInternalsForTesting)) {
    blink::WebTestingSupport::injectInternalsObject(frame);
  }

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kExposeIpcEcho)) {
    RenderView* view = render_view();
    if (!ipc_echo_)
      ipc_echo_.reset(new IPCEcho(view->GetWebView()->mainFrame()->document(),
                                  RenderThread::Get(), view->GetRoutingID()));
    ipc_echo_->Install(view->GetWebView()->mainFrame());
  }
}

bool ShellRenderViewObserver::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ShellRenderViewObserver, message)
    IPC_MESSAGE_HANDLER(ShellViewMsg_EchoPong, OnEchoPong)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void ShellRenderViewObserver::OnEchoPong(int id, const std::string& body) {
  ipc_echo_->DidRespondEcho(id, body.size());
}

}  // namespace content
