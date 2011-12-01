// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DEVTOOLS_CLIENT_HOST_H_
#define CONTENT_PUBLIC_BROWSER_DEVTOOLS_CLIENT_HOST_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "content/common/content_export.h"

namespace IPC {
class Message;
}

class RenderViewHost;
class TabContents;

namespace content {

class DevToolsFrontendHostDelegate;

// Describes interface for managing devtools clients from browser process. There
// are currently two types of clients: devtools windows and TCP socket
// debuggers.
class CONTENT_EXPORT DevToolsClientHost {
 public:
  virtual ~DevToolsClientHost() {}

  // Dispatches given message on the front-end.
  virtual void DispatchOnInspectorFrontend(const std::string& message) = 0;

  // This method is called when tab inspected by this devtools client is
  // closing.
  virtual void InspectedTabClosing() = 0;

  // This method is called when tab inspected by this devtools client is
  // navigating to |url|.
  virtual void FrameNavigating(const std::string& url) = 0;

  // Invoked when a tab is replaced by another tab. This is triggered by
  // TabStripModel::ReplaceTabContentsAt.
  virtual void TabReplaced(TabContents* new_tab) = 0;

  // Creates DevToolsClientHost for TabContents containing default DevTools
  // frontend implementation.
  static DevToolsClientHost* CreateDevToolsFrontendHost(
      TabContents* client_tab_contents,
      DevToolsFrontendHostDelegate* delegate);

  // Sets up DevToolsClient on the corresponding RenderView.
  static void SetupDevToolsFrontendClient(RenderViewHost* frontend_rvh);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DEVTOOLS_CLIENT_HOST_H_
