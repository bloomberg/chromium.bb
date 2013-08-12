// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_RENDERER_OVERRIDES_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_RENDERER_OVERRIDES_HANDLER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/devtools/devtools_protocol.h"

class SkBitmap;

namespace content {

class DevToolsAgentHost;
class DevToolsTracingHandler;

// Overrides Inspector commands before they are sent to the renderer.
// May override the implementation completely, ignore it, or handle
// additional browser process implementation details.
class RendererOverridesHandler : public DevToolsProtocol::Handler {
 public:
  explicit RendererOverridesHandler(DevToolsAgentHost* agent);
  virtual ~RendererOverridesHandler();

 private:
  scoped_refptr<DevToolsProtocol::Response>
      GrantPermissionsForSetFileInputFiles(
          scoped_refptr<DevToolsProtocol::Command> command);
  scoped_refptr<DevToolsProtocol::Response> PageHandleJavaScriptDialog(
      scoped_refptr<DevToolsProtocol::Command> command);
  scoped_refptr<DevToolsProtocol::Response> PageNavigate(
      scoped_refptr<DevToolsProtocol::Command> command);
  scoped_refptr<DevToolsProtocol::Response> PageCaptureScreenshot(
      scoped_refptr<DevToolsProtocol::Command> command);

  void ScreenshotCaptured(
      scoped_refptr<DevToolsProtocol::Command> command,
      const std::string& format,
      int quality,
      double scale,
      bool success,
      const SkBitmap& bitmap);

  DevToolsAgentHost* agent_;
  base::WeakPtrFactory<RendererOverridesHandler> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(RendererOverridesHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_RENDERER_OVERRIDES_HANDLER_H_
