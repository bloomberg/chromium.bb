// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_MESSAGE_HANDLER_H_
#pragma once

#include "content/browser/renderer_host/render_view_host_observer.h"

class Profile;
struct ExtensionHostMsg_DomMessage_Params;

// Filters and dispatches extension-related IPC messages that arrive from
// renderer/extension processes.  This object is created for renderers and also
// ExtensionHost/BackgroundContents.  Contrast this with ExtensionTabHelper,
// which is only created for TabContents.
class ExtensionMessageHandler : public RenderViewHostObserver {
 public:
  // |sender| is guaranteed to outlive this object.
  explicit ExtensionMessageHandler(RenderViewHost* render_view_host);
  virtual ~ExtensionMessageHandler();

  // RenderViewHostObserver overrides.
  virtual bool OnMessageReceived(const IPC::Message& message);

 private:
  // Message handlers.
  void OnPostMessage(int port_id, const std::string& message);
  void OnRequest(const ExtensionHostMsg_DomMessage_Params& params);

  DISALLOW_COPY_AND_ASSIGN(ExtensionMessageHandler);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_MESSAGE_HANDLER_H_
