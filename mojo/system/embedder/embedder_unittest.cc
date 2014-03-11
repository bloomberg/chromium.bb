// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/embedder/embedder.h"

#include <string.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "mojo/public/system/core.h"
#include "mojo/system/embedder/platform_channel_pair.h"
#include "mojo/system/embedder/test_embedder.h"
#include "mojo/system/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace embedder {
namespace {

class EmbedderTest : public testing::Test {
 public:
  EmbedderTest() : io_thread_(system::test::TestIOThread::kAutoStart) {}
  virtual ~EmbedderTest() {}

 protected:
  system::test::TestIOThread* io_thread() { return &io_thread_; }

 private:
  system::test::TestIOThread io_thread_;

  DISALLOW_COPY_AND_ASSIGN(EmbedderTest);
};

void StoreChannelInfo(ChannelInfo** store_channel_info_here,
                      ChannelInfo* channel_info) {
  CHECK(store_channel_info_here);
  CHECK(channel_info);
  *store_channel_info_here = channel_info;
}

TEST_F(EmbedderTest, ChannelsBasic) {
  Init();

  PlatformChannelPair channel_pair;
  ScopedPlatformHandle server_handle = channel_pair.PassServerHandle();
  ScopedPlatformHandle client_handle = channel_pair.PassClientHandle();

  ChannelInfo* server_channel_info = NULL;
  MojoHandle server_mp = CreateChannel(server_handle.Pass(),
                                       io_thread()->task_runner(),
                                       base::Bind(&StoreChannelInfo,
                                                  &server_channel_info));
  EXPECT_NE(server_mp, MOJO_HANDLE_INVALID);

  ChannelInfo* client_channel_info = NULL;
  MojoHandle client_mp = CreateChannel(client_handle.Pass(),
                                       io_thread()->task_runner(),
                                       base::Bind(&StoreChannelInfo,
                                                  &client_channel_info));
  EXPECT_NE(client_mp, MOJO_HANDLE_INVALID);

  // We can write to a message pipe handle immediately.
  const char kHello[] = "hello";
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoWriteMessage(server_mp, kHello,
                             static_cast<uint32_t>(sizeof(kHello)), NULL, 0,
                             MOJO_WRITE_MESSAGE_FLAG_NONE));

  // Now wait for the other side to become readable.
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoWait(client_mp, MOJO_WAIT_FLAG_READABLE,
                     MOJO_DEADLINE_INDEFINITE));

  char buffer[1000] = {};
  uint32_t num_bytes = static_cast<uint32_t>(sizeof(buffer));
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoReadMessage(client_mp, buffer, &num_bytes, NULL, NULL,
                            MOJO_READ_MESSAGE_FLAG_NONE));
  EXPECT_EQ(sizeof(kHello), num_bytes);
  EXPECT_STREQ(kHello, buffer);

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(server_mp));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(client_mp));

  EXPECT_TRUE(server_channel_info != NULL);
  system::test::PostTaskAndWait(io_thread()->task_runner(),
                                FROM_HERE,
                                base::Bind(&DestroyChannelOnIOThread,
                                           server_channel_info));

  EXPECT_TRUE(client_channel_info != NULL);
  system::test::PostTaskAndWait(io_thread()->task_runner(),
                                FROM_HERE,
                                base::Bind(&DestroyChannelOnIOThread,
                                           client_channel_info));

  EXPECT_TRUE(test::Shutdown());
}

TEST_F(EmbedderTest, ChannelsHandlePassing) {
  Init();

  PlatformChannelPair channel_pair;
  ScopedPlatformHandle server_handle = channel_pair.PassServerHandle();
  ScopedPlatformHandle client_handle = channel_pair.PassClientHandle();

  ChannelInfo* server_channel_info = NULL;
  MojoHandle server_mp = CreateChannel(server_handle.Pass(),
                                       io_thread()->task_runner(),
                                       base::Bind(&StoreChannelInfo,
                                                  &server_channel_info));
  EXPECT_NE(server_mp, MOJO_HANDLE_INVALID);

  ChannelInfo* client_channel_info = NULL;
  MojoHandle client_mp = CreateChannel(client_handle.Pass(),
                                       io_thread()->task_runner(),
                                       base::Bind(&StoreChannelInfo,
                                                  &client_channel_info));
  EXPECT_NE(client_mp, MOJO_HANDLE_INVALID);

  MojoHandle h0, h1;
  EXPECT_EQ(MOJO_RESULT_OK, MojoCreateMessagePipe(&h0, &h1));

  // Write a message to |h0| (attaching nothing).
  const char kHello[] = "hello";
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoWriteMessage(h0, kHello,
                             static_cast<uint32_t>(sizeof(kHello)), NULL, 0,
                             MOJO_WRITE_MESSAGE_FLAG_NONE));

  // Write one message to |server_mp|, attaching |h1|.
  const char kWorld[] = "world!!!";
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoWriteMessage(server_mp, kWorld,
                             static_cast<uint32_t>(sizeof(kWorld)), &h1, 1,
                             MOJO_WRITE_MESSAGE_FLAG_NONE));
  h1 = MOJO_HANDLE_INVALID;

  // Write another message to |h0|.
  const char kFoo[] = "foo";
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoWriteMessage(h0, kFoo,
                             static_cast<uint32_t>(sizeof(kFoo)), NULL, 0,
                             MOJO_WRITE_MESSAGE_FLAG_NONE));

  // Wait for |client_mp| to become readable.
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoWait(client_mp, MOJO_WAIT_FLAG_READABLE,
                     MOJO_DEADLINE_INDEFINITE));

  // Read a message from |client_mp|.
  char buffer[1000] = {};
  uint32_t num_bytes = static_cast<uint32_t>(sizeof(buffer));
  MojoHandle handles[10] = {};
  uint32_t num_handles = arraysize(handles);
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoReadMessage(client_mp, buffer, &num_bytes, handles,
                            &num_handles, MOJO_READ_MESSAGE_FLAG_NONE));
  EXPECT_EQ(sizeof(kWorld), num_bytes);
  EXPECT_STREQ(kWorld, buffer);
  EXPECT_EQ(1u, num_handles);
  EXPECT_NE(handles[0], MOJO_HANDLE_INVALID);
  h1 = handles[0];

  // Wait for |h1| to become readable.
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoWait(h1, MOJO_WAIT_FLAG_READABLE, MOJO_DEADLINE_INDEFINITE));

  // Read a message from |h1|.
  memset(buffer, 0, sizeof(buffer));
  num_bytes = static_cast<uint32_t>(sizeof(buffer));
  memset(handles, 0, sizeof(handles));
  num_handles = arraysize(handles);
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoReadMessage(h1, buffer, &num_bytes, handles, &num_handles,
                            MOJO_READ_MESSAGE_FLAG_NONE));
  EXPECT_EQ(sizeof(kHello), num_bytes);
  EXPECT_STREQ(kHello, buffer);
  EXPECT_EQ(0u, num_handles);

  // Wait for |h1| to become readable (again).
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoWait(h1, MOJO_WAIT_FLAG_READABLE, MOJO_DEADLINE_INDEFINITE));

  // Read the second message from |h1|.
  memset(buffer, 0, sizeof(buffer));
  num_bytes = static_cast<uint32_t>(sizeof(buffer));
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoReadMessage(h1, buffer, &num_bytes, NULL, NULL,
                            MOJO_READ_MESSAGE_FLAG_NONE));
  EXPECT_EQ(sizeof(kFoo), num_bytes);
  EXPECT_STREQ(kFoo, buffer);

  // Write a message to |h1|.
  const char kBarBaz[] = "barbaz";
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoWriteMessage(h1, kBarBaz,
                             static_cast<uint32_t>(sizeof(kBarBaz)), NULL, 0,
                             MOJO_WRITE_MESSAGE_FLAG_NONE));

  // Wait for |h0| to become readable.
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoWait(h0, MOJO_WAIT_FLAG_READABLE, MOJO_DEADLINE_INDEFINITE));

  // Read a message from |h0|.
  memset(buffer, 0, sizeof(buffer));
  num_bytes = static_cast<uint32_t>(sizeof(buffer));
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoReadMessage(h0, buffer, &num_bytes, NULL, NULL,
                            MOJO_READ_MESSAGE_FLAG_NONE));
  EXPECT_EQ(sizeof(kBarBaz), num_bytes);
  EXPECT_STREQ(kBarBaz, buffer);

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(server_mp));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(client_mp));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(h0));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(h1));

  EXPECT_TRUE(test::Shutdown());
}

// TODO(vtl): Test immediate write & close.
// TODO(vtl): Test broken-connection cases.

}  // namespace
}  // namespace embedder
}  // namespace mojo
