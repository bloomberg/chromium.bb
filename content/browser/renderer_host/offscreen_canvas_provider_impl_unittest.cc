// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/offscreen_canvas_provider_impl.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/compositor/test/no_transport_image_transport_factory.h"
#include "content/browser/renderer_host/offscreen_canvas_surface_impl.h"
#include "services/viz/public/interfaces/compositing/compositor_frame_sink.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/modules/offscreencanvas/offscreen_canvas_surface.mojom.h"
#include "ui/compositor/compositor.h"

#if !defined(OS_ANDROID)
#include "content/browser/compositor/image_transport_factory.h"
#endif

using testing::ElementsAre;
using testing::IsEmpty;

namespace content {
namespace {

constexpr uint32_t kRendererClientId = 3;
constexpr viz::FrameSinkId kFrameSinkParent(kRendererClientId, 1);
constexpr viz::FrameSinkId kFrameSinkA(kRendererClientId, 3);
constexpr viz::FrameSinkId kFrameSinkB(kRendererClientId, 4);

// Stub OffscreenCanvasSurfaceClient that stores the latest SurfaceInfo.
class StubOffscreenCanvasSurfaceClient
    : public blink::mojom::OffscreenCanvasSurfaceClient {
 public:
  StubOffscreenCanvasSurfaceClient() : binding_(this) {}
  ~StubOffscreenCanvasSurfaceClient() override {}

  blink::mojom::OffscreenCanvasSurfaceClientPtr GetInterfacePtr() {
    blink::mojom::OffscreenCanvasSurfaceClientPtr client;
    binding_.Bind(mojo::MakeRequest(&client));
    return client;
  }

  const viz::SurfaceInfo& GetLastSurfaceInfo() const {
    return last_surface_info_;
  }

 private:
  // blink::mojom::OffscreenCanvasSurfaceClient:
  void OnFirstSurfaceActivation(const viz::SurfaceInfo& surface_info) override {
    last_surface_info_ = surface_info;
  }

  mojo::Binding<blink::mojom::OffscreenCanvasSurfaceClient> binding_;
  viz::SurfaceInfo last_surface_info_;

  DISALLOW_COPY_AND_ASSIGN(StubOffscreenCanvasSurfaceClient);
};

// Stub CompositorFrameSinkClient that does nothing.
class StubCompositorFrameSinkClient
    : public viz::mojom::CompositorFrameSinkClient {
 public:
  StubCompositorFrameSinkClient() : binding_(this) {}
  ~StubCompositorFrameSinkClient() override {}

  viz::mojom::CompositorFrameSinkClientPtr GetInterfacePtr() {
    viz::mojom::CompositorFrameSinkClientPtr client;
    binding_.Bind(mojo::MakeRequest(&client));
    return client;
  }

 private:
  // viz::mojom::CompositorFrameSinkClient:
  void DidReceiveCompositorFrameAck(
      const std::vector<viz::ReturnedResource>& resources) override {}
  void OnBeginFrame(const viz::BeginFrameArgs& begin_frame_args) override {}
  void OnBeginFramePausedChanged(bool paused) override {}
  void ReclaimResources(
      const std::vector<viz::ReturnedResource>& resources) override {}

  mojo::Binding<viz::mojom::CompositorFrameSinkClient> binding_;

  DISALLOW_COPY_AND_ASSIGN(StubCompositorFrameSinkClient);
};

// Create a CompositorFrame suitable to send over IPC.
viz::CompositorFrame MakeCompositorFrame() {
  viz::CompositorFrame frame;
  frame.metadata.begin_frame_ack.source_id =
      viz::BeginFrameArgs::kManualSourceId;
  frame.metadata.begin_frame_ack.sequence_number =
      viz::BeginFrameArgs::kStartingFrameNumber;
  frame.metadata.device_scale_factor = 1.0f;

  auto render_pass = viz::RenderPass::Create();
  render_pass->id = 1;
  render_pass->output_rect = gfx::Rect(100, 100);
  frame.render_pass_list.push_back(std::move(render_pass));

  return frame;
}

// Creates a closure that sets |error_variable| true when run.
base::Closure ConnectionErrorClosure(bool* error_variable) {
  return base::Bind([](bool* error_variable) { *error_variable = true; },
                    error_variable);
}

}  // namespace

class OffscreenCanvasProviderImplTest : public testing::Test {
 public:
  OffscreenCanvasProviderImpl* provider() { return provider_.get(); }

  // Gets the OffscreenCanvasSurfaceImpl for |frame_sink_id| or null if it
  // it doesn't exist.
  OffscreenCanvasSurfaceImpl* GetOffscreenCanvasSurface(
      const viz::FrameSinkId& frame_sink_id) {
    auto iter = provider_->canvas_map_.find(frame_sink_id);
    if (iter == provider_->canvas_map_.end())
      return nullptr;
    return iter->second.get();
  }

  // Gets list of FrameSinkId for all offscreen canvases.
  std::vector<viz::FrameSinkId> GetAllCanvases() {
    std::vector<viz::FrameSinkId> frame_sink_ids;
    for (auto& map_entry : provider_->canvas_map_)
      frame_sink_ids.push_back(map_entry.second->frame_sink_id());
    std::sort(frame_sink_ids.begin(), frame_sink_ids.end());
    return frame_sink_ids;
  }

  void DeleteOffscreenCanvasProviderImpl() { provider_.reset(); }

  void RunUntilIdle() { base::RunLoop().RunUntilIdle(); }

 protected:
  void SetUp() override {
#if !defined(OS_ANDROID)
    ImageTransportFactory::SetFactory(
        std::make_unique<NoTransportImageTransportFactory>());
#endif
    host_frame_sink_manager_ = base::MakeUnique<viz::HostFrameSinkManager>();

    // The FrameSinkManagerImpl implementation is in-process here for tests.
    frame_sink_manager_ = base::MakeUnique<viz::FrameSinkManagerImpl>();
    surface_utils::ConnectWithLocalFrameSinkManager(
        host_frame_sink_manager_.get(), frame_sink_manager_.get());

    provider_ = base::MakeUnique<OffscreenCanvasProviderImpl>(
        host_frame_sink_manager_.get(), kRendererClientId);
  }
  void TearDown() override {
    provider_.reset();
    frame_sink_manager_.reset();
    host_frame_sink_manager_.reset();
#if !defined(OS_ANDROID)
    ImageTransportFactory::Terminate();
#endif
  }

 private:
  // A MessageLoop is required for mojo bindings which are used to
  // connect to graphics services.
  base::MessageLoop message_loop_;
  std::unique_ptr<viz::HostFrameSinkManager> host_frame_sink_manager_;
  std::unique_ptr<viz::FrameSinkManagerImpl> frame_sink_manager_;
  std::unique_ptr<OffscreenCanvasProviderImpl> provider_;
};

// Mimics the workflow of OffscreenCanvas.commit() on renderer process.
TEST_F(OffscreenCanvasProviderImplTest,
       SingleHTMLCanvasElementTransferToOffscreen) {
  // Mimic connection from the renderer main thread to browser.
  StubOffscreenCanvasSurfaceClient surface_client;
  blink::mojom::OffscreenCanvasSurfacePtr surface;
  provider()->CreateOffscreenCanvasSurface(kFrameSinkParent, kFrameSinkA,
                                           surface_client.GetInterfacePtr(),
                                           mojo::MakeRequest(&surface));

  OffscreenCanvasSurfaceImpl* surface_impl =
      GetOffscreenCanvasSurface(kFrameSinkA);

  // There should be a single OffscreenCanvasSurfaceImpl and it should have the
  // provided FrameSinkId and parent FrameSinkId.
  EXPECT_EQ(kFrameSinkA, surface_impl->frame_sink_id());
  EXPECT_EQ(kFrameSinkParent, surface_impl->parent_frame_sink_id());
  EXPECT_THAT(GetAllCanvases(), ElementsAre(kFrameSinkA));

  // Mimic connection from the renderer main or worker thread to browser.
  viz::mojom::CompositorFrameSinkPtr compositor_frame_sink;
  StubCompositorFrameSinkClient compositor_frame_sink_client;
  provider()->CreateCompositorFrameSink(
      kFrameSinkA, compositor_frame_sink_client.GetInterfacePtr(),
      mojo::MakeRequest(&compositor_frame_sink));

  // Renderer submits a CompositorFrame with |local_id|.
  const viz::LocalSurfaceId local_id(1, base::UnguessableToken::Create());
  compositor_frame_sink->SubmitCompositorFrame(
      local_id, MakeCompositorFrame(), nullptr,
      base::TimeTicks::Now().since_origin().InMicroseconds());

  RunUntilIdle();

  // OffscreenCanvasSurfaceImpl in browser should have LocalSurfaceId that was
  // submitted with the CompositorFrame.
  EXPECT_EQ(local_id, surface_impl->local_surface_id());

  // OffscreenCanvasSurfaceClient in the renderer should get the new SurfaceId
  // including the |local_id|.
  const auto& surface_info = surface_client.GetLastSurfaceInfo();
  EXPECT_EQ(kFrameSinkA, surface_info.id().frame_sink_id());
  EXPECT_EQ(local_id, surface_info.id().local_surface_id());
}

// Check that renderer closing the mojom::OffscreenCanvasSurface connection
// destroys the OffscreenCanvasSurfaceImpl in browser.
TEST_F(OffscreenCanvasProviderImplTest, ClientClosesConnection) {
  StubOffscreenCanvasSurfaceClient surface_client;
  blink::mojom::OffscreenCanvasSurfacePtr surface;
  provider()->CreateOffscreenCanvasSurface(kFrameSinkParent, kFrameSinkA,
                                           surface_client.GetInterfacePtr(),
                                           mojo::MakeRequest(&surface));

  RunUntilIdle();

  EXPECT_THAT(GetAllCanvases(), ElementsAre(kFrameSinkA));

  // Mimic closing the connection from the renderer.
  surface.reset();

  RunUntilIdle();

  // The renderer closing the connection should destroy the
  // OffscreenCanvasSurfaceImpl.
  EXPECT_THAT(GetAllCanvases(), IsEmpty());
}

// Check that destroying OffscreenCanvasProviderImpl closes connection to
// renderer.
TEST_F(OffscreenCanvasProviderImplTest, ProviderClosesConnections) {
  StubOffscreenCanvasSurfaceClient surface_client;
  blink::mojom::OffscreenCanvasSurfacePtr surface;
  provider()->CreateOffscreenCanvasSurface(kFrameSinkParent, kFrameSinkA,
                                           surface_client.GetInterfacePtr(),
                                           mojo::MakeRequest(&surface));

  // Observe connection errors on |surface|.
  bool connection_error = false;
  surface.set_connection_error_handler(
      ConnectionErrorClosure(&connection_error));

  RunUntilIdle();

  // There should be a OffscreenCanvasSurfaceImpl and |surface| should be bound.
  EXPECT_THAT(GetAllCanvases(), ElementsAre(kFrameSinkA));
  EXPECT_TRUE(surface.is_bound());
  EXPECT_FALSE(connection_error);

  // Delete OffscreenCanvasProviderImpl before client disconnects.
  DeleteOffscreenCanvasProviderImpl();

  RunUntilIdle();

  // This should destroy the OffscreenCanvasSurfaceImpl and close the connection
  // to |surface| triggering a connection error.
  EXPECT_TRUE(connection_error);
}

// Check that connecting CompositorFrameSink without first making a
// OffscreenCanvasSurface connection fails.
TEST_F(OffscreenCanvasProviderImplTest, ClientConnectionWrongOrder) {
  // Mimic connection from the renderer main or worker thread.
  viz::mojom::CompositorFrameSinkPtr compositor_frame_sink;
  StubCompositorFrameSinkClient compositor_frame_sink_client;
  // Try to connect CompositorFrameSink without first making
  // OffscreenCanvasSurface connection. This should fail.
  provider()->CreateCompositorFrameSink(
      kFrameSinkA, compositor_frame_sink_client.GetInterfacePtr(),
      mojo::MakeRequest(&compositor_frame_sink));

  // Observe connection errors on |compositor_frame_sink|.
  bool connection_error = false;
  compositor_frame_sink.set_connection_error_handler(
      ConnectionErrorClosure(&connection_error));

  RunUntilIdle();

  // The connection for |compositor_frame_sink| will have failed and triggered a
  // connection error.
  EXPECT_TRUE(connection_error);
}

// Check that trying to create an OffscreenCanvasSurfaceImpl with a client id
// that doesn't match the renderer fails.
TEST_F(OffscreenCanvasProviderImplTest, InvalidClientId) {
  const viz::FrameSinkId invalid_frame_sink_id(4, 3);
  EXPECT_NE(kRendererClientId, invalid_frame_sink_id.client_id());

  StubOffscreenCanvasSurfaceClient surface_client;
  blink::mojom::OffscreenCanvasSurfacePtr surface;
  provider()->CreateOffscreenCanvasSurface(
      kFrameSinkParent, invalid_frame_sink_id, surface_client.GetInterfacePtr(),
      mojo::MakeRequest(&surface));

  // Observe connection errors on |surface|.
  bool connection_error = false;
  surface.set_connection_error_handler(
      ConnectionErrorClosure(&connection_error));

  RunUntilIdle();

  // No OffscreenCanvasSurfaceImpl should have been created.
  EXPECT_THAT(GetAllCanvases(), IsEmpty());

  // The connection for |surface| will have failed and triggered a connection
  // error.
  EXPECT_TRUE(connection_error);
}

// Mimic renderer with two offscreen canvases.
TEST_F(OffscreenCanvasProviderImplTest,
       MultiHTMLCanvasElementTransferToOffscreen) {
  StubOffscreenCanvasSurfaceClient surface_client_a;
  blink::mojom::OffscreenCanvasSurfacePtr surface_a;
  provider()->CreateOffscreenCanvasSurface(kFrameSinkParent, kFrameSinkA,
                                           surface_client_a.GetInterfacePtr(),
                                           mojo::MakeRequest(&surface_a));

  StubOffscreenCanvasSurfaceClient surface_client_b;
  blink::mojom::OffscreenCanvasSurfacePtr surface_b;
  provider()->CreateOffscreenCanvasSurface(kFrameSinkParent, kFrameSinkB,
                                           surface_client_b.GetInterfacePtr(),
                                           mojo::MakeRequest(&surface_b));

  RunUntilIdle();

  // There should be two OffscreenCanvasSurfaceImpls created.
  EXPECT_THAT(GetAllCanvases(), ElementsAre(kFrameSinkA, kFrameSinkB));

  // Mimic closing first connection from the renderer.
  surface_a.reset();

  RunUntilIdle();

  EXPECT_THAT(GetAllCanvases(), ElementsAre(kFrameSinkB));

  // Mimic closing second connection from the renderer.
  surface_b.reset();

  RunUntilIdle();

  EXPECT_THAT(GetAllCanvases(), IsEmpty());
}

}  // namespace content
