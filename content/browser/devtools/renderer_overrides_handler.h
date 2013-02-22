// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_RENDERER_OVERRIDES_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_RENDERER_OVERRIDES_HANDLER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/devtools/devtools_protocol.h"

namespace content {

class DevToolsAgentHost;

// Overrides Inspector commands before they are sent to the renderer.
// May override the implementation completely, ignore it, or handle
// additional browser process implementation details.
class RendererOverridesHandler : public DevToolsProtocol::Handler {
 public:
  explicit RendererOverridesHandler(DevToolsAgentHost* agent);
  virtual ~RendererOverridesHandler();

 private:
  scoped_ptr<DevToolsProtocol::Response> GrantPermissionsForSetFileInputFiles(
      DevToolsProtocol::Command* command);
  scoped_ptr<DevToolsProtocol::Response> HandleJavaScriptDialog(
      DevToolsProtocol::Command* command);

  DevToolsAgentHost* agent_;

  DISALLOW_COPY_AND_ASSIGN(RendererOverridesHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_RENDERER_OVERRIDES_HANDLER_H_
