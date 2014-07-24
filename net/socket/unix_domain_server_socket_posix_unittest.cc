// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/unix_domain_server_socket_posix.h"

#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/socket/unix_domain_client_socket_posix.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

const char kSocketFilename[] = "socket_for_testing";
const char kInvalidSocketPath[] = "/invalid/path";

bool UserCanConnectCallback(bool allow_user, uid_t uid, gid_t gid) {
  // Here peers are running in same process.
  EXPECT_EQ(getuid(), uid);
  EXPECT_EQ(getgid(), gid);
  return allow_user;
}

UnixDomainServerSocket::AuthCallback CreateAuthCallback(bool allow_user) {
  return base::Bind(&UserCanConnectCallback, allow_user);
}

class UnixDomainServerSocketTest : public testing::Test {
 protected:
  UnixDomainServerSocketTest() {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    socket_path_ = temp_dir_.path().Append(kSocketFilename).value();
  }

  base::ScopedTempDir temp_dir_;
  std::string socket_path_;
};

TEST_F(UnixDomainServerSocketTest, ListenWithInvalidPath) {
  const bool kUseAbstractNamespace = false;
  UnixDomainServerSocket server_socket(CreateAuthCallback(true),
                                       kUseAbstractNamespace);
  EXPECT_EQ(ERR_FILE_NOT_FOUND,
            server_socket.ListenWithAddressAndPort(kInvalidSocketPath, 0, 1));
}

TEST_F(UnixDomainServerSocketTest, ListenWithInvalidPathWithAbstractNamespace) {
  const bool kUseAbstractNamespace = true;
  UnixDomainServerSocket server_socket(CreateAuthCallback(true),
                                       kUseAbstractNamespace);
#if defined(OS_ANDROID) || defined(OS_LINUX)
  EXPECT_EQ(OK,
            server_socket.ListenWithAddressAndPort(kInvalidSocketPath, 0, 1));
#else
  EXPECT_EQ(ERR_ADDRESS_INVALID,
            server_socket.ListenWithAddressAndPort(kInvalidSocketPath, 0, 1));
#endif
}

TEST_F(UnixDomainServerSocketTest, AcceptWithForbiddenUser) {
  const bool kUseAbstractNamespace = false;

  UnixDomainServerSocket server_socket(CreateAuthCallback(false),
                                       kUseAbstractNamespace);
  EXPECT_EQ(OK, server_socket.ListenWithAddressAndPort(socket_path_, 0, 1));

  scoped_ptr<StreamSocket> accepted_socket;
  TestCompletionCallback accept_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            server_socket.Accept(&accepted_socket, accept_callback.callback()));
  EXPECT_FALSE(accepted_socket);

  UnixDomainClientSocket client_socket(socket_path_, kUseAbstractNamespace);
  EXPECT_FALSE(client_socket.IsConnected());

  // Connect() will return OK before the server rejects the connection.
  TestCompletionCallback connect_callback;
  int rv = client_socket.Connect(connect_callback.callback());
  if (rv == ERR_IO_PENDING) {
    rv = connect_callback.WaitForResult();
  } else {
    EXPECT_TRUE(client_socket.IsConnected());
  }
  EXPECT_EQ(OK, rv);

  // Cannot use accept_callback.WaitForResult() because authentication error is
  // invisible to the caller.
  base::RunLoop().RunUntilIdle();
  // Server disconnects the connection.
  EXPECT_FALSE(client_socket.IsConnected());
  // But, server didn't create the accepted socket.
  EXPECT_FALSE(accepted_socket);

  const int read_buffer_size = 10;
  scoped_refptr<IOBuffer> read_buffer(new IOBuffer(read_buffer_size));
  TestCompletionCallback read_callback;
  EXPECT_EQ(0,  /* EOF */
            client_socket.Read(read_buffer, read_buffer_size,
                               read_callback.callback()));
}

// Normal cases including read/write are tested by UnixDomainClientSocketTest.

}  // namespace
}  // namespace net
