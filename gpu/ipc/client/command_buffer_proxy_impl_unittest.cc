// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/client/command_buffer_proxy_impl.h"

#include "base/memory/scoped_refptr.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "gpu/ipc/common/surface_handle.h"
#include "ipc/ipc_test_sink.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {
// GpuChannelHost is expected to be created on the IO thread, and posts tasks to
// setup its IPC listener, so it must be created after the thread task runner
// handle is set.  It expects Send to be called on any thread except IO thread,
// and posts tasks to the IO thread to ensure IPCs are sent in order, which is
// important for sync IPCs.  But we override Send, so we can't test sync IPC
// behavior with this setup.
class TestGpuChannelHost : public GpuChannelHost {
 public:
  TestGpuChannelHost(IPC::TestSink* sink)
      : GpuChannelHost(0 /* channel_id */,
                       GPUInfo(),
                       GpuFeatureInfo(),
                       mojo::ScopedMessagePipeHandle(
                           mojo::MessagePipeHandle(mojo::kInvalidHandleValue))),
        sink_(sink) {}

  bool Send(IPC::Message* msg) override { return sink_->Send(msg); }

 protected:
  ~TestGpuChannelHost() override {}

  IPC::TestSink* sink_;
};

class CommandBufferProxyImplTest : public testing::Test {
 public:
  CommandBufferProxyImplTest()
      : task_runner_(base::MakeRefCounted<base::TestSimpleTaskRunner>()),
        thread_task_runner_handle_override_(
            base::ThreadTaskRunnerHandle::OverrideForTesting(task_runner_)),
        channel_(base::MakeRefCounted<TestGpuChannelHost>(&sink_)) {}

  ~CommandBufferProxyImplTest() override {
    // Release channel, and run any cleanup tasks it posts.
    channel_ = nullptr;
    task_runner_->RunUntilIdle();
  }

  std::unique_ptr<CommandBufferProxyImpl> CreateAndInitializeProxy() {
    auto proxy = std::make_unique<CommandBufferProxyImpl>(
        channel_, nullptr /* gpu_memory_buffer_manager */, 0 /* stream_id */,
        task_runner_);
    proxy->Initialize(kNullSurfaceHandle, nullptr, SchedulingPriority::kNormal,
                      ContextCreationAttribs(), GURL());
    // Use an arbitrary valid shm_id. The command buffer doesn't use this
    // directly, but not setting it triggers DCHECKs.
    proxy->SetGetBuffer(1 /* shm_id */);
    sink_.ClearMessages();
    return proxy;
  }

  void ExpectOrderingBarrier(const GpuDeferredMessage& params,
                             int32_t route_id,
                             int32_t put_offset) {
    EXPECT_EQ(params.message.routing_id(), route_id);
    GpuCommandBufferMsg_AsyncFlush::Param async_flush;
    ASSERT_TRUE(
        GpuCommandBufferMsg_AsyncFlush::Read(&params.message, &async_flush));
    EXPECT_EQ(std::get<0>(async_flush), put_offset);
  }

 protected:
  IPC::TestSink sink_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ScopedClosureRunner thread_task_runner_handle_override_;
  scoped_refptr<TestGpuChannelHost> channel_;
};

TEST_F(CommandBufferProxyImplTest, OrderingBarriersAreCoalescedWithFlush) {
  auto proxy1 = CreateAndInitializeProxy();
  auto proxy2 = CreateAndInitializeProxy();

  proxy1->OrderingBarrier(10);
  proxy2->OrderingBarrier(20);
  proxy1->OrderingBarrier(30);
  proxy1->OrderingBarrier(40);
  proxy1->Flush(50);

  EXPECT_EQ(1u, sink_.message_count());
  const IPC::Message* msg =
      sink_.GetFirstMessageMatching(GpuChannelMsg_FlushDeferredMessages::ID);
  ASSERT_TRUE(msg);
  GpuChannelMsg_FlushDeferredMessages::Param params;
  ASSERT_TRUE(GpuChannelMsg_FlushDeferredMessages::Read(msg, &params));
  std::vector<GpuDeferredMessage> deferred_messages =
      std::get<0>(std::move(params));
  EXPECT_EQ(3u, deferred_messages.size());
  ExpectOrderingBarrier(deferred_messages[0], proxy1->route_id(), 10);
  ExpectOrderingBarrier(deferred_messages[1], proxy2->route_id(), 20);
  ExpectOrderingBarrier(deferred_messages[2], proxy1->route_id(), 50);
}

TEST_F(CommandBufferProxyImplTest, FlushPendingWorkFlushesOrderingBarriers) {
  auto proxy1 = CreateAndInitializeProxy();
  auto proxy2 = CreateAndInitializeProxy();

  proxy1->OrderingBarrier(10);
  proxy2->OrderingBarrier(20);
  proxy1->OrderingBarrier(30);
  proxy2->FlushPendingWork();

  EXPECT_EQ(1u, sink_.message_count());
  const IPC::Message* msg =
      sink_.GetFirstMessageMatching(GpuChannelMsg_FlushDeferredMessages::ID);
  ASSERT_TRUE(msg);
  GpuChannelMsg_FlushDeferredMessages::Param params;
  ASSERT_TRUE(GpuChannelMsg_FlushDeferredMessages::Read(msg, &params));
  std::vector<GpuDeferredMessage> deferred_messages =
      std::get<0>(std::move(params));
  EXPECT_EQ(3u, deferred_messages.size());
  ExpectOrderingBarrier(deferred_messages[0], proxy1->route_id(), 10);
  ExpectOrderingBarrier(deferred_messages[1], proxy2->route_id(), 20);
  ExpectOrderingBarrier(deferred_messages[2], proxy1->route_id(), 30);
}

TEST_F(CommandBufferProxyImplTest, EnsureWorkVisibleFlushesOrderingBarriers) {
  auto proxy1 = CreateAndInitializeProxy();
  auto proxy2 = CreateAndInitializeProxy();

  proxy1->OrderingBarrier(10);
  proxy2->OrderingBarrier(20);
  proxy1->OrderingBarrier(30);

  proxy2->EnsureWorkVisible();

  EXPECT_EQ(2u, sink_.message_count());
  const IPC::Message* msg = sink_.GetMessageAt(0);
  ASSERT_TRUE(msg);
  EXPECT_EQ(static_cast<uint32_t>(GpuChannelMsg_FlushDeferredMessages::ID),
            msg->type());

  GpuChannelMsg_FlushDeferredMessages::Param params;
  ASSERT_TRUE(GpuChannelMsg_FlushDeferredMessages::Read(msg, &params));
  std::vector<GpuDeferredMessage> deferred_messages =
      std::get<0>(std::move(params));
  EXPECT_EQ(3u, deferred_messages.size());
  ExpectOrderingBarrier(deferred_messages[0], proxy1->route_id(), 10);
  ExpectOrderingBarrier(deferred_messages[1], proxy2->route_id(), 20);
  ExpectOrderingBarrier(deferred_messages[2], proxy1->route_id(), 30);

  msg = sink_.GetMessageAt(1);
  ASSERT_TRUE(msg);
  EXPECT_EQ(static_cast<uint32_t>(GpuChannelMsg_Nop::ID), msg->type());
}

TEST_F(CommandBufferProxyImplTest,
       EnqueueDeferredMessageEnqueuesPendingOrderingBarriers) {
  auto proxy1 = CreateAndInitializeProxy();

  proxy1->OrderingBarrier(10);
  proxy1->OrderingBarrier(20);
  channel_->EnqueueDeferredMessage(
      GpuCommandBufferMsg_DestroyTransferBuffer(proxy1->route_id(), 3));
  EXPECT_EQ(0u, sink_.message_count());
  proxy1->FlushPendingWork();

  EXPECT_EQ(1u, sink_.message_count());
  const IPC::Message* msg =
      sink_.GetFirstMessageMatching(GpuChannelMsg_FlushDeferredMessages::ID);
  ASSERT_TRUE(msg);
  GpuChannelMsg_FlushDeferredMessages::Param params;
  ASSERT_TRUE(GpuChannelMsg_FlushDeferredMessages::Read(msg, &params));
  std::vector<GpuDeferredMessage> deferred_messages =
      std::get<0>(std::move(params));
  EXPECT_EQ(2u, deferred_messages.size());
  ExpectOrderingBarrier(deferred_messages[0], proxy1->route_id(), 20);
  EXPECT_EQ(deferred_messages[1].message.type(),
            GpuCommandBufferMsg_DestroyTransferBuffer::ID);
}
}  // namespace gpu
