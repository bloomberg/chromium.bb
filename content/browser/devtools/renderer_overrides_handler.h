// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_RENDERER_OVERRIDES_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_RENDERER_OVERRIDES_HANDLER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "cc/output/compositor_frame_metadata.h"
#include "content/browser/devtools/devtools_protocol.h"
#include "content/common/content_export.h"
#include "content/public/browser/render_widget_host.h"
#include "third_party/skia/include/core/SkBitmap.h"

class SkBitmap;

namespace IPC {
class Message;
}

namespace blink {
class WebMouseEvent;
}

namespace content {

class DevToolsTracingHandler;
class RenderViewHostImpl;

// Overrides Inspector commands before they are sent to the renderer.
// May override the implementation completely, ignore it, or handle
// additional browser process implementation details.
class CONTENT_EXPORT RendererOverridesHandler
    : public DevToolsProtocol::Handler {
 public:
  RendererOverridesHandler();
  virtual ~RendererOverridesHandler();

  void OnClientDetached();
  void SetRenderViewHost(RenderViewHostImpl* host);
  void ClearRenderViewHost();

 private:
  // Page domain.
  scoped_refptr<DevToolsProtocol::Response> PageQueryUsageAndQuota(
      scoped_refptr<DevToolsProtocol::Command>);

  void PageQueryUsageAndQuotaCompleted(
     scoped_refptr<DevToolsProtocol::Command>,
     scoped_ptr<base::DictionaryValue> response_data);

  RenderViewHostImpl* host_;
  base::WeakPtrFactory<RendererOverridesHandler> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(RendererOverridesHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_RENDERER_OVERRIDES_HANDLER_H_
