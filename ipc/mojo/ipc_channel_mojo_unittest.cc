// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/mojo/ipc_channel_mojo.h"

#include "base/base_paths.h"
#include "base/files/file.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/pickle.h"
#include "base/threading/thread.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_test_base.h"
#include "ipc/ipc_test_channel_listener.h"
#include "ipc/mojo/ipc_channel_mojo_readers.h"

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif

namespace {

class ListenerThatExpectsOK : public IPC::Listener {
 public:
  ListenerThatExpectsOK()
      : received_ok_(false) {}

  virtual ~ListenerThatExpectsOK() {}

  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE {
    PickleIterator iter(message);
    std::string should_be_ok;
    EXPECT_TRUE(iter.ReadString(&should_be_ok));
    EXPECT_EQ(should_be_ok, "OK");
    received_ok_ = true;
    base::MessageLoop::current()->Quit();
    return true;
  }

  virtual void OnChannelError() OVERRIDE {
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
    channel_ = IPC::ChannelMojo::Create(
        IPCTestBase::GetChannelName(name), IPC::Channel::MODE_CLIENT, listener,
        main_message_loop_.message_loop_proxy());
  }

  void Connect() {
    CHECK(channel_->Connect());
  }

  IPC::ChannelMojo* channel() const { return channel_.get(); }

 private:
  scoped_ptr<IPC::ChannelMojo> channel_;
  base::MessageLoopForIO main_message_loop_;
};

class IPCChannelMojoTest : public IPCTestBase {
 protected:
  virtual scoped_ptr<IPC::ChannelFactory> CreateChannelFactory(
      const IPC::ChannelHandle& handle,
      base::TaskRunner* runner) OVERRIDE {
    return IPC::ChannelMojo::CreateFactory(
        handle, IPC::Channel::MODE_SERVER, runner);
  }
};


class TestChannelListenerWithExtraExpectations
    : public IPC::TestChannelListener {
 public:
  TestChannelListenerWithExtraExpectations()
      : is_connected_called_(false) {
  }

  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE {
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

// Close given handle before use to simulate an error.
class ErraticChannelMojo : public IPC::ChannelMojo {
 public:
  ErraticChannelMojo(
      const IPC::ChannelHandle& channel_handle,
      IPC::Channel::Mode mode,
      IPC::Listener* listener,
      scoped_refptr<base::TaskRunner> runner)
      : ChannelMojo(channel_handle, mode, listener, runner) {
  }

  virtual void OnConnected(mojo::ScopedMessagePipeHandle pipe) {
    MojoClose(pipe.get().value());
    OnConnected(pipe.Pass());
  }
};

// Exists to create ErraticChannelMojo.
class ErraticChannelFactory : public IPC::ChannelFactory {
 public:
  explicit ErraticChannelFactory(
      const IPC::ChannelHandle& handle,
      base::TaskRunner* runner)
      : handle_(handle), runner_(runner) {
  }

  virtual std::string GetName() const OVERRIDE {
    return "";
  }

  virtual scoped_ptr<IPC::Channel> BuildChannel(
      IPC::Listener* listener) OVERRIDE {
    return scoped_ptr<IPC::Channel>(
        new ErraticChannelMojo(
            handle_, IPC::Channel::MODE_SERVER, listener, runner_));
  }

 private:
  IPC::ChannelHandle handle_;
  scoped_refptr<base::TaskRunner> runner_;
};

class ListenerExpectingErrors : public IPC::Listener {
 public:
  ListenerExpectingErrors()
      : has_error_(false) {
  }

  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE {
    return true;
  }

  virtual void OnChannelError() OVERRIDE {
    has_error_ = true;
    base::MessageLoop::current()->Quit();
  }

  bool has_error() const { return has_error_; }

 private:
  bool has_error_;
};


class IPCChannelMojoErrorTest : public IPCTestBase {
 protected:
  virtual scoped_ptr<IPC::ChannelFactory> CreateChannelFactory(
      const IPC::ChannelHandle& handle,
      base::TaskRunner* runner) OVERRIDE {
    return scoped_ptr<IPC::ChannelFactory>(
        new ErraticChannelFactory(handle, runner));
  }
};

class ListenerThatQuits : public IPC::Listener {
 public:
  ListenerThatQuits() {
  }

  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE {
    return true;
  }

  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE {
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

  // This messages are queued as pending.
  for (size_t i = 0; i < 2; ++i) {
    IPC::TestChannelListener::SendOneMessage(
        sender(), "hello from parent");
  }

  ASSERT_TRUE(StartClient());
  base::MessageLoop::current()->Run();

  this->channel()->Close();

  EXPECT_TRUE(WaitForClientShutdown());
  EXPECT_TRUE(listener.has_error());

  DestroyChannel();
}


#if defined(OS_POSIX)
class ListenerThatExpectsFile : public IPC::Listener {
 public:
  ListenerThatExpectsFile()
      : sender_(NULL) {}

  virtual ~ListenerThatExpectsFile() {}

  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE {
    PickleIterator iter(message);
    base::FileDescriptor desc;
    EXPECT_TRUE(message.ReadFileDescriptor(&iter, &desc));
    std::string content(GetSendingFileContent().size(), ' ');
    base::File file(desc.fd);
    file.Read(0, &content[0], content.size());
    EXPECT_EQ(content, GetSendingFileContent());
    base::MessageLoop::current()->Quit();
    ListenerThatExpectsOK::SendOK(sender_);
    return true;
  }

  virtual void OnChannelError() OVERRIDE {
    NOTREACHED();
  }

  static std::string GetSendingFileContent() {
    return "Hello";
  }

  static base::FilePath GetSendingFilePath() {
    base::FilePath path;
    bool ok = PathService::Get(base::DIR_CACHE, &path);
    EXPECT_TRUE(ok);
    return path.Append("ListenerThatExpectsFile.txt");
  }

  static void WriteAndSendFile(IPC::Sender* sender, base::File& file) {
    std::string content = GetSendingFileContent();
    file.WriteAtCurrentPos(content.data(), content.size());
    file.Flush();
    IPC::Message* message = new IPC::Message(
        0, 2, IPC::Message::PRIORITY_NORMAL);
    message->WriteFileDescriptor(
        base::FileDescriptor(file.TakePlatformFile(), false));
    ASSERT_TRUE(sender->Send(message));
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

  base::File file(ListenerThatExpectsFile::GetSendingFilePath(),
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE |
                  base::File::FLAG_READ);
  ListenerThatExpectsFile::WriteAndSendFile(channel(), file);
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
#endif

}  // namespace
