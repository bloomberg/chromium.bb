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
#include "components/viz/host/host_frame_sink_manager.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "components/viz/test/fake_host_frame_sink_client.h"
#include "components/viz/test/mock_compositor_frame_sink_client.h"
#include "content/browser/compositor/surface_utils.h"
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

// Creates a closure that sets |error_variable| true when run.
base::OnceClosure ConnectionErrorClosure(bool* error_variable) {
  DCHECK(error_variable);
  return base::BindOnce([](bool* error_variable) { *error_variable = true; },
                        error_variable);
}

// Stub OffscreenCanvasSurfaceClient that stores the latest SurfaceInfo.
class StubOffscreenCanvasSurfaceClient
    : public blink::mojom::OffscreenCanvasSurfaceClient {
 public:
  StubOffscreenCanvasSurfaceClient() : binding_(this) {}
  ~StubOffscreenCanvasSurfaceClient() override {}

  blink::mojom::OffscreenCanvasSurfaceClientPtr GetInterfacePtr() {
    blink::mojom::OffscreenCanvasSurfaceClientPtr client;
    binding_.Bind(mojo::MakeRequest(&client));
    binding_.set_connection_error_handler(
        ConnectionErrorClosure(&connection_error_));
    return client;
  }

  void Close() { binding_.Close(); }

  const viz::SurfaceInfo& last_surface_info() const {
    return last_surface_info_;
  }

  bool connection_error() const { return connection_error_; }

 private:
  // blink::mojom::OffscreenCanvasSurfaceClient:
  void OnFirstSurfaceActivation(const viz::SurfaceInfo& surface_info) override {
    last_surface_info_ = surface_info;
  }

  mojo::Binding<blink::mojom::OffscreenCanvasSurfaceClient> binding_;
  viz::SurfaceInfo last_surface_info_;
  bool connection_error_ = false;

  DISALLOW_COPY_AND_ASSIGN(StubOffscreenCanvasSurfaceClient);
};

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
    host_frame_sink_manager_ = std::make_unique<viz::HostFrameSinkManager>();

    // The FrameSinkManagerImpl implementation is in-process here for tests.
    frame_sink_manager_ = std::make_unique<viz::FrameSinkManagerImpl>();
    surface_utils::ConnectWithLocalFrameSinkManager(
        host_frame_sink_manager_.get(), frame_sink_manager_.get());

    provider_ = std::make_unique<OffscreenCanvasProviderImpl>(
        host_frame_sink_manager_.get(), kRendererClientId);

    host_frame_sink_manager_->RegisterFrameSinkId(kFrameSinkParent,
                                                  &host_frame_sink_client_);
  }
  void TearDown() override {
    host_frame_sink_manager_->InvalidateFrameSinkId(kFrameSinkParent);
    provider_.reset();
    host_frame_sink_manager_.reset();
    frame_sink_manager_.reset();
  }

 private:
  // A MessageLoop is required for mojo bindings which are used to
  // connect to graphics services.
  base::MessageLoop message_loop_;
  viz::FakeHostFrameSinkClient host_frame_sink_client_;
  std::unique_ptr<viz::HostFrameSinkManager> host_frame_sink_manager_;
  std::unique_ptr<viz::FrameSinkManagerImpl> frame_sink_manager_;
  std::unique_ptr<OffscreenCanvasProviderImpl> provider_;
};

// Mimics the workflow of OffscreenCanvas.commit() on renderer process.
TEST_F(OffscreenCanvasProviderImplTest,
       SingleHTMLCanvasElementTransferToOffscreen) {
  // Mimic connection from the renderer main thread to browser.
  StubOffscreenCanvasSurfaceClient surface_client;
  provider()->CreateOffscreenCanvasSurface(kFrameSinkParent, kFrameSinkA,
                                           surface_client.GetInterfacePtr());

  OffscreenCanvasSurfaceImpl* surface_impl =
      GetOffscreenCanvasSurface(kFrameSinkA);

  // There should be a single OffscreenCanvasSurfaceImpl and it should have the
  // provided FrameSinkId.
  EXPECT_EQ(kFrameSinkA, surface_impl->frame_sink_id());
  EXPECT_THAT(GetAllCanvases(), ElementsAre(kFrameSinkA));

  // Mimic connection from the renderer main or worker thread to browser.
  viz::mojom::CompositorFrameSinkPtr compositor_frame_sink;
  viz::MockCompositorFrameSinkClient compositor_frame_sink_client;
  provider()->CreateCompositorFrameSink(
      kFrameSinkA, compositor_frame_sink_client.BindInterfacePtr(),
      mojo::MakeRequest(&compositor_frame_sink));

  // Renderer submits a CompositorFrame with |local_id|.
  const viz::LocalSurfaceId local_id(1, base::UnguessableToken::Create());
  compositor_frame_sink->SubmitCompositorFrame(
      local_id, viz::MakeDefaultCompositorFrame(), nullptr,
      base::TimeTicks::Now().since_origin().InMicroseconds());

  RunUntilIdle();

  // OffscreenCanvasSurfaceImpl in browser should have LocalSurfaceId that was
  // submitted with the CompositorFrame.
  EXPECT_EQ(local_id, surface_impl->local_surface_id());

  // OffscreenCanvasSurfaceClient in the renderer should get the new SurfaceId
  // including the |local_id|.
  const auto& surface_info = surface_client.last_surface_info();
  EXPECT_EQ(kFrameSinkA, surface_info.id().frame_sink_id());
  EXPECT_EQ(local_id, surface_info.id().local_surface_id());
}

// Check that renderer closing the mojom::OffscreenCanvasSurface connection
// destroys the OffscreenCanvasSurfaceImpl in browser.
TEST_F(OffscreenCanvasProviderImplTest, ClientClosesConnection) {
  StubOffscreenCanvasSurfaceClient surface_client;
  provider()->CreateOffscreenCanvasSurface(kFrameSinkParent, kFrameSinkA,
                                           surface_client.GetInterfacePtr());

  RunUntilIdle();

  EXPECT_THAT(GetAllCanvases(), ElementsAre(kFrameSinkA));

  // Mimic closing the connection from the renderer.
  surface_client.Close();

  RunUntilIdle();

  // The renderer closing the connection should destroy the
  // OffscreenCanvasSurfaceImpl.
  EXPECT_THAT(GetAllCanvases(), IsEmpty());
}

// Check that destroying OffscreenCanvasProviderImpl closes connection to
// renderer.
TEST_F(OffscreenCanvasProviderImplTest, ProviderClosesConnections) {
  StubOffscreenCanvasSurfaceClient surface_client;
  provider()->CreateOffscreenCanvasSurface(kFrameSinkParent, kFrameSinkA,
                                           surface_client.GetInterfacePtr());

  RunUntilIdle();

  // There should be a OffscreenCanvasSurfaceImpl and |surface_client| should be
  // bound.
  EXPECT_THAT(GetAllCanvases(), ElementsAre(kFrameSinkA));
  EXPECT_FALSE(surface_client.connection_error());

  // Delete OffscreenCanvasProviderImpl before client disconnects.
  DeleteOffscreenCanvasProviderImpl();

  RunUntilIdle();

  // This should destroy the OffscreenCanvasSurfaceImpl and close the connection
  // to |surface_client| triggering a connection error.
  EXPECT_TRUE(surface_client.connection_error());
}

// Check that connecting CompositorFrameSink without first making a
// OffscreenCanvasSurface connection fails.
TEST_F(OffscreenCanvasProviderImplTest, ClientConnectionWrongOrder) {
  // Mimic connection from the renderer main or worker thread.
  viz::mojom::CompositorFrameSinkPtr compositor_frame_sink;
  viz::MockCompositorFrameSinkClient compositor_frame_sink_client;
  // Try to connect CompositorFrameSink without first making
  // OffscreenCanvasSurface connection. This should fail.
  provider()->CreateCompositorFrameSink(
      kFrameSinkA, compositor_frame_sink_client.BindInterfacePtr(),
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
  provider()->CreateOffscreenCanvasSurface(kFrameSinkParent,
                                           invalid_frame_sink_id,
                                           surface_client.GetInterfacePtr());

  RunUntilIdle();

  // No OffscreenCanvasSurfaceImpl should have been created.
  EXPECT_THAT(GetAllCanvases(), IsEmpty());

  // The connection for |surface_client| will have failed and triggered a
  // connection error.
  EXPECT_TRUE(surface_client.connection_error());
}

// Mimic renderer with two offscreen canvases.
TEST_F(OffscreenCanvasProviderImplTest,
       MultiHTMLCanvasElementTransferToOffscreen) {
  StubOffscreenCanvasSurfaceClient surface_client_a;
  provider()->CreateOffscreenCanvasSurface(kFrameSinkParent, kFrameSinkA,
                                           surface_client_a.GetInterfacePtr());

  StubOffscreenCanvasSurfaceClient surface_client_b;
  provider()->CreateOffscreenCanvasSurface(kFrameSinkParent, kFrameSinkB,
                                           surface_client_b.GetInterfacePtr());

  RunUntilIdle();

  // There should be two OffscreenCanvasSurfaceImpls created.
  EXPECT_THAT(GetAllCanvases(), ElementsAre(kFrameSinkA, kFrameSinkB));

  // Mimic closing first connection from the renderer.
  surface_client_a.Close();

  RunUntilIdle();

  EXPECT_THAT(GetAllCanvases(), ElementsAre(kFrameSinkB));

  // Mimic closing second connection from the renderer.
  surface_client_b.Close();

  RunUntilIdle();

  EXPECT_THAT(GetAllCanvases(), IsEmpty());
}

}  // namespace content
