// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEBUGGER_DEVTOOLS_FRONTEND_HOST_H_
#define CONTENT_BROWSER_DEBUGGER_DEVTOOLS_FRONTEND_HOST_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "content/public/browser/devtools_client_host.h"
#include "content/public/browser/render_view_host_observer.h"

class TabContents;

namespace content {

class DevToolsFrontendHostDelegate;

// This class handles messages from DevToolsClient and calls corresponding
// methods on DevToolsFrontendHostDelegate which is implemented by the
// embedder. This allows us to avoid exposing DevTools client messages through
// the content public API.
class DevToolsFrontendHost : public DevToolsClientHost,
                             public RenderViewHostObserver {
 public:
  DevToolsFrontendHost(TabContents* tab_contents,
                       DevToolsFrontendHostDelegate* delegate);

 private:
  virtual ~DevToolsFrontendHost();

  // DevToolsFrontendHost implementation.
  virtual void DispatchOnInspectorFrontend(const std::string& message) OVERRIDE;
  virtual void InspectedTabClosing() OVERRIDE;
  virtual void FrameNavigating(const std::string& url) OVERRIDE;
  virtual void TabReplaced(TabContents* new_tab) OVERRIDE;

  // content::RenderViewHostObserver overrides.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  void OnDispatchOnInspectorBackend(const std::string& message);
  void OnActivateWindow();
  void OnCloseWindow();
  void OnMoveWindow(int x, int y);
  void OnRequestDockWindow();
  void OnRequestUndockWindow();
  void OnSaveAs(const std::string& file_name,
                const std::string& content);

  TabContents* tab_contents_;
  DevToolsFrontendHostDelegate* delegate_;
  DISALLOW_COPY_AND_ASSIGN(DevToolsFrontendHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEBUGGER_DEVTOOLS_FRONTEND_HOST_H_
