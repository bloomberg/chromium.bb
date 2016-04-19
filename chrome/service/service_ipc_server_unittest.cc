// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/service_ipc_server.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "chrome/common/service_messages.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

void PumpCurrentLoop() {
  base::MessageLoop::ScopedNestableTaskAllower nestable_task_allower(
      base::MessageLoop::current());
  base::RunLoop().RunUntilIdle();
}

class FakeServiceIPCServerClient : public ServiceIPCServer::Client {
 public:
  FakeServiceIPCServerClient() {}
  ~FakeServiceIPCServerClient() override {}
  void OnShutdown() override;
  void OnUpdateAvailable() override;
  bool OnIPCClientDisconnect() override;

  int shutdown_calls = 0;
  int update_available_calls = 0;
  int ipc_client_disconnect_calls = 0;
};

void FakeServiceIPCServerClient::OnShutdown() {
  shutdown_calls++;
}

void FakeServiceIPCServerClient::OnUpdateAvailable() {
  update_available_calls++;
}

bool FakeServiceIPCServerClient::OnIPCClientDisconnect() {
  ipc_client_disconnect_calls++;

  // Always return true to indicate the server must continue listening for new
  // connections.
  return true;
}

class FakeChannelListener : public IPC::Listener {
 public:
  FakeChannelListener() {}
  ~FakeChannelListener() override {}
  bool OnMessageReceived(const IPC::Message& message) override { return true; }
};

class FakeMessageHandler : public ServiceIPCServer::MessageHandler {
 public:
  explicit FakeMessageHandler(bool should_handle);
  bool HandleMessage(const IPC::Message& message) override;
  bool should_handle_;
  int handle_message_calls_;
};

FakeMessageHandler::FakeMessageHandler(bool should_handle)
    : should_handle_(should_handle), handle_message_calls_(0) {
}

bool FakeMessageHandler::HandleMessage(const IPC::Message& message) {
  handle_message_calls_++;
  return should_handle_;
}

}  // namespace

class ServiceIPCServerTest : public ::testing::Test {
 public:
  ServiceIPCServerTest();
  ~ServiceIPCServerTest() override {}
  void SetUp() override;
  void TearDown() override;
  void PumpLoops();

  // Simulates the browser process connecting to the service process.
  void ConnectClientChannel();

  // Simulates the browser process shutting down.
  void DestroyClientChannel();

  // Sends |message| to the ServiceIPCServer.
  void SendToServiceProcess(IPC::Message* message);

  IPC::SyncChannel* GetServerChannel() {
    return server_->channel_.get();
  }

 protected:
  FakeServiceIPCServerClient service_process_client_;
  IPC::ChannelHandle channel_handle_;
  base::MessageLoopForUI main_message_loop_;
  base::Thread io_thread_;
  base::WaitableEvent shutdown_event_;
  std::unique_ptr<ServiceIPCServer> server_;
  FakeChannelListener client_process_channel_listener_;
  std::unique_ptr<IPC::SyncChannel> client_process_channel_;
};

ServiceIPCServerTest::ServiceIPCServerTest()
    : channel_handle_(IPC::Channel::GenerateUniqueRandomChannelID()),
      io_thread_("ServiceIPCServerTest IO"),
      shutdown_event_(true /* manual_reset */, false /* initially_signaled */) {
}

void ServiceIPCServerTest::SetUp() {
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  ASSERT_TRUE(io_thread_.StartWithOptions(options));

  server_.reset(new ServiceIPCServer(&service_process_client_,
                                     io_thread_.task_runner(),
                                     channel_handle_,
                                     &shutdown_event_));
  server_->Init();
}

void ServiceIPCServerTest::TearDown() {
  // Close the ipc channels to prevent memory leaks.
  if (client_process_channel_) {
    client_process_channel_->Close();
    PumpLoops();
  }
  if (GetServerChannel()) {
    GetServerChannel()->Close();
    PumpLoops();
  }
  io_thread_.Stop();
}

void ServiceIPCServerTest::PumpLoops() {
  base::RunLoop run_loop;
  io_thread_.task_runner()->PostTaskAndReply(FROM_HERE,
                                             base::Bind(&PumpCurrentLoop),
                                             run_loop.QuitClosure());
  run_loop.Run();
  PumpCurrentLoop();
}

void ServiceIPCServerTest::ConnectClientChannel() {
  client_process_channel_ = IPC::SyncChannel::Create(
      channel_handle_,
      IPC::Channel::MODE_NAMED_CLIENT,
      &client_process_channel_listener_,
      io_thread_.task_runner(),
      true /* create_pipe_now */,
      &shutdown_event_);
  PumpLoops();
}

void ServiceIPCServerTest::DestroyClientChannel() {
  client_process_channel_.reset();
  PumpLoops();
}

void ServiceIPCServerTest::SendToServiceProcess(IPC::Message* message) {
  client_process_channel_->Send(message);
  PumpLoops();
}

TEST_F(ServiceIPCServerTest, ConnectDisconnectReconnect) {
  // Initially there is no ipc client connected.
  ASSERT_FALSE(server_->is_ipc_client_connected());

  // When a channel is connected the server is notified via OnChannelConnected.
  ConnectClientChannel();
  ASSERT_TRUE(server_->is_ipc_client_connected());

  // When the channel is destroyed the server is notified via OnChannelError.
  // In turn, the server notifies its service process client.
  DestroyClientChannel();
  ASSERT_FALSE(server_->is_ipc_client_connected());
  ASSERT_EQ(1, service_process_client_.ipc_client_disconnect_calls);

  // On Windows only, the server recreates its channel in OnChannelError, if the
  // service process client tells it to continue listening. On other platforms
  // the channel is reused for subsequent reconnects by the client process. This
  // means however that OnChannelConnected is not called again and the server is
  // only aware of being connected once an IPC message is received.
  ConnectClientChannel();
#if defined(OS_WIN)
  ASSERT_TRUE(server_->is_ipc_client_connected());
#else
  ASSERT_FALSE(server_->is_ipc_client_connected());
#endif
  SendToServiceProcess(new ServiceMsg_UpdateAvailable());
  ASSERT_TRUE(server_->is_ipc_client_connected());

  // Destroy the client process channel again to verify the
  // ServiceIPCServer::Client is notified again. This means that unlike
  // OnChannelConnected, OnChannelError is called more than once.
  DestroyClientChannel();
  ASSERT_FALSE(server_->is_ipc_client_connected());
  ASSERT_EQ(2, service_process_client_.ipc_client_disconnect_calls);
}

TEST_F(ServiceIPCServerTest, Shutdown) {
  ConnectClientChannel();
  ASSERT_TRUE(server_->is_ipc_client_connected());

  // When a shutdown message is received, the ServiceIPCServer::Client is
  // notified.
  SendToServiceProcess(new ServiceMsg_Shutdown());
  ASSERT_EQ(1, service_process_client_.shutdown_calls);
}

TEST_F(ServiceIPCServerTest, UpdateAvailable) {
  ConnectClientChannel();
  ASSERT_TRUE(server_->is_ipc_client_connected());

  // When a product update message is received, the ServiceIPCServer::Client is
  // notified.
  SendToServiceProcess(new ServiceMsg_UpdateAvailable());
  ASSERT_EQ(1, service_process_client_.update_available_calls);
}

TEST_F(ServiceIPCServerTest, SingleMessageHandler) {
  ConnectClientChannel();
  ASSERT_TRUE(server_->is_ipc_client_connected());

  // Verify that a message handler is offered messages not handled by the server
  // itself.
  FakeMessageHandler* handler =
      new FakeMessageHandler(true /* should_handle */);
  server_->AddMessageHandler(base::WrapUnique(handler));
  SendToServiceProcess(new ServiceMsg_DisableCloudPrintProxy());
  ASSERT_EQ(1, handler->handle_message_calls_);
}

TEST_F(ServiceIPCServerTest, MultipleMessageHandlers) {
  ConnectClientChannel();
  ASSERT_TRUE(server_->is_ipc_client_connected());

  // If there are multiple handlers they are offered the message in order of
  // being added until it is handled.
  FakeMessageHandler* handler1 =
      new FakeMessageHandler(false /* should_handle */);
  server_->AddMessageHandler(base::WrapUnique(handler1));
  FakeMessageHandler* handler2 =
      new FakeMessageHandler(true /* should_handle */);
  server_->AddMessageHandler(base::WrapUnique(handler2));
  FakeMessageHandler* handler3 =
      new FakeMessageHandler(true /* should_handle */);
  server_->AddMessageHandler(base::WrapUnique(handler3));
  SendToServiceProcess(new ServiceMsg_DisableCloudPrintProxy());
  ASSERT_EQ(1, handler1->handle_message_calls_);
  ASSERT_EQ(1, handler2->handle_message_calls_);
  ASSERT_EQ(0, handler3->handle_message_calls_);
}
