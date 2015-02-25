// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/mojo/ipc_channel_mojo.h"

#include "base/base_paths.h"
#include "base/files/file.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/pickle.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_test_base.h"
#include "ipc/ipc_test_channel_listener.h"
#include "ipc/mojo/ipc_channel_mojo_host.h"
#include "ipc/mojo/ipc_mojo_handle_attachment.h"
#include "ipc/mojo/ipc_mojo_message_helper.h"

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#include "ipc/ipc_platform_file_attachment_posix.h"
#endif

namespace {

class ListenerThatExpectsOK : public IPC::Listener {
 public:
  ListenerThatExpectsOK()
      : received_ok_(false) {}

  ~ListenerThatExpectsOK() override {}

  bool OnMessageReceived(const IPC::Message& message) override {
    PickleIterator iter(message);
    std::string should_be_ok;
    EXPECT_TRUE(iter.ReadString(&should_be_ok));
    EXPECT_EQ(should_be_ok, "OK");
    received_ok_ = true;
    base::MessageLoop::current()->Quit();
    return true;
  }

  void OnChannelError() override {
    // The connection should be healthy while the listener is waiting
    // message.  An error can occur after that because the peer
    // process dies.
    DCHECK(received_ok_);
  }

  static void SendOK(IPC::Sender* sender) {
    IPC::Message* message = new IPC::Message(
        0, 2, IPC::Message::PRIORITY_NORMAL);
    message->WriteString(std::string("OK"));
    ASSERT_TRUE(sender->Send(message));
  }

 private:
  bool received_ok_;
};

class ChannelClient {
 public:
  explicit ChannelClient(IPC::Listener* listener, const char* name) {
    channel_ = IPC::ChannelMojo::Create(NULL,
                                        IPCTestBase::GetChannelName(name),
                                        IPC::Channel::MODE_CLIENT,
                                        listener);
  }

  void Connect() {
    CHECK(channel_->Connect());
  }

  IPC::ChannelMojo* channel() const { return channel_.get(); }

 private:
  base::MessageLoopForIO main_message_loop_;
  scoped_ptr<IPC::ChannelMojo> channel_;
};

class IPCChannelMojoTest : public IPCTestBase {
 protected:
  scoped_ptr<IPC::ChannelFactory> CreateChannelFactory(
      const IPC::ChannelHandle& handle,
      base::TaskRunner* runner) override {
    host_.reset(new IPC::ChannelMojoHost(task_runner()));
    return IPC::ChannelMojo::CreateServerFactory(host_->channel_delegate(),
                                                 handle);
  }

  bool DidStartClient() override {
    bool ok = IPCTestBase::DidStartClient();
    DCHECK(ok);
    host_->OnClientLaunched(client_process().Handle());
    return ok;
  }

 private:
  scoped_ptr<IPC::ChannelMojoHost> host_;
};


class TestChannelListenerWithExtraExpectations
    : public IPC::TestChannelListener {
 public:
  TestChannelListenerWithExtraExpectations()
      : is_connected_called_(false) {
  }

  void OnChannelConnected(int32 peer_pid) override {
    IPC::TestChannelListener::OnChannelConnected(peer_pid);
    EXPECT_TRUE(base::kNullProcessId != peer_pid);
    is_connected_called_ = true;
  }

  bool is_connected_called() const { return is_connected_called_; }

 private:
  bool is_connected_called_;
};

TEST_F(IPCChannelMojoTest, ConnectedFromClient) {
  Init("IPCChannelMojoTestClient");

  // Set up IPC channel and start client.
  TestChannelListenerWithExtraExpectations listener;
  CreateChannel(&listener);
  listener.Init(sender());
  ASSERT_TRUE(ConnectChannel());
  ASSERT_TRUE(StartClient());

  IPC::TestChannelListener::SendOneMessage(
      sender(), "hello from parent");

  base::MessageLoop::current()->Run();
  EXPECT_TRUE(base::kNullProcessId != this->channel()->GetPeerPID());

  this->channel()->Close();

  EXPECT_TRUE(WaitForClientShutdown());
  EXPECT_TRUE(listener.is_connected_called());
  EXPECT_TRUE(listener.HasSentAll());

  DestroyChannel();
}

// A long running process that connects to us
MULTIPROCESS_IPC_TEST_CLIENT_MAIN(IPCChannelMojoTestClient) {
  TestChannelListenerWithExtraExpectations listener;
  ChannelClient client(&listener, "IPCChannelMojoTestClient");
  client.Connect();
  listener.Init(client.channel());

  IPC::TestChannelListener::SendOneMessage(
      client.channel(), "hello from child");
  base::MessageLoop::current()->Run();
  EXPECT_TRUE(listener.is_connected_called());
  EXPECT_TRUE(listener.HasSentAll());

  return 0;
}

class ListenerExpectingErrors : public IPC::Listener {
 public:
  ListenerExpectingErrors()
      : has_error_(false) {
  }

  void OnChannelConnected(int32 peer_pid) override {
    base::MessageLoop::current()->Quit();
  }

  bool OnMessageReceived(const IPC::Message& message) override { return true; }

  void OnChannelError() override {
    has_error_ = true;
    base::MessageLoop::current()->Quit();
  }

  bool has_error() const { return has_error_; }

 private:
  bool has_error_;
};


class IPCChannelMojoErrorTest : public IPCTestBase {
 protected:
  scoped_ptr<IPC::ChannelFactory> CreateChannelFactory(
      const IPC::ChannelHandle& handle,
      base::TaskRunner* runner) override {
    host_.reset(new IPC::ChannelMojoHost(task_runner()));
    return IPC::ChannelMojo::CreateServerFactory(host_->channel_delegate(),
                                                 handle);
  }

  bool DidStartClient() override {
    bool ok = IPCTestBase::DidStartClient();
    DCHECK(ok);
    host_->OnClientLaunched(client_process().Handle());
    return ok;
  }

 private:
  scoped_ptr<IPC::ChannelMojoHost> host_;
};

class ListenerThatQuits : public IPC::Listener {
 public:
  ListenerThatQuits() {
  }

  bool OnMessageReceived(const IPC::Message& message) override {
    return true;
  }

  void OnChannelConnected(int32 peer_pid) override {
    base::MessageLoop::current()->Quit();
  }
};

// A long running process that connects to us.
MULTIPROCESS_IPC_TEST_CLIENT_MAIN(IPCChannelMojoErraticTestClient) {
  ListenerThatQuits listener;
  ChannelClient client(&listener, "IPCChannelMojoErraticTestClient");
  client.Connect();

  base::MessageLoop::current()->Run();

  return 0;
}

TEST_F(IPCChannelMojoErrorTest, SendFailWithPendingMessages) {
  Init("IPCChannelMojoErraticTestClient");

  // Set up IPC channel and start client.
  ListenerExpectingErrors listener;
  CreateChannel(&listener);
  ASSERT_TRUE(ConnectChannel());

  // This matches a value in mojo/edk/system/constants.h
  const int kMaxMessageNumBytes = 4 * 1024 * 1024;
  std::string overly_large_data(kMaxMessageNumBytes, '*');
  // This messages are queued as pending.
  for (size_t i = 0; i < 10; ++i) {
    IPC::TestChannelListener::SendOneMessage(
        sender(), overly_large_data.c_str());
  }

  ASSERT_TRUE(StartClient());
  base::MessageLoop::current()->Run();

  this->channel()->Close();

  EXPECT_TRUE(WaitForClientShutdown());
  EXPECT_TRUE(listener.has_error());

  DestroyChannel();
}

struct TestingMessagePipe {
  TestingMessagePipe() {
    EXPECT_EQ(MOJO_RESULT_OK, mojo::CreateMessagePipe(nullptr, &self, &peer));
  }

  mojo::ScopedMessagePipeHandle self;
  mojo::ScopedMessagePipeHandle peer;
};

class HandleSendingHelper {
 public:
  static std::string GetSendingFileContent() { return "Hello"; }

  static void WritePipe(IPC::Message* message, TestingMessagePipe* pipe) {
    std::string content = HandleSendingHelper::GetSendingFileContent();
    EXPECT_EQ(MOJO_RESULT_OK,
              mojo::WriteMessageRaw(pipe->self.get(), &content[0],
                                    static_cast<uint32_t>(content.size()),
                                    nullptr, 0, 0));
    EXPECT_TRUE(
        IPC::MojoMessageHelper::WriteMessagePipeTo(message, pipe->peer.Pass()));
  }

  static void WritePipeThenSend(IPC::Sender* sender, TestingMessagePipe* pipe) {
    IPC::Message* message =
        new IPC::Message(0, 2, IPC::Message::PRIORITY_NORMAL);
    WritePipe(message, pipe);
    ASSERT_TRUE(sender->Send(message));
  }

  static void ReadReceivedPipe(const IPC::Message& message,
                               PickleIterator* iter) {
    mojo::ScopedMessagePipeHandle pipe;
    EXPECT_TRUE(
        IPC::MojoMessageHelper::ReadMessagePipeFrom(&message, iter, &pipe));
    std::string content(GetSendingFileContent().size(), ' ');

    uint32_t num_bytes = static_cast<uint32_t>(content.size());
    EXPECT_EQ(MOJO_RESULT_OK,
              mojo::ReadMessageRaw(pipe.get(), &content[0], &num_bytes, nullptr,
                                   nullptr, 0));
    EXPECT_EQ(content, GetSendingFileContent());
  }

#if defined(OS_POSIX)
  static base::FilePath GetSendingFilePath() {
    base::FilePath path;
    bool ok = PathService::Get(base::DIR_CACHE, &path);
    EXPECT_TRUE(ok);
    return path.Append("ListenerThatExpectsFile.txt");
  }

  static void WriteFile(IPC::Message* message, base::File& file) {
    std::string content = GetSendingFileContent();
    file.WriteAtCurrentPos(content.data(), content.size());
    file.Flush();
    message->WriteAttachment(new IPC::internal::PlatformFileAttachment(
        base::ScopedFD(file.TakePlatformFile())));
  }

  static void WriteFileThenSend(IPC::Sender* sender, base::File& file) {
    IPC::Message* message =
        new IPC::Message(0, 2, IPC::Message::PRIORITY_NORMAL);
    WriteFile(message, file);
    ASSERT_TRUE(sender->Send(message));
  }

  static void WriteFileAndPipeThenSend(IPC::Sender* sender,
                                       base::File& file,
                                       TestingMessagePipe* pipe) {
    IPC::Message* message =
        new IPC::Message(0, 2, IPC::Message::PRIORITY_NORMAL);
    WriteFile(message, file);
    WritePipe(message, pipe);
    ASSERT_TRUE(sender->Send(message));
  }

  static void ReadReceivedFile(const IPC::Message& message,
                               PickleIterator* iter) {
    base::ScopedFD fd;
    scoped_refptr<IPC::MessageAttachment> attachment;
    EXPECT_TRUE(message.ReadAttachment(iter, &attachment));
    base::File file(attachment->TakePlatformFile());
    std::string content(GetSendingFileContent().size(), ' ');
    file.Read(0, &content[0], content.size());
    EXPECT_EQ(content, GetSendingFileContent());
  }
#endif
};

class ListenerThatExpectsMessagePipe : public IPC::Listener {
 public:
  ListenerThatExpectsMessagePipe() : sender_(NULL) {}

  ~ListenerThatExpectsMessagePipe() override {}

  bool OnMessageReceived(const IPC::Message& message) override {
    PickleIterator iter(message);
    HandleSendingHelper::ReadReceivedPipe(message, &iter);
    base::MessageLoop::current()->Quit();
    ListenerThatExpectsOK::SendOK(sender_);
    return true;
  }

  void OnChannelError() override { NOTREACHED(); }

  void set_sender(IPC::Sender* sender) { sender_ = sender; }

 private:
  IPC::Sender* sender_;
};

TEST_F(IPCChannelMojoTest, SendMessagePipe) {
  Init("IPCChannelMojoTestSendMessagePipeClient");

  ListenerThatExpectsOK listener;
  CreateChannel(&listener);
  ASSERT_TRUE(ConnectChannel());
  ASSERT_TRUE(StartClient());

  TestingMessagePipe pipe;
  HandleSendingHelper::WritePipeThenSend(channel(), &pipe);

  base::MessageLoop::current()->Run();
  this->channel()->Close();

  EXPECT_TRUE(WaitForClientShutdown());
  DestroyChannel();
}

MULTIPROCESS_IPC_TEST_CLIENT_MAIN(IPCChannelMojoTestSendMessagePipeClient) {
  ListenerThatExpectsMessagePipe listener;
  ChannelClient client(&listener, "IPCChannelMojoTestSendPlatformHandleClient");
  client.Connect();
  listener.set_sender(client.channel());

  base::MessageLoop::current()->Run();

  return 0;
}

#if defined(OS_WIN)
class IPCChannelMojoDeadHandleTest : public IPCTestBase {
 protected:
  virtual scoped_ptr<IPC::ChannelFactory> CreateChannelFactory(
      const IPC::ChannelHandle& handle,
      base::TaskRunner* runner) override {
    host_.reset(new IPC::ChannelMojoHost(task_runner()));
    return IPC::ChannelMojo::CreateServerFactory(host_->channel_delegate(),
                                                 handle);
  }

  virtual bool DidStartClient() override {
    IPCTestBase::DidStartClient();
    const base::ProcessHandle client = client_process().Handle();
    // Forces GetFileHandleForProcess() fail. It happens occasionally
    // in production, so we should exercise it somehow.
    // TODO(morrita): figure out how to safely test this.
    // ::CloseHandle(client);
    host_->OnClientLaunched(client);
    return true;
  }

 private:
  scoped_ptr<IPC::ChannelMojoHost> host_;
};

TEST_F(IPCChannelMojoDeadHandleTest, InvalidClientHandle) {
  // Any client type is fine as it is going to be killed anyway.
  Init("IPCChannelMojoTestDoNothingClient");

  // Set up IPC channel and start client.
  ListenerExpectingErrors listener;
  CreateChannel(&listener);
  ASSERT_TRUE(ConnectChannel());

  ASSERT_TRUE(StartClient());
  base::MessageLoop::current()->Run();

  this->channel()->Close();

  // WaitForClientShutdown() fails as client_hanadle() is already
  // closed.
  EXPECT_FALSE(WaitForClientShutdown());
  EXPECT_TRUE(listener.has_error());

  DestroyChannel();
}

MULTIPROCESS_IPC_TEST_CLIENT_MAIN(IPCChannelMojoTestDoNothingClient) {
  ListenerThatQuits listener;
  ChannelClient client(&listener, "IPCChannelMojoTestDoNothingClient");
  client.Connect();

  // Quits without running the message loop as this client won't
  // receive any messages from the server.

  return 0;
}
#endif

#if defined(OS_POSIX)
class ListenerThatExpectsFile : public IPC::Listener {
 public:
  ListenerThatExpectsFile()
      : sender_(NULL) {}

  ~ListenerThatExpectsFile() override {}

  bool OnMessageReceived(const IPC::Message& message) override {
    PickleIterator iter(message);
    HandleSendingHelper::ReadReceivedFile(message, &iter);
    base::MessageLoop::current()->Quit();
    ListenerThatExpectsOK::SendOK(sender_);
    return true;
  }

  void OnChannelError() override {
    NOTREACHED();
  }

  void set_sender(IPC::Sender* sender) { sender_ = sender; }

 private:
  IPC::Sender* sender_;
};


TEST_F(IPCChannelMojoTest, SendPlatformHandle) {
  Init("IPCChannelMojoTestSendPlatformHandleClient");

  ListenerThatExpectsOK listener;
  CreateChannel(&listener);
  ASSERT_TRUE(ConnectChannel());
  ASSERT_TRUE(StartClient());

  base::File file(HandleSendingHelper::GetSendingFilePath(),
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE |
                      base::File::FLAG_READ);
  HandleSendingHelper::WriteFileThenSend(channel(), file);
  base::MessageLoop::current()->Run();

  this->channel()->Close();

  EXPECT_TRUE(WaitForClientShutdown());
  DestroyChannel();
}

MULTIPROCESS_IPC_TEST_CLIENT_MAIN(IPCChannelMojoTestSendPlatformHandleClient) {
  ListenerThatExpectsFile listener;
  ChannelClient client(
      &listener, "IPCChannelMojoTestSendPlatformHandleClient");
  client.Connect();
  listener.set_sender(client.channel());

  base::MessageLoop::current()->Run();

  return 0;
}

class ListenerThatExpectsFileAndPipe : public IPC::Listener {
 public:
  ListenerThatExpectsFileAndPipe() : sender_(NULL) {}

  ~ListenerThatExpectsFileAndPipe() override {}

  bool OnMessageReceived(const IPC::Message& message) override {
    PickleIterator iter(message);
    HandleSendingHelper::ReadReceivedFile(message, &iter);
    HandleSendingHelper::ReadReceivedPipe(message, &iter);
    base::MessageLoop::current()->Quit();
    ListenerThatExpectsOK::SendOK(sender_);
    return true;
  }

  void OnChannelError() override { NOTREACHED(); }

  void set_sender(IPC::Sender* sender) { sender_ = sender; }

 private:
  IPC::Sender* sender_;
};

TEST_F(IPCChannelMojoTest, SendPlatformHandleAndPipe) {
  Init("IPCChannelMojoTestSendPlatformHandleAndPipeClient");

  ListenerThatExpectsOK listener;
  CreateChannel(&listener);
  ASSERT_TRUE(ConnectChannel());
  ASSERT_TRUE(StartClient());

  base::File file(HandleSendingHelper::GetSendingFilePath(),
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE |
                      base::File::FLAG_READ);
  TestingMessagePipe pipe;
  HandleSendingHelper::WriteFileAndPipeThenSend(channel(), file, &pipe);

  base::MessageLoop::current()->Run();
  this->channel()->Close();

  EXPECT_TRUE(WaitForClientShutdown());
  DestroyChannel();
}

MULTIPROCESS_IPC_TEST_CLIENT_MAIN(
    IPCChannelMojoTestSendPlatformHandleAndPipeClient) {
  ListenerThatExpectsFileAndPipe listener;
  ChannelClient client(&listener,
                       "IPCChannelMojoTestSendPlatformHandleAndPipeClient");
  client.Connect();
  listener.set_sender(client.channel());

  base::MessageLoop::current()->Run();

  return 0;
}

#endif

#if defined(OS_LINUX)

const base::ProcessId kMagicChildId = 54321;

class ListenerThatVerifiesPeerPid : public IPC::Listener {
 public:
  void OnChannelConnected(int32 peer_pid) override {
    EXPECT_EQ(peer_pid, kMagicChildId);
    base::MessageLoop::current()->Quit();
  }

  bool OnMessageReceived(const IPC::Message& message) override {
    NOTREACHED();
    return true;
  }
};

TEST_F(IPCChannelMojoTest, VerifyGlobalPid) {
  Init("IPCChannelMojoTestVerifyGlobalPidClient");

  ListenerThatVerifiesPeerPid listener;
  CreateChannel(&listener);
  ASSERT_TRUE(ConnectChannel());
  ASSERT_TRUE(StartClient());

  base::MessageLoop::current()->Run();
  this->channel()->Close();

  EXPECT_TRUE(WaitForClientShutdown());
  DestroyChannel();
}

MULTIPROCESS_IPC_TEST_CLIENT_MAIN(IPCChannelMojoTestVerifyGlobalPidClient) {
  IPC::Channel::SetGlobalPid(kMagicChildId);
  ListenerThatQuits listener;
  ChannelClient client(&listener,
                       "IPCChannelMojoTestVerifyGlobalPidClient");
  client.Connect();

  base::MessageLoop::current()->Run();

  return 0;
}

#endif // OS_LINUX

}  // namespace
