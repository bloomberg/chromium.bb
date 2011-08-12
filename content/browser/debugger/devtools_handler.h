// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEBUGGER_DEVTOOLS_HANDLER_H_
#define CONTENT_BROWSER_DEBUGGER_DEVTOOLS_HANDLER_H_
#pragma once

#include "content/browser/renderer_host/render_view_host_observer.h"

class DevToolsClientHost;

class DevToolsHandler : public RenderViewHostObserver {
 public:
  explicit DevToolsHandler(RenderViewHost* render_view_host);
  virtual ~DevToolsHandler();

  // RenderViewHostObserver overrides.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  void OnForwardToAgent(const IPC::Message& message);
  void OnForwardToClient(const IPC::Message& message);
  void OnActivateWindow();
  void OnCloseWindow();
  void OnRequestDockWindow();
  void OnRequestUndockWindow();
  void OnSaveAs(const std::string& file_name,
                const std::string& content);
  void OnRuntimePropertyChanged(const std::string& name,
                                const std::string& value);
  void OnClearBrowserCache();
  void OnClearBrowserCookies();

  DevToolsClientHost* GetOwnerClientHost();

  DISALLOW_COPY_AND_ASSIGN(DevToolsHandler);
};

#endif  // CONTENT_BROWSER_DEBUGGER_DEVTOOLS_HANDLER_H_
