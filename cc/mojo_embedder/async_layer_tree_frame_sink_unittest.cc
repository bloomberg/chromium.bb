// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/mojo_embedder/async_layer_tree_frame_sink.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "cc/test/fake_layer_tree_frame_sink_client.h"
#include "components/viz/client/local_surface_id_provider.h"
#include "components/viz/test/begin_frame_args_test.h"
#include "components/viz/test/begin_frame_source_test.h"
#include "components/viz/test/test_context_provider.h"
#include "components/viz/test/test_gpu_memory_buffer_manager.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/viz/public/interfaces/compositing/compositor_frame_sink.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace mojo_embedder {
namespace {

// Used to track the thread DidLoseLayerTreeFrameSink() is called on (and quit
// a RunLoop).
class ThreadTrackingLayerTreeFrameSinkClient
    : public FakeLayerTreeFrameSinkClient {
 public:
  ThreadTrackingLayerTreeFrameSinkClient(
      base::PlatformThreadId* called_thread_id,
      base::RunLoop* run_loop)
      : called_thread_id_(called_thread_id), run_loop_(run_loop) {}
  ~ThreadTrackingLayerTreeFrameSinkClient() override = default;

  // FakeLayerTreeFrameSinkClient:
  void DidLoseLayerTreeFrameSink() override {
    EXPECT_FALSE(did_lose_layer_tree_frame_sink_called());
    FakeLayerTreeFrameSinkClient::DidLoseLayerTreeFrameSink();
    *called_thread_id_ = base::PlatformThread::CurrentId();
    run_loop_->Quit();
  }

 private:
  base::PlatformThreadId* called_thread_id_;
  base::RunLoop* run_loop_;

  DISALLOW_COPY_AND_ASSIGN(ThreadTrackingLayerTreeFrameSinkClient);
};

TEST(AsyncLayerTreeFrameSinkTest,
     DidLoseLayerTreeFrameSinkCalledOnConnectionError) {
  base::Thread bg_thread("BG Thread");
  bg_thread.Start();

  scoped_refptr<viz::TestContextProvider> provider =
      viz::TestContextProvider::Create();
  viz::TestGpuMemoryBufferManager test_gpu_memory_buffer_manager;

  viz::mojom::CompositorFrameSinkPtrInfo sink_info;
  viz::mojom::CompositorFrameSinkRequest sink_request =
      mojo::MakeRequest(&sink_info);
  viz::mojom::CompositorFrameSinkClientPtr client;
  viz::mojom::CompositorFrameSinkClientRequest client_request =
      mojo::MakeRequest(&client);

  AsyncLayerTreeFrameSink::InitParams init_params;
  init_params.context_provider = std::move(provider);
  init_params.compositor_task_runner = bg_thread.task_runner();
  init_params.gpu_memory_buffer_manager = &test_gpu_memory_buffer_manager;
  init_params.pipes.compositor_frame_sink_info = std::move(sink_info);
  init_params.pipes.client_request = std::move(client_request);
  init_params.local_surface_id_provider =
      std::make_unique<viz::DefaultLocalSurfaceIdProvider>();
  init_params.enable_surface_synchronization = true;
  AsyncLayerTreeFrameSink layer_tree_frame_sink(std::move(init_params));

  base::PlatformThreadId called_thread_id = base::kInvalidThreadId;
  base::RunLoop close_run_loop;
  ThreadTrackingLayerTreeFrameSinkClient frame_sink_client(&called_thread_id,
                                                           &close_run_loop);

  auto bind_in_background =
      [](AsyncLayerTreeFrameSink* layer_tree_frame_sink,
         ThreadTrackingLayerTreeFrameSinkClient* frame_sink_client) {
        layer_tree_frame_sink->BindToClient(frame_sink_client);
      };
  bg_thread.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(bind_in_background,
                                base::Unretained(&layer_tree_frame_sink),
                                base::Unretained(&frame_sink_client)));
  // Closes the pipe, which should trigger calling DidLoseLayerTreeFrameSink()
  // (and quitting the RunLoop). There is no need to wait for BindToClient()
  // to complete as mojo::Binding error callbacks are processed asynchronously.
  sink_request = viz::mojom::CompositorFrameSinkRequest();
  close_run_loop.Run();

  EXPECT_NE(base::kInvalidThreadId, called_thread_id);
  EXPECT_EQ(called_thread_id, bg_thread.GetThreadId());

  // DetachFromClient() has to be called on the background thread.
  base::RunLoop detach_run_loop;
  auto detach_in_background = [](AsyncLayerTreeFrameSink* layer_tree_frame_sink,
                                 base::RunLoop* detach_run_loop) {
    layer_tree_frame_sink->DetachFromClient();
    detach_run_loop->Quit();
  };
  bg_thread.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(detach_in_background,
                                base::Unretained(&layer_tree_frame_sink),
                                base::Unretained(&detach_run_loop)));
  detach_run_loop.Run();
}

// If many begin frames are received at the same time we should drop all but the
// last one. See AsyncLayerTreeFrameSink::OnBeginFrame().
TEST(AsyncLayerTreeFrameSinkTest, MultipleBeginFrames) {
  scoped_refptr<viz::TestContextProvider> provider =
      viz::TestContextProvider::Create();
  viz::TestGpuMemoryBufferManager test_gpu_memory_buffer_manager;

  viz::mojom::CompositorFrameSinkPtrInfo sink_info;
  viz::mojom::CompositorFrameSinkRequest sink_request =
      mojo::MakeRequest(&sink_info);
  viz::mojom::CompositorFrameSinkClientPtr client;
  viz::mojom::CompositorFrameSinkClientRequest client_request =
      mojo::MakeRequest(&client);

  AsyncLayerTreeFrameSink::InitParams init_params;
  init_params.context_provider = std::move(provider);
  init_params.compositor_task_runner = base::ThreadTaskRunnerHandle::Get();
  init_params.gpu_memory_buffer_manager = &test_gpu_memory_buffer_manager;
  init_params.pipes.compositor_frame_sink_info = std::move(sink_info);
  init_params.pipes.client_request = std::move(client_request);
  init_params.local_surface_id_provider =
      std::make_unique<viz::DefaultLocalSurfaceIdProvider>();
  init_params.enable_surface_synchronization = true;

  AsyncLayerTreeFrameSink layer_tree_frame_sink(std::move(init_params));

  FakeLayerTreeFrameSinkClient frame_sink_client;
  layer_tree_frame_sink.BindToClient(&frame_sink_client);

  viz::MockBeginFrameObserver observer;
  frame_sink_client.begin_frame_source()->AddObserver(&observer);

  base::RunLoop run_loop;
  // This simulates mojo queuing up multiple begin frame messages back to back.
  for (int i = 0; i < 10; i++) {
    client->OnBeginFrame(viz::CreateBeginFrameArgsForTesting(
        BEGINFRAME_FROM_HERE, viz::BeginFrameArgs::kStartingSourceId,
        viz::BeginFrameArgs::kStartingFrameNumber + i,
        base::TimeTicks() + i * viz::BeginFrameArgs::DefaultInterval()));
  }
  // Only the last begin frame will be issued to the observer even though
  // multiple begin frames are sent by the CompositorFrameSink.
  const uint64_t kLastSequenceNumber =
      viz::BeginFrameArgs::kStartingFrameNumber + 9;
  const base::TimeTicks kLastFrameTime =
      base::TimeTicks() + 9 * viz::BeginFrameArgs::DefaultInterval();
  EXPECT_CALL(observer,
              OnBeginFrame(viz::CreateBeginFrameArgsForTesting(
                  BEGINFRAME_FROM_HERE, viz::BeginFrameArgs::kStartingSourceId,
                  kLastSequenceNumber, kLastFrameTime)));
  // This will run all posted tasks. The begin frames are coalesced into one
  // task which uses the BeginFrameArgs from the last received begin frames.
  run_loop.RunUntilIdle();

  testing::Mock::VerifyAndClearExpectations(&observer);
  // Remove observer to prevent DCHECK in ~ExternalBeginFrameSource.
  frame_sink_client.begin_frame_source()->RemoveObserver(&observer);
}

class MockCompositorFrameSink : public viz::mojom::CompositorFrameSink {
 public:
  explicit MockCompositorFrameSink(
      viz::mojom::CompositorFrameSinkRequest request)
      : binding_(this, std::move(request)) {}
  ~MockCompositorFrameSink() override = default;
  // viz::mojom::CompositorFrameSink implementation
  MOCK_METHOD1(SetNeedsBeginFrame, void(bool));
  MOCK_METHOD0(SetWantsAnimateOnlyBeginFrames, void(void));
  MOCK_METHOD4(SubmitCompositorFrame,
               void(const viz::LocalSurfaceId&,
                    viz::CompositorFrame,
                    base::Optional<viz::HitTestRegionList>,
                    uint64_t));
  MOCK_METHOD5(SubmitCompositorFrameSync,
               void(const viz::LocalSurfaceId&,
                    viz::CompositorFrame,
                    base::Optional<viz::HitTestRegionList>,
                    uint64_t,
                    SubmitCompositorFrameSyncCallback));
  MOCK_METHOD1(DidNotProduceFrame, void(const viz::BeginFrameAck&));
  MOCK_METHOD2(DidAllocateSharedBitmap,
               void(mojo::ScopedSharedBufferHandle,
                    const viz::SharedBitmapId&));
  MOCK_METHOD1(DidDeleteSharedBitmap, void(const viz::SharedBitmapId&));

 private:
  mojo::Binding<viz::mojom::CompositorFrameSink> binding_;
  DISALLOW_COPY_AND_ASSIGN(MockCompositorFrameSink);
};

// If SetNeedsBeginFrames(false) is called in between receiving a begin frame
// and the begin frame task running, DidNotProduceFrame message should be sent.
TEST(AsyncLayerTreeFrameSinkTest, DidNotProduceFrame) {
  scoped_refptr<viz::TestContextProvider> provider =
      viz::TestContextProvider::Create();
  viz::TestGpuMemoryBufferManager test_gpu_memory_buffer_manager;

  viz::mojom::CompositorFrameSinkPtrInfo sink_info;
  viz::mojom::CompositorFrameSinkRequest sink_request =
      mojo::MakeRequest(&sink_info);
  MockCompositorFrameSink compositor_frame_sink(std::move(sink_request));

  viz::mojom::CompositorFrameSinkClientPtr client;
  viz::mojom::CompositorFrameSinkClientRequest client_request =
      mojo::MakeRequest(&client);

  AsyncLayerTreeFrameSink::InitParams init_params;
  init_params.context_provider = std::move(provider);
  init_params.compositor_task_runner = base::ThreadTaskRunnerHandle::Get();
  init_params.gpu_memory_buffer_manager = &test_gpu_memory_buffer_manager;
  init_params.pipes.compositor_frame_sink_info = std::move(sink_info);
  init_params.pipes.client_request = std::move(client_request);
  init_params.local_surface_id_provider =
      std::make_unique<viz::DefaultLocalSurfaceIdProvider>();
  init_params.enable_surface_synchronization = true;

  AsyncLayerTreeFrameSink layer_tree_frame_sink(std::move(init_params));

  FakeLayerTreeFrameSinkClient frame_sink_client;
  layer_tree_frame_sink.BindToClient(&frame_sink_client);

  EXPECT_CALL(compositor_frame_sink,
              DidNotProduceFrame(viz::BeginFrameAck(
                  viz::BeginFrameArgs::kStartingSourceId,
                  viz::BeginFrameArgs::kStartingFrameNumber, false)));

  base::RunLoop run_loop;

  viz::MockBeginFrameObserver observer;

  // AsyncLayerTreeFrameSink uses an ExternalBeginFrameSource by default.
  auto* begin_frame_source = static_cast<viz::ExternalBeginFrameSource*>(
      frame_sink_client.begin_frame_source());

  // This calls LayerTreeFrameSink::SetNeedsBeginFrames(true).
  begin_frame_source->AddObserver(&observer);

  client->OnBeginFrame(viz::CreateBeginFrameArgsForTesting(
      BEGINFRAME_FROM_HERE, viz::BeginFrameArgs::kStartingSourceId,
      viz::BeginFrameArgs::kStartingFrameNumber, base::TimeTicks()));

  // This calls LayerTreeFrameSink::SetNeedsBeginFrames(false).
  begin_frame_source->RemoveObserver(&observer);

  run_loop.RunUntilIdle();

  EXPECT_FALSE(
      begin_frame_source->last_begin_frame_args_for_testing().IsValid());
}

}  // namespace
}  // namespace mojo_embedder
}  // namespace cc
