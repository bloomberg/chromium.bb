// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEBUGGER_DEVTOOLS_HANDLER_H_
#define CHROME_BROWSER_DEBUGGER_DEVTOOLS_HANDLER_H_
#pragma once

#include "content/browser/renderer_host/render_view_host_observer.h"

class DevToolsHandler : public RenderViewHostObserver {
 public:
  explicit DevToolsHandler(RenderViewHost* render_view_host);
  virtual ~DevToolsHandler();

  // RenderViewHostObserver overrides.
  virtual bool OnMessageReceived(const IPC::Message& message);

 private:
  void OnForwardToAgent(const IPC::Message& message);
  void OnForwardToClient(const IPC::Message& message);
  void OnActivateWindow();
  void OnCloseWindow();
  void OnRequestDockWindow();
  void OnRequestUndockWindow();
  void OnRuntimePropertyChanged(const std::string& name,
                                const std::string& value);

  DISALLOW_COPY_AND_ASSIGN(DevToolsHandler);
};

#endif  // CHROME_BROWSER_DEBUGGER_DEVTOOLS_HANDLER_H_
