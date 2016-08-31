// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_COMPOSITOR_BLIMP_COMPOSITOR_H_
#define BLIMP_CLIENT_CORE_COMPOSITOR_BLIMP_COMPOSITOR_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "blimp/client/core/compositor/blimp_output_surface.h"
#include "blimp/client/core/input/blimp_input_manager.h"
#include "cc/surfaces/surface_factory_client.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_settings.h"
#include "cc/trees/remote_proto_channel.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"

namespace base {
class SingleThreadTaskRunner;
class Thread;
}  // namespace base

namespace cc {
namespace proto {
class CompositorMessage;
class InitializeImpl;
}  // namespace proto

class ContextProvider;
class Layer;
class LayerTreeHost;
class LayerTreeSettings;
class Surface;
class SurfaceFactory;
class SurfaceId;
class SurfaceIdAllocator;
}  // namespace cc

namespace blimp {
class BlimpMessage;

namespace client {

class BlimpCompositorDependencies;
class CompositorDependencies;

// The BlimpCompositorClient provides the BlimpCompositor with the necessary
// dependencies for cc::LayerTreeHost owned by this compositor and for
// communicating the compositor and input messages to the corresponding
// render widget of this compositor on the engine.
class BlimpCompositorClient {
 public:
  // Should send web gesture events which could not be handled locally by the
  // compositor to the engine.
  virtual void SendWebGestureEvent(
      int render_widget_id,
      const blink::WebGestureEvent& gesture_event) = 0;

  // Should send the compositor messages from the remote client LayerTreeHost of
  // this compositor to the corresponding remote server LayerTreeHost.
  virtual void SendCompositorMessage(
      int render_widget_id,
      const cc::proto::CompositorMessage& message) = 0;

 protected:
  virtual ~BlimpCompositorClient() {}
};

// BlimpCompositor provides the basic framework and setup to host a
// LayerTreeHost. This class owns the remote client cc::LayerTreeHost, which
// performs the compositing work for the remote server LayerTreeHost. The server
// LayerTreeHost for a BlimpCompositor is owned by the
// content::RenderWidgetCompositor. Thus, each BlimpCompositor is tied to a
// RenderWidget, identified by a custom |render_widget_id| generated on the
// engine. The lifetime of this compositor is controlled by its corresponding
// RenderWidget.
// This class should only be accessed from the main thread.
class BlimpCompositor : public cc::LayerTreeHostClient,
                        public cc::RemoteProtoChannel,
                        public BlimpInputManagerClient,
                        public BlimpOutputSurfaceClient,
                        public cc::SurfaceFactoryClient {
 public:
  BlimpCompositor(const int render_widget_id,
                  BlimpCompositorDependencies* compositor_dependencies,
                  BlimpCompositorClient* client);

  ~BlimpCompositor() override;

  virtual void SetVisible(bool visible);

  // Forwards the touch event to the |input_manager_|.
  // virtual for testing.
  virtual bool OnTouchEvent(const ui::MotionEvent& motion_event);

  // Called to forward the compositor message from the remote server
  // LayerTreeHost of the render widget for this compositor.
  // virtual for testing.
  virtual void OnCompositorMessageReceived(
      std::unique_ptr<cc::proto::CompositorMessage> message);

  // Called when the a ContextProvider has been created by the
  // CompositorDependencies class.  If |host_| is waiting on an OutputSurface
  // this will build one for it.
  void OnContextProviderCreated(
      const scoped_refptr<cc::ContextProvider>& provider);

  scoped_refptr<cc::Layer> layer() const { return layer_; }

  int render_widget_id() const { return render_widget_id_; }

 private:
  friend class BlimpCompositorForTesting;

  // LayerTreeHostClient implementation.
  void WillBeginMainFrame() override {}
  void DidBeginMainFrame() override {}
  void BeginMainFrame(const cc::BeginFrameArgs& args) override {}
  void BeginMainFrameNotExpectedSoon() override {}
  void UpdateLayerTreeHost() override {}
  void ApplyViewportDeltas(const gfx::Vector2dF& inner_delta,
                           const gfx::Vector2dF& outer_delta,
                           const gfx::Vector2dF& elastic_overscroll_delta,
                           float page_scale,
                           float top_controls_delta) override {}
  void RequestNewOutputSurface() override;
  void DidInitializeOutputSurface() override;
  // TODO(khushalsagar): Need to handle context initialization failures.
  void DidFailToInitializeOutputSurface() override {}
  void WillCommit() override {}
  void DidCommit() override {}
  void DidCommitAndDrawFrame() override;
  void DidCompleteSwapBuffers() override {}
  void DidCompletePageScaleAnimation() override {}

  // RemoteProtoChannel implementation.
  void SetProtoReceiver(ProtoReceiver* receiver) override;
  void SendCompositorProto(const cc::proto::CompositorMessage& proto) override;

  // BlimpInputManagerClient implementation.
  void SendWebGestureEvent(
      const blink::WebGestureEvent& gesture_event) override;

  // BlimpOutputSurfaceClient implementation.
  void BindToOutputSurface(
      base::WeakPtr<BlimpOutputSurface> output_surface) override;
  void SwapCompositorFrame(cc::CompositorFrame frame) override;
  void UnbindOutputSurface() override;

  // SurfaceFactoryClient implementation.
  void ReturnResources(const cc::ReturnedResourceArray& resources) override;
  void SetBeginFrameSource(cc::BeginFrameSource* begin_frame_source) override {}

  // Helper method to get the embedder dependencies.
  CompositorDependencies* GetEmbedderDeps();

  // TODO(khushalsagar): Move all of this to the |DocumentView| or another
  // platform specific class. So we use the DelegatedFrameHostAndroid like the
  // RenderWidgetHostViewAndroid.
  void DestroyDelegatedContent();

  // Helper method to build the internal CC LayerTreeHost instance from
  // |message|.
  void CreateLayerTreeHost(const cc::proto::InitializeImpl& initialize_message);

  // Helper method to destroy the internal CC LayerTreeHost instance and all its
  // associated state.
  void DestroyLayerTreeHost();

  // The unique identifier for the render widget for this compositor.
  const int render_widget_id_;

  BlimpCompositorClient* client_;

  BlimpCompositorDependencies* compositor_dependencies_;

  std::unique_ptr<cc::LayerTreeHost> host_;

  // Whether or not |host_| should be visible.  This is stored in case |host_|
  // is null when SetVisible() is called.
  bool host_should_be_visible_;

  // The SurfaceFactory is bound to the lifetime of the BlimpOutputSurface. When
  // detached, the surface factory will be destroyed.
  std::unique_ptr<cc::SurfaceFactory> surface_factory_;
  base::WeakPtr<BlimpOutputSurface> output_surface_;

  // Whether or not |host_| has asked for an output surface.
  bool output_surface_request_pending_;

  // Data for the current frame.
  cc::SurfaceId surface_id_;
  gfx::Size current_surface_size_;

  base::ThreadChecker thread_checker_;

  // Surfaces related stuff and layer which holds the delegated content from the
  // compositor.
  std::unique_ptr<cc::SurfaceIdAllocator> surface_id_allocator_;
  scoped_refptr<cc::Layer> layer_;

  // To be notified of any incoming compositor protos that are specifically sent
  // to |render_widget_id_|.
  cc::RemoteProtoChannel::ProtoReceiver* remote_proto_channel_receiver_;

  // Handles input events for the current render widget. The lifetime of the
  // input manager is tied to the lifetime of the |host_| which owns the
  // cc::InputHandler. The input events are forwarded to this input handler by
  // the manager to be handled by the client compositor for the current render
  // widget.
  std::unique_ptr<BlimpInputManager> input_manager_;

  base::WeakPtrFactory<BlimpCompositor> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BlimpCompositor);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_COMPOSITOR_BLIMP_COMPOSITOR_H_
