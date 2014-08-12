// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DEVTOOLS_CLIENT_HOST_H_
#define CONTENT_PUBLIC_BROWSER_DEVTOOLS_CLIENT_HOST_H_

#include <string>

#include "base/basictypes.h"
#include "content/common/content_export.h"

namespace IPC {
class Message;
}

namespace content {

class RenderViewHost;
class WebContents;

class DevToolsFrontendHostDelegate;

// Describes interface for managing devtools clients from browser process. There
// are currently two types of clients: devtools windows and TCP socket
// debuggers.
class CONTENT_EXPORT DevToolsClientHost {
 public:
  virtual ~DevToolsClientHost() {}

  // Dispatches given message on the front-end.
  virtual void DispatchOnInspectorFrontend(const std::string& message) = 0;

  // This method is called when the contents inspected by this devtools client
  // is closing.
  virtual void InspectedContentsClosing() = 0;

  // Called to notify client that it has been kicked out by some other client
  // with greater priority.
  virtual void ReplacedWithAnotherClient() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DEVTOOLS_CLIENT_HOST_H_
