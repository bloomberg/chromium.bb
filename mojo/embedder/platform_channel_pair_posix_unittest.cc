// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/embedder/platform_channel_pair.h"

#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include "base/logging.h"
#include "base/macros.h"
#include "mojo/embedder/platform_channel_utils_posix.h"
#include "mojo/embedder/scoped_platform_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace embedder {
namespace {

class PlatformChannelPairPosixTest : public testing::Test {
 public:
  PlatformChannelPairPosixTest() {}
  virtual ~PlatformChannelPairPosixTest() {}

  virtual void SetUp() OVERRIDE {
    // Make sure |SIGPIPE| isn't being ignored.
    struct sigaction action = {};
    action.sa_handler = SIG_DFL;
    ASSERT_EQ(0, sigaction(SIGPIPE, &action, &old_action_));
  }

  virtual void TearDown() OVERRIDE {
    // Restore the |SIGPIPE| handler.
    ASSERT_EQ(0, sigaction(SIGPIPE, &old_action_, NULL));
  }

 private:
  struct sigaction old_action_;

  DISALLOW_COPY_AND_ASSIGN(PlatformChannelPairPosixTest);
};

TEST_F(PlatformChannelPairPosixTest, NoSigPipe) {
  PlatformChannelPair channel_pair;

  ScopedPlatformHandle server_handle = channel_pair.PassServerHandle().Pass();
  ScopedPlatformHandle client_handle = channel_pair.PassClientHandle().Pass();

  // Write to the client.
  static const char kHello[] = "hello";
  EXPECT_EQ(static_cast<ssize_t>(sizeof(kHello)),
            write(client_handle.get().fd, kHello, sizeof(kHello)));

  // Close the client.
  client_handle.reset();

  // Read from the server; this should be okay.
  char buffer[100] = {};
  EXPECT_EQ(static_cast<ssize_t>(sizeof(kHello)),
            read(server_handle.get().fd, buffer, sizeof(buffer)));
  EXPECT_STREQ(kHello, buffer);

  // Try reading again.
  ssize_t result = read(server_handle.get().fd, buffer, sizeof(buffer));
  // We should probably get zero (for "end of file"), but -1 would also be okay.
  EXPECT_TRUE(result == 0 || result == -1);
  if (result == -1)
    PLOG(WARNING) << "read (expected 0 for EOF)";

  // Test our replacement for |write()|/|send()|.
  result = PlatformChannelWrite(server_handle.get(), kHello, sizeof(kHello));
  EXPECT_EQ(-1, result);
  if (errno != EPIPE)
    PLOG(WARNING) << "write (expected EPIPE)";

  // Test our replacement for |writev()|/|sendv()|.
  struct iovec iov[2] = {
    { const_cast<char*>(kHello), sizeof(kHello) },
    { const_cast<char*>(kHello), sizeof(kHello) }
  };
  result = PlatformChannelWritev(server_handle.get(), iov, 2);
  EXPECT_EQ(-1, result);
  if (errno != EPIPE)
    PLOG(WARNING) << "write (expected EPIPE)";
}

}  // namespace
}  // namespace embedder
}  // namespace mojo
