// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These tests are POSIX only.

#include "ipc/ipc_channel_posix.h"

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "base/basictypes.h"
#include "base/eintr_wrapper.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "testing/multiprocess_func_list.h"

namespace {

enum {
  QUIT_MESSAGE = 47
};

class IPCChannelPosixTestListener : public IPC::Channel::Listener {
 public:
  enum STATUS {
    DISCONNECTED,
    MESSAGE_RECEIVED,
    CHANNEL_ERROR,
    CONNECTED,
    DENIED,
    LISTEN_ERROR
  };

  IPCChannelPosixTestListener(bool quit_only_on_message)
      : status_(DISCONNECTED), quit_only_on_message_(quit_only_on_message) {}

  virtual ~IPCChannelPosixTestListener() {}

  virtual bool OnMessageReceived(const IPC::Message& message) {
    EXPECT_EQ(message.type(), QUIT_MESSAGE);
    status_ = MESSAGE_RECEIVED;
    QuitRunLoop();
    return true;
  }

  virtual void OnChannelConnected(int32 peer_pid) {
    status_ = CONNECTED;
    if (!quit_only_on_message_) {
      QuitRunLoop();
    }
  }

  virtual void OnChannelError()  {
    status_ = CHANNEL_ERROR;
    if (!quit_only_on_message_) {
      QuitRunLoop();
    }
  }

  virtual void OnChannelDenied() {
    status_ = DENIED;
    if (!quit_only_on_message_) {
      QuitRunLoop();
    }
  }

  virtual void OnChannelListenError() {
    status_ = LISTEN_ERROR;
    if (!quit_only_on_message_) {
      QuitRunLoop();
    }
  }

  STATUS status() { return status_; }

  void QuitRunLoop() {
    MessageLoopForIO::current()->QuitNow();
  }

 private:
  // The current status of the listener.
  STATUS status_;
  // If |quit_only_on_message_| then the listener will only break out of
  // the run loop when the QUIT_MESSAGE is received.
  bool quit_only_on_message_;
};

}  // namespace

class IPCChannelPosixTest : public base::MultiProcessTest {
 public:
  static const char kConnectionSocketTestName[];
  static void SetUpSocket(IPC::ChannelHandle *handle,
                          IPC::Channel::Mode mode);
  static void SpinRunLoop(int milliseconds);

 protected:
  virtual void SetUp();
  virtual void TearDown();

private:
  scoped_ptr<MessageLoopForIO> message_loop_;
};

const char IPCChannelPosixTest::kConnectionSocketTestName[] =
    "/var/tmp/chrome_IPCChannelPosixTest__ConnectionSocket";

void IPCChannelPosixTest::SetUp() {
  MultiProcessTest::SetUp();
  // Construct a fresh IO Message loop for the duration of each test.
  message_loop_.reset(new MessageLoopForIO());
}

void IPCChannelPosixTest::TearDown() {
  message_loop_.reset(NULL);
  MultiProcessTest::TearDown();
}

// Create up a socket and bind and listen to it, or connect it
// depending on the |mode|.
void IPCChannelPosixTest::SetUpSocket(IPC::ChannelHandle *handle,
                                      IPC::Channel::Mode mode) {
  const std::string& name = handle->name;

  int socket_fd = socket(PF_UNIX, SOCK_STREAM, 0);
  ASSERT_GE(socket_fd, 0) << name;
  ASSERT_GE(fcntl(socket_fd, F_SETFL, O_NONBLOCK), 0);
  struct sockaddr_un server_address = { 0 };
  memset(&server_address, 0, sizeof(server_address));
  server_address.sun_family = AF_UNIX;
  int path_len = snprintf(server_address.sun_path, IPC::kMaxPipeNameLength,
                          "%s", name.c_str());
  DCHECK_EQ(static_cast<int>(name.length()), path_len);
  size_t server_address_len = offsetof(struct sockaddr_un,
                                       sun_path) + path_len + 1;

  if (mode == IPC::Channel::MODE_NAMED_SERVER) {
    // Only one server at a time. Cleanup garbage if it exists.
    unlink(name.c_str());
      // Make sure the path we need exists.
    FilePath path(name);
    FilePath dir_path = path.DirName();
    ASSERT_TRUE(file_util::CreateDirectory(dir_path));
    ASSERT_GE(bind(socket_fd,
                   reinterpret_cast<struct sockaddr *>(&server_address),
                   server_address_len), 0) << server_address.sun_path
                                           << ": " << strerror(errno)
                                           << "(" << errno << ")";
    ASSERT_GE(listen(socket_fd, SOMAXCONN), 0) << server_address.sun_path
                                               << ": " << strerror(errno)
                                               << "(" << errno << ")";
  } else if (mode == IPC::Channel::MODE_NAMED_CLIENT) {
    ASSERT_GE(connect(socket_fd,
                      reinterpret_cast<struct sockaddr *>(&server_address),
                      server_address_len), 0) << server_address.sun_path
                                              << ": " << strerror(errno)
                                              << "(" << errno << ")";
  } else {
    FAIL() << "Unknown mode " << mode;
  }
  handle->socket.fd = socket_fd;
}

void IPCChannelPosixTest::SpinRunLoop(int milliseconds) {
  MessageLoopForIO *loop = MessageLoopForIO::current();
  // Post a quit task so that this loop eventually ends and we don't hang
  // in the case of a bad test. Usually, the run loop will quit sooner than
  // that because all tests use a IPCChannelPosixTestListener which quits the
  // current run loop on any channel activity.
  loop->PostDelayedTask(FROM_HERE, new MessageLoop::QuitTask(), milliseconds);
  loop->Run();
}

TEST_F(IPCChannelPosixTest, BasicListen) {
  // Test creating a socket that is listening.
  IPC::ChannelHandle handle("/var/tmp/IPCChannelPosixTest_BasicListen");
  SetUpSocket(&handle, IPC::Channel::MODE_NAMED_SERVER);
  unlink(handle.name.c_str());
  IPC::Channel channel(handle, IPC::Channel::MODE_NAMED_SERVER, NULL);
  ASSERT_TRUE(channel.Connect());
  ASSERT_TRUE(channel.AcceptsConnections());
  ASSERT_FALSE(channel.HasAcceptedConnection());
  channel.ResetToAcceptingConnectionState();
  ASSERT_FALSE(channel.HasAcceptedConnection());
}

TEST_F(IPCChannelPosixTest, BasicConnected) {
  // Test creating a socket that is connected.
  int pipe_fds[2];
  ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, pipe_fds));
  std::string socket_name("/var/tmp/IPCChannelPosixTest_BasicConnected");
  ASSERT_GE(fcntl(pipe_fds[0], F_SETFL, O_NONBLOCK), 0);

  base::FileDescriptor fd(pipe_fds[0], false);
  IPC::ChannelHandle handle(socket_name, fd);
  IPC::Channel channel(handle, IPC::Channel::MODE_SERVER, NULL);
  ASSERT_TRUE(channel.Connect());
  ASSERT_FALSE(channel.AcceptsConnections());
  channel.Close();
  ASSERT_TRUE(HANDLE_EINTR(close(pipe_fds[1])) == 0);

  // Make sure that we can use the socket that is created for us by
  // a standard channel.
  IPC::Channel channel2(socket_name, IPC::Channel::MODE_SERVER, NULL);
  ASSERT_TRUE(channel2.Connect());
  ASSERT_FALSE(channel2.AcceptsConnections());
}

TEST_F(IPCChannelPosixTest, AdvancedConnected) {
  // Test creating a connection to an external process.
  IPCChannelPosixTestListener listener(false);
  IPC::ChannelHandle chan_handle(kConnectionSocketTestName);
  SetUpSocket(&chan_handle, IPC::Channel::MODE_NAMED_SERVER);
  IPC::Channel channel(chan_handle, IPC::Channel::MODE_NAMED_SERVER, &listener);
  ASSERT_TRUE(channel.Connect());
  ASSERT_TRUE(channel.AcceptsConnections());
  ASSERT_FALSE(channel.HasAcceptedConnection());

  base::ProcessHandle handle = SpawnChild("IPCChannelPosixTestConnectionProc",
                                          false);
  ASSERT_TRUE(handle);
  SpinRunLoop(TestTimeouts::action_max_timeout_ms());
  ASSERT_EQ(IPCChannelPosixTestListener::CONNECTED, listener.status());
  ASSERT_TRUE(channel.HasAcceptedConnection());
  IPC::Message* message = new IPC::Message(0, // routing_id
                                           QUIT_MESSAGE, // message type
                                           IPC::Message::PRIORITY_NORMAL);
  channel.Send(message);
  SpinRunLoop(TestTimeouts::action_timeout_ms());
  int exit_code = 0;
  EXPECT_TRUE(base::WaitForExitCode(handle, &exit_code));
  EXPECT_EQ(0, exit_code);
  ASSERT_EQ(IPCChannelPosixTestListener::CHANNEL_ERROR, listener.status());
  ASSERT_FALSE(channel.HasAcceptedConnection());
}

TEST_F(IPCChannelPosixTest, ResetState) {
  // Test creating a connection to an external process. Close the connection,
  // but continue to listen and make sure another external process can connect
  // to us.
  IPCChannelPosixTestListener listener(false);
  IPC::ChannelHandle chan_handle(kConnectionSocketTestName);
  SetUpSocket(&chan_handle, IPC::Channel::MODE_NAMED_SERVER);
  IPC::Channel channel(chan_handle, IPC::Channel::MODE_NAMED_SERVER, &listener);
  ASSERT_TRUE(channel.Connect());
  ASSERT_TRUE(channel.AcceptsConnections());
  ASSERT_FALSE(channel.HasAcceptedConnection());

  base::ProcessHandle handle = SpawnChild("IPCChannelPosixTestConnectionProc",
                                          false);
  ASSERT_TRUE(handle);
  SpinRunLoop(TestTimeouts::action_max_timeout_ms());
  ASSERT_EQ(IPCChannelPosixTestListener::CONNECTED, listener.status());
  ASSERT_TRUE(channel.HasAcceptedConnection());
  channel.ResetToAcceptingConnectionState();
  ASSERT_FALSE(channel.HasAcceptedConnection());

  base::ProcessHandle handle2 = SpawnChild("IPCChannelPosixTestConnectionProc",
                                          false);
  ASSERT_TRUE(handle2);
  SpinRunLoop(TestTimeouts::action_max_timeout_ms());
  ASSERT_EQ(IPCChannelPosixTestListener::CONNECTED, listener.status());
  ASSERT_TRUE(channel.HasAcceptedConnection());
  IPC::Message* message = new IPC::Message(0, // routing_id
                                           QUIT_MESSAGE, // message type
                                           IPC::Message::PRIORITY_NORMAL);
  channel.Send(message);
  SpinRunLoop(TestTimeouts::action_timeout_ms());
  EXPECT_TRUE(base::KillProcess(handle, 0, false));
  int exit_code = 0;
  EXPECT_TRUE(base::WaitForExitCode(handle2, &exit_code));
  EXPECT_EQ(0, exit_code);
  ASSERT_EQ(IPCChannelPosixTestListener::CHANNEL_ERROR, listener.status());
  ASSERT_FALSE(channel.HasAcceptedConnection());
}

TEST_F(IPCChannelPosixTest, MultiConnection) {
  // Test setting up a connection to an external process, and then have
  // another external process attempt to connect to us.
  IPCChannelPosixTestListener listener(false);
  IPC::ChannelHandle chan_handle(kConnectionSocketTestName);
  SetUpSocket(&chan_handle, IPC::Channel::MODE_NAMED_SERVER);
  IPC::Channel channel(chan_handle, IPC::Channel::MODE_NAMED_SERVER, &listener);
  ASSERT_TRUE(channel.Connect());
  ASSERT_TRUE(channel.AcceptsConnections());
  ASSERT_FALSE(channel.HasAcceptedConnection());

  base::ProcessHandle handle = SpawnChild("IPCChannelPosixTestConnectionProc",
                                          false);
  ASSERT_TRUE(handle);
  SpinRunLoop(TestTimeouts::action_max_timeout_ms());
  ASSERT_EQ(IPCChannelPosixTestListener::CONNECTED, listener.status());
  ASSERT_TRUE(channel.HasAcceptedConnection());
  base::ProcessHandle handle2 = SpawnChild("IPCChannelPosixFailConnectionProc",
                                           false);
  ASSERT_TRUE(handle2);
  SpinRunLoop(TestTimeouts::action_max_timeout_ms());
  int exit_code = 0;
  EXPECT_TRUE(base::WaitForExitCode(handle2, &exit_code));
  EXPECT_EQ(exit_code, 0);
  ASSERT_EQ(IPCChannelPosixTestListener::DENIED, listener.status());
  ASSERT_TRUE(channel.HasAcceptedConnection());
  IPC::Message* message = new IPC::Message(0, // routing_id
                                           QUIT_MESSAGE, // message type
                                           IPC::Message::PRIORITY_NORMAL);
  channel.Send(message);
  SpinRunLoop(TestTimeouts::action_timeout_ms());
  EXPECT_TRUE(base::WaitForExitCode(handle, &exit_code));
  EXPECT_EQ(exit_code, 0);
  ASSERT_EQ(IPCChannelPosixTestListener::CHANNEL_ERROR, listener.status());
  ASSERT_FALSE(channel.HasAcceptedConnection());
}

TEST_F(IPCChannelPosixTest, DoubleServer) {
  // Test setting up two servers with the same name.
  IPCChannelPosixTestListener listener(false);
  IPCChannelPosixTestListener listener2(false);
  IPC::ChannelHandle chan_handle(kConnectionSocketTestName);
  IPC::Channel channel(chan_handle, IPC::Channel::MODE_SERVER, &listener);
  IPC::Channel channel2(chan_handle, IPC::Channel::MODE_SERVER, &listener2);
  ASSERT_TRUE(channel.Connect());
  ASSERT_FALSE(channel2.Connect());
}

TEST_F(IPCChannelPosixTest, BadMode) {
  // Test setting up two servers with a bad mode.
  IPCChannelPosixTestListener listener(false);
  IPC::ChannelHandle chan_handle(kConnectionSocketTestName);
  IPC::Channel channel(chan_handle, IPC::Channel::MODE_NONE, &listener);
  ASSERT_FALSE(channel.Connect());
}

// A long running process that connects to us
MULTIPROCESS_TEST_MAIN(IPCChannelPosixTestConnectionProc) {
  MessageLoopForIO message_loop;
  IPCChannelPosixTestListener listener(true);
  IPC::ChannelHandle handle(IPCChannelPosixTest::kConnectionSocketTestName);
  IPCChannelPosixTest::SetUpSocket(&handle, IPC::Channel::MODE_NAMED_CLIENT);
  IPC::Channel channel(handle, IPC::Channel::MODE_NAMED_CLIENT, &listener);
  EXPECT_TRUE(channel.Connect());
  IPCChannelPosixTest::SpinRunLoop(TestTimeouts::action_max_timeout_ms());
  EXPECT_EQ(IPCChannelPosixTestListener::MESSAGE_RECEIVED, listener.status());
  return 0;
}

// Simple external process that shouldn't be able to connect to us.
MULTIPROCESS_TEST_MAIN(IPCChannelPosixFailConnectionProc) {
  MessageLoopForIO message_loop;
  IPCChannelPosixTestListener listener(false);
  IPC::ChannelHandle handle(IPCChannelPosixTest::kConnectionSocketTestName);
  IPCChannelPosixTest::SetUpSocket(&handle, IPC::Channel::MODE_NAMED_CLIENT);
  IPC::Channel channel(handle, IPC::Channel::MODE_NAMED_CLIENT, &listener);

  // In this case connect may succeed or fail depending on if the packet
  // actually gets sent at sendmsg. Since we never delay on send, we may not
  // see the error. However even if connect succeeds, eventually we will get an
  // error back since the channel will be closed when we attempt to read from
  // it.
  bool connected = channel.Connect();
  if (connected) {
    IPCChannelPosixTest::SpinRunLoop(TestTimeouts::action_max_timeout_ms());
    EXPECT_EQ(IPCChannelPosixTestListener::CHANNEL_ERROR, listener.status());
  } else {
    EXPECT_EQ(IPCChannelPosixTestListener::DISCONNECTED, listener.status());
  }
  return 0;
}

