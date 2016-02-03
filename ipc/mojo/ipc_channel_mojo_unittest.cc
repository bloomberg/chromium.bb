// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/mojo/ipc_channel_mojo.h"

#include <stddef.h>
#include <stdint.h>
#include <utility>

#include "base/base_paths.h"
#include "base/files/file.h"
#include "base/location.h"
#include "base/path_service.h"
#include "base/pickle.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_test_base.h"
#include "ipc/ipc_test_channel_listener.h"
#include "ipc/mojo/ipc_mojo_handle_attachment.h"
#include "ipc/mojo/ipc_mojo_message_helper.h"
#include "ipc/mojo/ipc_mojo_param_traits.h"
#include "ipc/mojo/scoped_ipc_support.h"

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
    base::PickleIterator iter(message);
    std::string should_be_ok;
    EXPECT_TRUE(iter.ReadString(&should_be_ok));
    EXPECT_EQ(should_be_ok, "OK");
    received_ok_ = true;
    base::MessageLoop::current()->QuitWhenIdle();
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
    channel_ = IPC::ChannelMojo::Create(main_message_loop_.task_runner(),
                                        IPCTestBase::GetChannelName(name),
                                        IPC::Channel::MODE_CLIENT, listener);
  }

  void Connect() {
    CHECK(channel_->Connect());
  }

  void Close() {
    channel_->Close();

    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  run_loop.QuitClosure());
    run_loop.Run();
  }

  IPC::ChannelMojo* channel() const { return channel_.get(); }

 private:
  base::MessageLoopForIO main_message_loop_;
  scoped_ptr<IPC::ChannelMojo> channel_;
};

class IPCChannelMojoTestBase : public IPCTestBase {
 public:
  void InitWithMojo(const std::string& test_client_name) {
    Init(test_client_name);
  }

  void TearDown() override {
    // Make sure Mojo IPC support is properly shutdown on the I/O loop before
    // TearDown continues.
    base::RunLoop run_loop;
    task_runner()->PostTask(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();

    IPCTestBase::TearDown();
  }
};

class IPCChannelMojoTest : public IPCChannelMojoTestBase {
 protected:
  scoped_ptr<IPC::ChannelFactory> CreateChannelFactory(
      const IPC::ChannelHandle& handle,
      base::SequencedTaskRunner* runner) override {
    return IPC::ChannelMojo::CreateServerFactory(task_runner(), handle);
  }

  bool DidStartClient() override {
    bool ok = IPCTestBase::DidStartClient();
    DCHECK(ok);
    return ok;
  }
};


class TestChannelListenerWithExtraExpectations
    : public IPC::TestChannelListener {
 public:
  TestChannelListenerWithExtraExpectations()
      : is_connected_called_(false) {
  }

  void OnChannelConnected(int32_t peer_pid) override {
    IPC::TestChannelListener::OnChannelConnected(peer_pid);
    EXPECT_TRUE(base::kNullProcessId != peer_pid);
    is_connected_called_ = true;
  }

  bool is_connected_called() const { return is_connected_called_; }

 private:
  bool is_connected_called_;
};

// Times out on Android; see http://crbug.com/502290
#if defined(OS_ANDROID)
#define MAYBE_ConnectedFromClient DISABLED_ConnectedFromClient
#else
#define MAYBE_ConnectedFromClient ConnectedFromClient
#endif
TEST_F(IPCChannelMojoTest, MAYBE_ConnectedFromClient) {
  InitWithMojo("IPCChannelMojoTestClient");

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

  client.Close();

  return 0;
}

class ListenerExpectingErrors : public IPC::Listener {
 public:
  ListenerExpectingErrors()
      : has_error_(false) {
  }

  void OnChannelConnected(int32_t peer_pid) override {
    base::MessageLoop::current()->QuitWhenIdle();
  }

  bool OnMessageReceived(const IPC::Message& message) override { return true; }

  void OnChannelError() override {
    has_error_ = true;
    base::MessageLoop::current()->QuitWhenIdle();
  }

  bool has_error() const { return has_error_; }

 private:
  bool has_error_;
};


class IPCChannelMojoErrorTest : public IPCChannelMojoTestBase {
 protected:
  scoped_ptr<IPC::ChannelFactory> CreateChannelFactory(
      const IPC::ChannelHandle& handle,
      base::SequencedTaskRunner* runner) override {
    return IPC::ChannelMojo::CreateServerFactory(task_runner(), handle);
  }

  bool DidStartClient() override {
    bool ok = IPCTestBase::DidStartClient();
    DCHECK(ok);
    return ok;
  }
};

class ListenerThatQuits : public IPC::Listener {
 public:
  ListenerThatQuits() {
  }

  bool OnMessageReceived(const IPC::Message& message) override {
    return true;
  }

  void OnChannelConnected(int32_t peer_pid) override {
    base::MessageLoop::current()->QuitWhenIdle();
  }
};

// A long running process that connects to us.
MULTIPROCESS_IPC_TEST_CLIENT_MAIN(IPCChannelMojoErraticTestClient) {
  ListenerThatQuits listener;
  ChannelClient client(&listener, "IPCChannelMojoErraticTestClient");
  client.Connect();

  base::MessageLoop::current()->Run();

  client.Close();

  return 0;
}

// Times out on Android; see http://crbug.com/502290
#if defined(OS_ANDROID)
#define MAYBE_SendFailWithPendingMessages DISABLED_SendFailWithPendingMessages
#else
#define MAYBE_SendFailWithPendingMessages SendFailWithPendingMessages
#endif
TEST_F(IPCChannelMojoErrorTest, MAYBE_SendFailWithPendingMessages) {
  InitWithMojo("IPCChannelMojoErraticTestClient");

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
    EXPECT_TRUE(IPC::MojoMessageHelper::WriteMessagePipeTo(
        message, std::move(pipe->peer)));
  }

  static void WritePipeThenSend(IPC::Sender* sender, TestingMessagePipe* pipe) {
    IPC::Message* message =
        new IPC::Message(0, 2, IPC::Message::PRIORITY_NORMAL);
    WritePipe(message, pipe);
    ASSERT_TRUE(sender->Send(message));
  }

  static void ReadReceivedPipe(const IPC::Message& message,
                               base::PickleIterator* iter) {
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
                               base::PickleIterator* iter) {
    base::ScopedFD fd;
    scoped_refptr<base::Pickle::Attachment> attachment;
    EXPECT_TRUE(message.ReadAttachment(iter, &attachment));
    base::File file(static_cast<IPC::MessageAttachment*>(attachment.get())
                        ->TakePlatformFile());
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
    base::PickleIterator iter(message);
    HandleSendingHelper::ReadReceivedPipe(message, &iter);
    base::MessageLoop::current()->QuitWhenIdle();
    ListenerThatExpectsOK::SendOK(sender_);
    return true;
  }

  void OnChannelError() override { NOTREACHED(); }

  void set_sender(IPC::Sender* sender) { sender_ = sender; }

 private:
  IPC::Sender* sender_;
};

// Times out on Android; see http://crbug.com/502290
#if defined(OS_ANDROID)
#define MAYBE_SendMessagePipe DISABLED_SendMessagePipe
#else
#define MAYBE_SendMessagePipe SendMessagePipe
#endif
TEST_F(IPCChannelMojoTest, MAYBE_SendMessagePipe) {
  InitWithMojo("IPCChannelMojoTestSendMessagePipeClient");

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
  ChannelClient client(&listener, "IPCChannelMojoTestSendMessagePipeClient");
  client.Connect();
  listener.set_sender(client.channel());

  base::MessageLoop::current()->Run();

  client.Close();

  return 0;
}

void ReadOK(mojo::MessagePipeHandle pipe) {
  std::string should_be_ok("xx");
  uint32_t num_bytes = static_cast<uint32_t>(should_be_ok.size());
  CHECK_EQ(MOJO_RESULT_OK,
           mojo::ReadMessageRaw(pipe, &should_be_ok[0], &num_bytes, nullptr,
                                nullptr, 0));
  EXPECT_EQ(should_be_ok, std::string("OK"));
}

void WriteOK(mojo::MessagePipeHandle pipe) {
  std::string ok("OK");
  CHECK_EQ(MOJO_RESULT_OK,
           mojo::WriteMessageRaw(pipe, &ok[0], static_cast<uint32_t>(ok.size()),
                                 nullptr, 0, 0));
}

class ListenerThatExpectsMessagePipeUsingParamTrait : public IPC::Listener {
 public:
  explicit ListenerThatExpectsMessagePipeUsingParamTrait(bool receiving_valid)
      : sender_(NULL), receiving_valid_(receiving_valid) {}

  ~ListenerThatExpectsMessagePipeUsingParamTrait() override {}

  bool OnMessageReceived(const IPC::Message& message) override {
    base::PickleIterator iter(message);
    mojo::MessagePipeHandle handle;
    EXPECT_TRUE(IPC::ParamTraits<mojo::MessagePipeHandle>::Read(&message, &iter,
                                                                &handle));
    EXPECT_EQ(handle.is_valid(), receiving_valid_);
    if (receiving_valid_) {
      ReadOK(handle);
      MojoClose(handle.value());
    }

    base::MessageLoop::current()->QuitWhenIdle();
    ListenerThatExpectsOK::SendOK(sender_);
    return true;
  }

  void OnChannelError() override { NOTREACHED(); }
  void set_sender(IPC::Sender* sender) { sender_ = sender; }

 private:
  IPC::Sender* sender_;
  bool receiving_valid_;
};

void ParamTraitMessagePipeClient(bool receiving_valid_handle,
                                  const char* channel_name) {
  ListenerThatExpectsMessagePipeUsingParamTrait listener(
      receiving_valid_handle);
  ChannelClient client(&listener, channel_name);
  client.Connect();
  listener.set_sender(client.channel());

  base::MessageLoop::current()->Run();

  client.Close();
}

// Times out on Android; see http://crbug.com/502290
#if defined(OS_ANDROID)
#define MAYBE_ParamTraitValidMessagePipe DISABLED_ParamTraitValidMessagePipe
#else
#define MAYBE_ParamTraitValidMessagePipe ParamTraitValidMessagePipe
#endif
TEST_F(IPCChannelMojoTest, MAYBE_ParamTraitValidMessagePipe) {
  InitWithMojo("ParamTraitValidMessagePipeClient");

  ListenerThatExpectsOK listener;
  CreateChannel(&listener);
  ASSERT_TRUE(ConnectChannel());
  ASSERT_TRUE(StartClient());

  TestingMessagePipe pipe;

  scoped_ptr<IPC::Message> message(new IPC::Message());
  IPC::ParamTraits<mojo::MessagePipeHandle>::Write(message.get(),
                                                   pipe.peer.release());
  WriteOK(pipe.self.get());

  this->channel()->Send(message.release());
  base::MessageLoop::current()->Run();
  this->channel()->Close();

  EXPECT_TRUE(WaitForClientShutdown());
  DestroyChannel();
}

MULTIPROCESS_IPC_TEST_CLIENT_MAIN(ParamTraitValidMessagePipeClient) {
  ParamTraitMessagePipeClient(true, "ParamTraitValidMessagePipeClient");
  return 0;
}

// Times out on Android; see http://crbug.com/502290
#if defined(OS_ANDROID)
#define MAYBE_ParamTraitInvalidMessagePipe DISABLED_ParamTraitInvalidMessagePipe
#else
#define MAYBE_ParamTraitInvalidMessagePipe ParamTraitInvalidMessagePipe
#endif
TEST_F(IPCChannelMojoTest, MAYBE_ParamTraitInvalidMessagePipe) {
  InitWithMojo("ParamTraitInvalidMessagePipeClient");

  ListenerThatExpectsOK listener;
  CreateChannel(&listener);
  ASSERT_TRUE(ConnectChannel());
  ASSERT_TRUE(StartClient());

  mojo::MessagePipeHandle invalid_handle;
  scoped_ptr<IPC::Message> message(new IPC::Message());
  IPC::ParamTraits<mojo::MessagePipeHandle>::Write(message.get(),
                                                   invalid_handle);

  this->channel()->Send(message.release());
  base::MessageLoop::current()->Run();
  this->channel()->Close();

  EXPECT_TRUE(WaitForClientShutdown());
  DestroyChannel();
}

MULTIPROCESS_IPC_TEST_CLIENT_MAIN(ParamTraitInvalidMessagePipeClient) {
  ParamTraitMessagePipeClient(false, "ParamTraitInvalidMessagePipeClient");
  return 0;
}

TEST_F(IPCChannelMojoTest, SendFailAfterClose) {
  InitWithMojo("IPCChannelMojoTestSendOkClient");

  ListenerThatExpectsOK listener;
  CreateChannel(&listener);
  ASSERT_TRUE(ConnectChannel());
  ASSERT_TRUE(StartClient());

  base::MessageLoop::current()->Run();
  this->channel()->Close();
  ASSERT_FALSE(this->channel()->Send(new IPC::Message()));

  EXPECT_TRUE(WaitForClientShutdown());
  DestroyChannel();
}

class ListenerSendingOneOk : public IPC::Listener {
 public:
  ListenerSendingOneOk() {
  }

  bool OnMessageReceived(const IPC::Message& message) override {
    return true;
  }

  void OnChannelConnected(int32_t peer_pid) override {
    ListenerThatExpectsOK::SendOK(sender_);
    base::MessageLoop::current()->QuitWhenIdle();
  }

  void set_sender(IPC::Sender* sender) { sender_ = sender; }

 private:
  IPC::Sender* sender_;
};

MULTIPROCESS_IPC_TEST_CLIENT_MAIN(IPCChannelMojoTestSendOkClient) {
  ListenerSendingOneOk listener;
  ChannelClient client(&listener, "IPCChannelMojoTestSendOkClient");
  client.Connect();
  listener.set_sender(client.channel());

  base::MessageLoop::current()->Run();

  client.Close();

  return 0;
}

#if defined(OS_WIN)
class IPCChannelMojoDeadHandleTest : public IPCChannelMojoTestBase {
 protected:
  scoped_ptr<IPC::ChannelFactory> CreateChannelFactory(
      const IPC::ChannelHandle& handle,
      base::SequencedTaskRunner* runner) override {
    return IPC::ChannelMojo::CreateServerFactory(task_runner(), handle);
  }

  bool DidStartClient() override {
    IPCTestBase::DidStartClient();
    // const base::ProcessHandle client = client_process().Handle();
    // Forces GetFileHandleForProcess() fail. It happens occasionally
    // in production, so we should exercise it somehow.
    // TODO(morrita): figure out how to safely test this. See crbug.com/464109.
    // ::CloseHandle(client);
    return true;
  }
};

TEST_F(IPCChannelMojoDeadHandleTest, InvalidClientHandle) {
  // Any client type is fine as it is going to be killed anyway.
  InitWithMojo("IPCChannelMojoTestDoNothingClient");

  // Set up IPC channel and start client.
  ListenerExpectingErrors listener;
  CreateChannel(&listener);
  ASSERT_TRUE(ConnectChannel());

  ASSERT_TRUE(StartClient());
  base::MessageLoop::current()->Run();

  this->channel()->Close();

  // TODO(morrita): We need CloseHandle() call in DidStartClient(),
  // which has been disabled since crrev.com/843113003, to
  // make this fail. See crbug.com/464109.
  // EXPECT_FALSE(WaitForClientShutdown());
  WaitForClientShutdown();
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
    base::PickleIterator iter(message);
    HandleSendingHelper::ReadReceivedFile(message, &iter);
    base::MessageLoop::current()->QuitWhenIdle();
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

// Times out on Android; see http://crbug.com/502290
#if defined(OS_ANDROID)
#define MAYBE_SendPlatformHandle DISABLED_SendPlatformHandle
#else
#define MAYBE_SendPlatformHandle SendPlatformHandle
#endif
TEST_F(IPCChannelMojoTest, MAYBE_SendPlatformHandle) {
  InitWithMojo("IPCChannelMojoTestSendPlatformHandleClient");

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

  client.Close();

  return 0;
}

class ListenerThatExpectsFileAndPipe : public IPC::Listener {
 public:
  ListenerThatExpectsFileAndPipe() : sender_(NULL) {}

  ~ListenerThatExpectsFileAndPipe() override {}

  bool OnMessageReceived(const IPC::Message& message) override {
    base::PickleIterator iter(message);
    HandleSendingHelper::ReadReceivedFile(message, &iter);
    HandleSendingHelper::ReadReceivedPipe(message, &iter);
    base::MessageLoop::current()->QuitWhenIdle();
    ListenerThatExpectsOK::SendOK(sender_);
    return true;
  }

  void OnChannelError() override { NOTREACHED(); }

  void set_sender(IPC::Sender* sender) { sender_ = sender; }

 private:
  IPC::Sender* sender_;
};

// Times out on Android; see http://crbug.com/502290
#if defined(OS_ANDROID)
#define MAYBE_SendPlatformHandleAndPipe DISABLED_SendPlatformHandleAndPipe
#else
#define MAYBE_SendPlatformHandleAndPipe SendPlatformHandleAndPipe
#endif
TEST_F(IPCChannelMojoTest, MAYBE_SendPlatformHandleAndPipe) {
  InitWithMojo("IPCChannelMojoTestSendPlatformHandleAndPipeClient");

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

  client.Close();

  return 0;
}

#endif

#if defined(OS_LINUX)

const base::ProcessId kMagicChildId = 54321;

class ListenerThatVerifiesPeerPid : public IPC::Listener {
 public:
  void OnChannelConnected(int32_t peer_pid) override {
    EXPECT_EQ(peer_pid, kMagicChildId);
    base::MessageLoop::current()->QuitWhenIdle();
  }

  bool OnMessageReceived(const IPC::Message& message) override {
    NOTREACHED();
    return true;
  }
};

TEST_F(IPCChannelMojoTest, VerifyGlobalPid) {
  InitWithMojo("IPCChannelMojoTestVerifyGlobalPidClient");

  ListenerThatVerifiesPeerPid listener;
  CreateChannel(&listener);
  ASSERT_TRUE(ConnectChannel());
  ASSERT_TRUE(StartClient());

  base::MessageLoop::current()->Run();
  channel()->Close();

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

  client.Close();

  return 0;
}

#endif // OS_LINUX

}  // namespace
