// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_SURFACES_INSTANCE_H_
#define ANDROID_WEBVIEW_BROWSER_SURFACES_INSTANCE_H_

#include <memory>
#include <vector>

#include "base/memory/ref_counted.h"
#include "cc/surfaces/display_client.h"
#include "cc/surfaces/frame_sink_id.h"
#include "cc/surfaces/surface_factory_client.h"
#include "cc/surfaces/surface_id.h"

namespace cc {
class Display;
class SurfaceIdAllocator;
class SurfaceFactory;
class SurfaceManager;
}

namespace gfx {
class Rect;
class Size;
class Transform;
}

namespace android_webview {

class ParentOutputSurface;

class SurfacesInstance : public base::RefCounted<SurfacesInstance>,
                         public cc::DisplayClient,
                         public cc::SurfaceFactoryClient {
 public:
  static scoped_refptr<SurfacesInstance> GetOrCreateInstance();

  cc::FrameSinkId AllocateFrameSinkId();
  cc::SurfaceManager* GetSurfaceManager();

  void DrawAndSwap(const gfx::Size& viewport,
                   const gfx::Rect& clip,
                   const gfx::Transform& transform,
                   const gfx::Size& frame_size,
                   const cc::SurfaceId& child_id);

  void AddChildId(const cc::SurfaceId& child_id);
  void RemoveChildId(const cc::SurfaceId& child_id);

 private:
  friend class base::RefCounted<SurfacesInstance>;

  SurfacesInstance();
  ~SurfacesInstance() override;

  // cc::DisplayClient overrides.
  void DisplayOutputSurfaceLost() override;
  void DisplayWillDrawAndSwap(
      bool will_draw_and_swap,
      const cc::RenderPassList& render_passes) override {}
  void DisplayDidDrawAndSwap() override {}

  // cc::SurfaceFactoryClient implementation.
  void ReturnResources(const cc::ReturnedResourceArray& resources) override;
  void SetBeginFrameSource(cc::BeginFrameSource* begin_frame_source) override;

  void SetEmptyRootFrame();

  uint32_t next_client_id_;

  cc::FrameSinkId frame_sink_id_;

  std::unique_ptr<cc::SurfaceManager> surface_manager_;
  std::unique_ptr<cc::Display> display_;
  std::unique_ptr<cc::SurfaceIdAllocator> surface_id_allocator_;
  std::unique_ptr<cc::SurfaceFactory> surface_factory_;

  cc::LocalFrameId root_id_;
  std::vector<cc::SurfaceId> child_ids_;

  // This is owned by |display_|.
  ParentOutputSurface* output_surface_;

  DISALLOW_COPY_AND_ASSIGN(SurfacesInstance);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_SURFACES_INSTANCE_H_
