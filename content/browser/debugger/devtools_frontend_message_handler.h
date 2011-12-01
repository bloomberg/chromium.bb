// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEBUGGER_DEVTOOLS_FRONTEND_MESSAGE_HANDLER_H_
#define CONTENT_BROWSER_DEBUGGER_DEVTOOLS_FRONTEND_MESSAGE_HANDLER_H_
#pragma once

#include "base/basictypes.h"
#include "content/public/browser/render_view_host_observer.h"

class TabContents;

namespace content {

class DevToolsFrontendWindowDelegate;

// This class handles messages from DevToolsClient and calls corresponding
// methods on DevToolsFrontendWindowDelegate which is implemented by the
// embedder. This allows us to avoid exposing DevTools client messages through
// the content public API.
class DevToolsFrontendMessageHandler : public RenderViewHostObserver {
 public:
  DevToolsFrontendMessageHandler(TabContents* tab_contents,
                                 DevToolsFrontendWindowDelegate* delegate);

 private:
  virtual ~DevToolsFrontendMessageHandler();

  // content::RenderViewHostObserver overrides.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  void ForwardToDevToolsAgent(const IPC::Message& message);
  void OnActivateWindow();
  void OnCloseWindow();
  void OnMoveWindow(int x, int y);
  void OnRequestDockWindow();
  void OnRequestUndockWindow();
  void OnSaveAs(const std::string& file_name,
                const std::string& content);

  TabContents* tab_contents_;
  DevToolsFrontendWindowDelegate* delegate_;
  DISALLOW_COPY_AND_ASSIGN(DevToolsFrontendMessageHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEBUGGER_DEVTOOLS_FRONTEND_MESSAGE_HANDLER_H_
