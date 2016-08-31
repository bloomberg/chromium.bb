// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_COMPOSITOR_BLIMP_COMPOSITOR_MANAGER_H_
#define BLIMP_CLIENT_CORE_COMPOSITOR_BLIMP_COMPOSITOR_MANAGER_H_

#include <map>

#include "base/macros.h"
#include "blimp/client/core/compositor/blimp_compositor.h"
#include "blimp/client/core/compositor/blob_image_serialization_processor.h"
#include "blimp/client/core/render_widget/render_widget_feature.h"
#include "cc/layers/layer.h"
#include "cc/trees/layer_tree_settings.h"

namespace blimp {
namespace client {

// The BlimpCompositorManager manages multiple BlimpCompositor instances, each
// mapped to a render widget on the engine. The compositor corresponding to
// the render widget initialized on the engine will be the |active_compositor_|.
// Only the |active_compositor_| holds the accelerated widget and builds the
// output surface from this widget to draw to the view. All events from the
// native view are forwarded to this compositor.
class BlimpCompositorManager
    : public RenderWidgetFeature::RenderWidgetFeatureDelegate,
      public BlimpCompositorClient {
 public:
  explicit BlimpCompositorManager(
      RenderWidgetFeature* render_widget_feature,
      BlimpCompositorDependencies* compositor_dependencies);
  ~BlimpCompositorManager() override;

  void SetVisible(bool visible);

  bool OnTouchEvent(const ui::MotionEvent& motion_event);

  scoped_refptr<cc::Layer> layer() const { return layer_; }

 protected:
  // virtual for testing.
  virtual std::unique_ptr<BlimpCompositor> CreateBlimpCompositor(
      int render_widget_id,
      BlimpCompositorDependencies* compositor_dependencies,
      BlimpCompositorClient* client);

  // Returns the compositor for the |render_widget_id|. Will return nullptr if
  // no compositor is found.
  // protected for testing.
  BlimpCompositor* GetCompositor(int render_widget_id);

 private:
  // RenderWidgetFeatureDelegate implementation.
  void OnRenderWidgetCreated(int render_widget_id) override;
  void OnRenderWidgetInitialized(int render_widget_id) override;
  void OnRenderWidgetDeleted(int render_widget_id) override;
  void OnCompositorMessageReceived(
      int render_widget_id,
      std::unique_ptr<cc::proto::CompositorMessage> message) override;

  // BlimpCompositorClient implementation.
  void SendWebGestureEvent(
      int render_widget_id,
      const blink::WebGestureEvent& gesture_event) override;
  void SendCompositorMessage(
      int render_widget_id,
      const cc::proto::CompositorMessage& message) override;

  // The bridge to the network layer that does the proto/RenderWidget id work.
  // BlimpCompositorManager does not own this and it is expected to outlive this
  // BlimpCompositorManager instance.
  RenderWidgetFeature* render_widget_feature_;

  bool visible_;

  // The layer which holds the content from the active compositor.
  scoped_refptr<cc::Layer> layer_;

  // A map of render_widget_ids to the BlimpCompositor instance.
  using CompositorMap = std::map<int, std::unique_ptr<BlimpCompositor>>;
  CompositorMap compositors_;

  // The |active_compositor_| represents the compositor from the CompositorMap
  // that is currently visible and has the |window_|. It corresponds to the
  // render widget currently initialized on the engine.
  BlimpCompositor* active_compositor_;

  BlimpCompositorDependencies* compositor_dependencies_;

  DISALLOW_COPY_AND_ASSIGN(BlimpCompositorManager);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_COMPOSITOR_BLIMP_COMPOSITOR_MANAGER_H_
