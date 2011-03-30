// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEBUGGER_DEVTOOLS_HANDLER_H_
#define CHROME_BROWSER_DEBUGGER_DEVTOOLS_HANDLER_H_
#pragma once

#include "content/browser/tab_contents/tab_contents_observer.h"

class RenderViewHost;

class DevToolsHandler {
 public:
  DevToolsHandler(TabContents* tab, RenderViewHost* render_view_host);
  virtual ~DevToolsHandler();

  // TabContentsObserver overrides.
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

  RenderViewHost* GetRenderViewHost();

  // If tab_ is null, then render_view_host_ is used instead
  TabContents* tab_;
  RenderViewHost* render_view_host_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsHandler);
};

// A wrapper around DevToolsHandler that implements TabContentsObserver.
class DevToolsTabHelper : public TabContentsObserver {
 public:
  explicit DevToolsTabHelper(TabContents* tab_contents);

  // TabContentsObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message);

 private:
  DevToolsHandler handler_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsTabHelper);
};

#endif  // CHROME_BROWSER_DEBUGGER_DEVTOOLS_HANDLER_H_
