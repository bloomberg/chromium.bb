// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/embedder/platform_channel_pair.h"

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include <vector>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "mojo/embedder/platform_channel_utils_posix.h"
#include "mojo/embedder/scoped_platform_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace embedder {
namespace {

ScopedPlatformHandle PlatformHandleFromFILE(FILE* fp) {
  CHECK(fp);
  return ScopedPlatformHandle(PlatformHandle(dup(fileno(fp))));
}

FILE* FILEFromPlatformHandle(ScopedPlatformHandle h, const char* mode) {
  CHECK(h.is_valid());
  return fdopen(h.release().fd, mode);
}

void WaitReadable(PlatformHandle h) {
  struct pollfd pfds = {};
  pfds.fd = h.fd;
  pfds.events = POLLIN;
  CHECK_EQ(poll(&pfds, 1, -1), 1);
}

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

TEST_F(PlatformChannelPairPosixTest, SendReceiveData) {
  PlatformChannelPair channel_pair;
  ScopedPlatformHandle server_handle = channel_pair.PassServerHandle().Pass();
  ScopedPlatformHandle client_handle = channel_pair.PassClientHandle().Pass();

  for (size_t i = 0; i < 10; i++) {
    std::string send_string(1 << i, 'A' + i);

    EXPECT_EQ(static_cast<ssize_t>(send_string.size()),
              PlatformChannelWrite(server_handle.get(), send_string.data(),
                                   send_string.size()));

    WaitReadable(client_handle.get());

    char buf[10000] = {};
    scoped_ptr<PlatformHandleVector> received_handles;
    ssize_t result = PlatformChannelRecvmsg(client_handle.get(), buf,
                                            sizeof(buf), &received_handles);
    EXPECT_EQ(static_cast<ssize_t>(send_string.size()), result);
    EXPECT_EQ(send_string, std::string(buf, static_cast<size_t>(result)));
    EXPECT_FALSE(received_handles);
  }
}

TEST_F(PlatformChannelPairPosixTest, SendReceiveFDs) {
  PlatformChannelPair channel_pair;
  ScopedPlatformHandle server_handle = channel_pair.PassServerHandle().Pass();
  ScopedPlatformHandle client_handle = channel_pair.PassClientHandle().Pass();

  for (size_t i = 1; i < kPlatformChannelMaxNumHandles; i++) {
    // Make |i| files, with the j-th file consisting of j copies of the digit i.
    PlatformHandleVector platform_handles;
    for (size_t j = 1; j <= i; j++) {
      base::FilePath ignored;
      FILE* fp = base::CreateAndOpenTemporaryFile(&ignored);
      ASSERT_TRUE(fp);
      platform_handles.push_back(PlatformHandleFromFILE(fp).release());
      ASSERT_TRUE(platform_handles.back().is_valid());
      fwrite(std::string(j, '0' + i).data(), 1, j, fp);
      fclose(fp);
    }

    // Send the FDs.
    EXPECT_TRUE(PlatformChannelSendHandles(server_handle.get(),
                                           &platform_handles[0],
                                           platform_handles.size()));

    WaitReadable(client_handle.get());

    char buf[100] = { 'a' };
    scoped_ptr<PlatformHandleVector> received_handles;
    EXPECT_EQ(1, PlatformChannelRecvmsg(client_handle.get(), buf, sizeof(buf),
                                        &received_handles));
    EXPECT_EQ('\0', buf[0]);
    ASSERT_TRUE(received_handles);
    EXPECT_EQ(i, received_handles->size());

    for (size_t j = 0; j < received_handles->size(); j++) {
      FILE* fp = FILEFromPlatformHandle(
          ScopedPlatformHandle((*received_handles)[j]), "rb");
      (*received_handles)[j] = PlatformHandle();
      ASSERT_TRUE(fp);
      rewind(fp);
      char read_buf[100];
      size_t bytes_read = fread(read_buf, 1, sizeof(read_buf), fp);
      fclose(fp);
      EXPECT_EQ(j + 1, bytes_read);
      EXPECT_EQ(std::string(j + 1, '0' + i), std::string(read_buf, bytes_read));
    }
  }
}

TEST_F(PlatformChannelPairPosixTest, AppendReceivedFDs) {
  PlatformChannelPair channel_pair;
  ScopedPlatformHandle server_handle = channel_pair.PassServerHandle().Pass();
  ScopedPlatformHandle client_handle = channel_pair.PassClientHandle().Pass();

  const std::string file_contents("hello world");

  {
    base::FilePath ignored;
    FILE* fp = base::CreateAndOpenTemporaryFile(&ignored);
    ASSERT_TRUE(fp);
    PlatformHandleVector platform_handles;
    platform_handles.push_back(PlatformHandleFromFILE(fp).release());
    ASSERT_TRUE(platform_handles.back().is_valid());
    fwrite(file_contents.data(), 1, file_contents.size(), fp);
    fclose(fp);

    // Send the FD.
    EXPECT_TRUE(PlatformChannelSendHandles(server_handle.get(),
                                           &platform_handles[0],
                                           platform_handles.size()));
  }

  WaitReadable(client_handle.get());

  // Start with an invalid handle in the vector.
  scoped_ptr<PlatformHandleVector> handles(new PlatformHandleVector());
  handles->push_back(PlatformHandle());

  char buf[100] = { 'a' };
  EXPECT_EQ(1, PlatformChannelRecvmsg(client_handle.get(), buf, sizeof(buf),
                                      &handles));
  EXPECT_EQ('\0', buf[0]);
  ASSERT_TRUE(handles);
  ASSERT_EQ(2u, handles->size());
  EXPECT_FALSE((*handles)[0].is_valid());
  EXPECT_TRUE((*handles)[1].is_valid());

  {
    FILE* fp = FILEFromPlatformHandle(ScopedPlatformHandle((*handles)[1]),
                                      "rb");
    (*handles)[1] = PlatformHandle();
    ASSERT_TRUE(fp);
    rewind(fp);
    char read_buf[100];
    size_t bytes_read = fread(read_buf, 1, sizeof(read_buf), fp);
    fclose(fp);
    EXPECT_EQ(file_contents.size(), bytes_read);
    EXPECT_EQ(file_contents, std::string(read_buf, bytes_read));
  }
}

}  // namespace
}  // namespace embedder
}  // namespace mojo
