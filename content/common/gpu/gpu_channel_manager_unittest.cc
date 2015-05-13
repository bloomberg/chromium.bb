// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_simple_task_runner.h"
#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/gpu_channel_manager.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/message_router.h"
#include "gpu/command_buffer/common/value_state.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/valuebuffer_manager.h"
#include "ipc/ipc_sync_channel.h"

#include "testing/gtest/include/gtest/gtest.h"

using base::WaitableEvent;
using gpu::gles2::ValuebufferManager;
using gpu::ValueState;

namespace IPC {

class SimpleWorker : public Listener, public Sender {
 public:
  SimpleWorker(Channel::Mode mode, const std::string& thread_name)
      : mode_(mode),
        ipc_thread_((thread_name + "_ipc").c_str()),
        shutdown_event_(true, false) {
  }

  ~SimpleWorker() override {
  }

  void AddRef() { }
  void Release() { }
  bool Send(Message* msg) override { return channel_->Send(msg); }

  virtual void Start() {
    StartThread(&ipc_thread_, base::MessageLoop::TYPE_IO);
    channel_.reset(CreateChannel());
  }

  virtual void Shutdown() {
    WaitableEvent ipc_done(false, false);
    ipc_thread_.message_loop()->PostTask(
        FROM_HERE, base::Bind(&SimpleWorker::OnShutdown, this, &ipc_done));

    channel_.reset();

    ipc_done.Wait();
    ipc_thread_.Stop();
  }

 protected:
  SyncChannel* CreateChannel() {
    scoped_ptr<SyncChannel> channel = SyncChannel::Create(
        channel_name_, mode_, this, ipc_thread_.message_loop_proxy().get(),
        true, &shutdown_event_);
    return channel.release();
  }

  void OnShutdown(WaitableEvent* ipc_event) {
    base::RunLoop().RunUntilIdle();
    ipc_event->Signal();
  }

  const base::Thread& ipc_thread() const { return ipc_thread_; }

  WaitableEvent* shutdown_event() { return &shutdown_event_; }

  SyncChannel* channel() { return channel_.get(); }

 private:
  bool OnMessageReceived(const Message& message) override { return false; }

  void StartThread(base::Thread* thread, base::MessageLoop::Type type) {
    base::Thread::Options options;
    options.message_loop_type = type;
    thread->StartWithOptions(options);
  }

  std::string channel_name_;
  Channel::Mode mode_;
  scoped_ptr<SyncChannel> channel_;
  base::Thread ipc_thread_;

  base::WaitableEvent shutdown_event_;

  DISALLOW_COPY_AND_ASSIGN(SimpleWorker);
};

class SimpleServer : public SimpleWorker {
 public:
  explicit SimpleServer()
    : SimpleWorker(Channel::MODE_SERVER, "simpler_server") { }
};

} // namespace IPC

namespace content {

class SimpleGpuClient : public IPC::SimpleWorker {
 public:
  SimpleGpuClient()
   : IPC::SimpleWorker(IPC::Channel::MODE_CLIENT, "simple_client"),
     router_(this) {
  }

  void Start() override {
    IPC::SimpleWorker::Start();
    gpu_channel_manager_.reset(
        new GpuChannelManager(&router_,
                              NULL,
                              ipc_thread().message_loop_proxy().get(),
                              shutdown_event(),
                              channel()));
  }

  void Shutdown() override {
    gpu_channel_manager_.reset();
    IPC::SimpleWorker::Shutdown();
  }

  GpuChannelManager* gpu_channel_manager() {
    return gpu_channel_manager_.get();
  }

 private:
  class SimpleMessageRouter : public MessageRouter {
   public:
    explicit SimpleMessageRouter(IPC::Sender* sender)
      : sender_(sender) {
    }

    bool Send(IPC::Message* msg) override { return sender_->Send(msg); }

   private:
    IPC::Sender* const sender_;
  };

  SimpleMessageRouter router_;

  scoped_ptr<GpuChannelManager> gpu_channel_manager_;
};

class GpuChannelManagerTest : public testing::Test {
 public:
  GpuChannelManagerTest() {}

  void SetUp() override {
    simple_client_.reset(new SimpleGpuClient());
    simple_server_.reset(new IPC::SimpleServer());
    simple_server_->Start();
    simple_client_->Start();
  }

  void TearDown() override {
    simple_client_->Shutdown();
    simple_server_->Shutdown();
  }

 protected:
  scoped_ptr<SimpleGpuClient> simple_client_;
  scoped_ptr<IPC::SimpleServer> simple_server_;

 private:
  base::MessageLoop message_loop_;
};

TEST_F(GpuChannelManagerTest, SecureValueStateForwarding) {
  const int kClientId1 = 111;
  const int kClientId2 = 222;
  ValueState value_state1;
  value_state1.int_value[0] = 1111;
  value_state1.int_value[1] = 0;
  value_state1.int_value[2] = 0;
  value_state1.int_value[3] = 0;
  ValueState value_state2;
  value_state2.int_value[0] = 3333;
  value_state2.int_value[1] = 0;
  value_state2.int_value[2] = 0;
  value_state2.int_value[3] = 0;

  ASSERT_TRUE(simple_client_->gpu_channel_manager() != NULL);

  // Initialize gpu channels
  simple_client_->gpu_channel_manager()->OnMessageReceived(
      GpuMsg_EstablishChannel(kClientId1, false, false));
  GpuChannel *channel1 =
      simple_client_->gpu_channel_manager()->LookupChannel(kClientId1);
  simple_client_->gpu_channel_manager()->OnMessageReceived(
      GpuMsg_EstablishChannel(kClientId2, false, false));
  GpuChannel *channel2 =
      simple_client_->gpu_channel_manager()->LookupChannel(kClientId2);
  ASSERT_TRUE(channel1 != NULL);
  ASSERT_TRUE(channel2 != NULL);
  ASSERT_NE(channel1, channel2);

  // Make sure value states are only accessible by proper channels
  simple_client_->gpu_channel_manager()->OnMessageReceived(
      GpuMsg_UpdateValueState(
          kClientId1, GL_MOUSE_POSITION_CHROMIUM, value_state1));
  simple_client_->gpu_channel_manager()->OnMessageReceived(
      GpuMsg_UpdateValueState(
          kClientId2, GL_MOUSE_POSITION_CHROMIUM, value_state2));

  const gpu::ValueStateMap* pending_value_buffer_state1 =
      channel1->pending_valuebuffer_state();
  const gpu::ValueStateMap* pending_value_buffer_state2 =
      channel2->pending_valuebuffer_state();
  ASSERT_NE(pending_value_buffer_state1, pending_value_buffer_state2);

  const ValueState* state1 =
      pending_value_buffer_state1->GetState(GL_MOUSE_POSITION_CHROMIUM);
  const ValueState* state2 =
      pending_value_buffer_state2->GetState(GL_MOUSE_POSITION_CHROMIUM);
  ASSERT_NE(state1, state2);

  ASSERT_EQ(state1->int_value[0], value_state1.int_value[0]);
  ASSERT_EQ(state2->int_value[0], value_state2.int_value[0]);
  ASSERT_NE(state1->int_value[0], state2->int_value[0]);
}

}  // namespace content
