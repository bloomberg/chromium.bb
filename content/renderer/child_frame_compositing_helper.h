// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_CHILD_FRAME_COMPOSITING_HELPER_H_
#define CONTENT_RENDERER_CHILD_FRAME_COMPOSITING_HELPER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory.h"
#include "base/memory/weak_ptr.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surface_reference_factory.h"
#include "content/common/content_export.h"
#include "ui/gfx/geometry/size.h"

namespace cc {
struct SurfaceSequence;

class Layer;
class SurfaceInfo;
}

namespace blink {
class WebLayer;
class WebPluginContainer;
class WebRemoteFrame;
}

namespace gfx {
class Size;
}

namespace content {

class BrowserPlugin;
class RenderFrameProxy;

class CONTENT_EXPORT ChildFrameCompositingHelper
    : public base::RefCounted<ChildFrameCompositingHelper> {
 public:
  static ChildFrameCompositingHelper* CreateForBrowserPlugin(
      const base::WeakPtr<BrowserPlugin>& browser_plugin);
  static ChildFrameCompositingHelper* CreateForRenderFrameProxy(
      RenderFrameProxy* render_frame_proxy);

  void OnContainerDestroy();
  void OnSetSurface(const cc::SurfaceInfo& surface_info,
                    const cc::SurfaceSequence& sequence);
  void UpdateVisibility(bool);
  void ChildFrameGone();

  cc::SurfaceId surface_id() const { return surface_id_; }

 protected:
  // Friend RefCounted so that the dtor can be non-public.
  friend class base::RefCounted<ChildFrameCompositingHelper>;

 private:
  ChildFrameCompositingHelper(
      const base::WeakPtr<BrowserPlugin>& browser_plugin,
      blink::WebRemoteFrame* frame,
      RenderFrameProxy* render_frame_proxy,
      int host_routing_id);

  virtual ~ChildFrameCompositingHelper();

  blink::WebPluginContainer* GetContainer();

  void CheckSizeAndAdjustLayerProperties(const gfx::Size& new_size,
                                         float device_scale_factor,
                                         cc::Layer* layer);
  void UpdateWebLayer(std::unique_ptr<blink::WebLayer> layer);

  const int host_routing_id_;

  gfx::Size buffer_size_;

  // The lifetime of this weak pointer should be greater than the lifetime of
  // other member objects, as they may access this pointer during their
  // destruction.
  const base::WeakPtr<BrowserPlugin> browser_plugin_;
  RenderFrameProxy* const render_frame_proxy_;

  std::unique_ptr<blink::WebLayer> web_layer_;
  cc::SurfaceId surface_id_;
  blink::WebRemoteFrame* frame_;

  scoped_refptr<cc::SurfaceReferenceFactory> surface_reference_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChildFrameCompositingHelper);
};

}  // namespace content

#endif  // CONTENT_RENDERER_CHILD_FRAME_COMPOSITING_HELPER_H_
