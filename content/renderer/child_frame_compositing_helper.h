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
#include "base/memory/shared_memory.h"
#include "base/memory/weak_ptr.h"
#include "cc/layers/surface_layer.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/common/surfaces/surface_reference_factory.h"
#include "content/common/content_export.h"
#include "ui/gfx/geometry/size.h"

namespace cc {
struct SurfaceSequence;

class Layer;
}

namespace blink {
class WebLayer;
class WebPluginContainer;
class WebRemoteFrame;
}

namespace gfx {
class Size;
}

namespace viz {
class SurfaceInfo;
}

namespace content {

class BrowserPlugin;
class RenderFrameProxy;

class CONTENT_EXPORT ChildFrameCompositingHelper {
 public:
  virtual ~ChildFrameCompositingHelper();

  static ChildFrameCompositingHelper* CreateForBrowserPlugin(
      const base::WeakPtr<BrowserPlugin>& browser_plugin);
  static ChildFrameCompositingHelper* CreateForRenderFrameProxy(
      RenderFrameProxy* render_frame_proxy);

  void OnContainerDestroy();
  void SetPrimarySurfaceInfo(const viz::SurfaceInfo& surface_info);
  void SetFallbackSurfaceId(const viz::SurfaceId& surface_id,
                            const viz::SurfaceSequence& sequence);
  void UpdateVisibility(bool);
  void ChildFrameGone();

  const viz::SurfaceId& surface_id() const { return last_primary_surface_id_; }

 protected:
  // Friend RefCounted so that the dtor can be non-public.
  friend class base::RefCounted<ChildFrameCompositingHelper>;

 private:
  ChildFrameCompositingHelper(
      const base::WeakPtr<BrowserPlugin>& browser_plugin,
      blink::WebRemoteFrame* frame,
      RenderFrameProxy* render_frame_proxy,
      int host_routing_id);

  blink::WebPluginContainer* GetContainer();

  void CheckSizeAndAdjustLayerProperties(const viz::SurfaceInfo& surface_info,
                                         cc::Layer* layer);
  void UpdateWebLayer(std::unique_ptr<blink::WebLayer> layer);

  const int host_routing_id_;

  viz::SurfaceId last_primary_surface_id_;
  gfx::Size last_surface_size_in_pixels_;

  viz::SurfaceId fallback_surface_id_;

  // The lifetime of this weak pointer should be greater than the lifetime of
  // other member objects, as they may access this pointer during their
  // destruction.
  const base::WeakPtr<BrowserPlugin> browser_plugin_;
  RenderFrameProxy* const render_frame_proxy_;

  scoped_refptr<cc::SurfaceLayer> surface_layer_;
  std::unique_ptr<blink::WebLayer> web_layer_;
  blink::WebRemoteFrame* frame_;

  // If surface references are enabled use a stub reference factory.
  // TODO(kylechar): Remove variable along with surface sequences.
  // See https://crbug.com/676384.
  bool enable_surface_references_;

  scoped_refptr<viz::SurfaceReferenceFactory> surface_reference_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChildFrameCompositingHelper);
};

}  // namespace content

#endif  // CONTENT_RENDERER_CHILD_FRAME_COMPOSITING_HELPER_H_
