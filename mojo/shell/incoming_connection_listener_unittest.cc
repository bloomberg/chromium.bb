// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "mojo/embedder/channel_init.h"
#include "mojo/embedder/platform_channel_pair.h"
#include "mojo/shell/external_application_registrar_connection.h"
#include "mojo/shell/incoming_connection_listener_posix.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/socket/socket_descriptor.h"
#include "net/socket/unix_domain_client_socket_posix.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace shell {
namespace {
// Delegate implementation that expects success.
class TestDelegate : public IncomingConnectionListenerPosix::Delegate {
 public:
  TestDelegate() {}
  virtual ~TestDelegate() {}

  virtual void OnListening(int rv) override { EXPECT_EQ(net::OK, rv); }
  virtual void OnConnection(net::SocketDescriptor incoming) override {
    EXPECT_NE(net::kInvalidSocket, incoming);
  }
};

// Delegate implementation that expects a (configurable) failure to listen.
class ListeningFailsDelegate
    : public IncomingConnectionListenerPosix::Delegate {
 public:
  explicit ListeningFailsDelegate(int expected) : expected_error_(expected) {}
  virtual ~ListeningFailsDelegate() {}

  virtual void OnListening(int rv) override { EXPECT_EQ(expected_error_, rv); }
  virtual void OnConnection(net::SocketDescriptor incoming) override {
    FAIL() << "No connection should be attempted.";
  }

 private:
  const int expected_error_;
};

// For ExternalApplicationRegistrarConnection::Connect() callbacks.
void OnConnect(base::Closure quit_callback, int rv) {
  EXPECT_EQ(net::OK, rv);
  base::MessageLoop::current()->PostTask(FROM_HERE, quit_callback);
}
}  // namespace

class IncomingConnectionListenerTest : public testing::Test {
 public:
  IncomingConnectionListenerTest() {}
  virtual ~IncomingConnectionListenerTest() {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    socket_path_ = temp_dir_.path().Append(FILE_PATH_LITERAL("socket"));
  }

 protected:
  base::MessageLoopForIO loop_;
  base::RunLoop run_loop_;

  base::ScopedTempDir temp_dir_;
  base::FilePath socket_path_;
};

TEST_F(IncomingConnectionListenerTest, CleanupCheck) {
  TestDelegate delegate;
  {
    IncomingConnectionListenerPosix cleanup_check(socket_path_, &delegate);
    cleanup_check.StartListening();
    ASSERT_TRUE(base::PathExists(socket_path_));
  }
  ASSERT_FALSE(base::PathExists(socket_path_));
}

TEST_F(IncomingConnectionListenerTest, ConnectSuccess) {
  TestDelegate delegate;
  IncomingConnectionListenerPosix listener(socket_path_, &delegate);

  ASSERT_FALSE(base::PathExists(socket_path_));
  listener.StartListening();
  ASSERT_TRUE(base::PathExists(socket_path_));

  ExternalApplicationRegistrarConnection connection(socket_path_);
  connection.Connect(base::Bind(&OnConnect, run_loop_.QuitClosure()));

  run_loop_.Run();
}

TEST_F(IncomingConnectionListenerTest, ConnectFails_SocketFileExists) {
  ListeningFailsDelegate fail_delegate(net::ERR_FILE_EXISTS);
  IncomingConnectionListenerPosix listener(socket_path_, &fail_delegate);

  ASSERT_EQ(1, base::WriteFile(socket_path_, "1", 1));
  ASSERT_TRUE(base::PathExists(socket_path_));

  // The listener should fail to start up.
  listener.StartListening();
}

TEST_F(IncomingConnectionListenerTest, ConnectFails_SocketDirNonexistent) {
  base::FilePath nonexistent_dir(temp_dir_.path().Append("dir").Append("file"));

  ListeningFailsDelegate fail_delegate(net::ERR_FILE_NOT_FOUND);
  IncomingConnectionListenerPosix listener(nonexistent_dir, &fail_delegate);

  // The listener should fail to start up.
  listener.StartListening();
}

}  // namespace shell
}  // namespace mojo
