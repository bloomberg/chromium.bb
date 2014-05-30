// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"

#include "cc/layers/delegated_frame_resource_collection.h"
#include "cc/layers/delegated_renderer_layer.h"
#include "cc/output/output_surface_client.h"
#include "cc/surfaces/surface_aggregator.h"
#include "cc/surfaces/surface_client.h"
#include "cc/surfaces/surfaces_export.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_host_single_thread_client.h"

namespace gfx {
class Size;
}

namespace cc {

class DirectRenderer;
class DisplayClient;
class LayerTreeHost;
class OutputSurface;
class ResourceProvider;
class Surface;
class SurfaceManager;

class CC_SURFACES_EXPORT Display
    : public SurfaceClient,
      public DelegatedFrameResourceCollectionClient,
      NON_EXPORTED_BASE(public LayerTreeHostClient),
      NON_EXPORTED_BASE(public LayerTreeHostSingleThreadClient) {
 public:
  Display(DisplayClient* client, SurfaceManager* manager);
  virtual ~Display();

  void Resize(const gfx::Size& new_size);
  bool Draw();

  int CurrentSurfaceID();

  // LayerTreeHostClient implementation.
  virtual void WillBeginMainFrame(int frame_id) OVERRIDE {}
  virtual void DidBeginMainFrame() OVERRIDE {}
  virtual void Animate(base::TimeTicks frame_begin_time) OVERRIDE {}
  virtual void Layout() OVERRIDE {}
  virtual void ApplyScrollAndScale(const gfx::Vector2d& scroll_delta,
                                   float page_scale) OVERRIDE {}
  virtual scoped_ptr<OutputSurface> CreateOutputSurface(bool fallback) OVERRIDE;
  virtual void DidInitializeOutputSurface() OVERRIDE {}
  virtual void WillCommit() OVERRIDE {}
  virtual void DidCommit() OVERRIDE {}
  virtual void DidCommitAndDrawFrame() OVERRIDE {}
  virtual void DidCompleteSwapBuffers() OVERRIDE {}

  // LayerTreeHostSingleThreadClient implementation.
  virtual void ScheduleComposite() OVERRIDE;
  virtual void ScheduleAnimation() OVERRIDE;
  virtual void DidPostSwapBuffers() OVERRIDE {}
  virtual void DidAbortSwapBuffers() OVERRIDE {}

  // DelegatedFrameResourceCollectionClient implementation.
  virtual void UnusedResourcesAreAvailable() OVERRIDE {}

  // SurfaceClient implementation.
  virtual void ReturnResources(const ReturnedResourceArray& resources) OVERRIDE;

 private:
  void DoComposite();

  bool scheduled_draw_;

  DisplayClient* client_;
  SurfaceManager* manager_;
  SurfaceAggregator aggregator_;
  scoped_ptr<Surface> current_surface_;
  scoped_ptr<LayerTreeHost> layer_tree_host_;
  scoped_refptr<DelegatedFrameResourceCollection> resource_collection_;
  scoped_refptr<DelegatedFrameProvider> delegated_frame_provider_;
  scoped_refptr<DelegatedRendererLayer> delegated_layer_;

  DISALLOW_COPY_AND_ASSIGN(Display);
};

}  // namespace cc
