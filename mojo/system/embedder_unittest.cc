// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/embedder.h"

#include <string.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
// TODO(vtl): Remove build_config.h include when fully implemented on Windows.
#include "build/build_config.h"
#include "mojo/public/system/core.h"
#include "mojo/system/platform_channel_pair.h"
#include "mojo/system/test_embedder.h"
#include "mojo/system/test_utils.h"

namespace mojo {
namespace embedder {
namespace {

typedef system::test::TestWithIOThreadBase EmbedderTest;

void StoreChannelInfo(ChannelInfo** store_channel_info_here,
                      ChannelInfo* channel_info) {
  CHECK(store_channel_info_here);
  CHECK(channel_info);
  *store_channel_info_here = channel_info;
}

TEST_F(EmbedderTest, ChannelsBasic) {
  Init();

// TODO(vtl): |PlatformChannelPair| not implemented on Windows yet.
#if !defined(OS_WIN)
  PlatformChannelPair channel_pair;
  ScopedPlatformHandle server_handle = channel_pair.PassServerHandle();
  ScopedPlatformHandle client_handle = channel_pair.PassClientHandle();

  ChannelInfo* server_channel_info = NULL;
  MojoHandle server_mp = CreateChannel(server_handle.Pass(),
                                       io_thread_task_runner(),
                                       base::Bind(&StoreChannelInfo,
                                                  &server_channel_info));
  EXPECT_NE(server_mp, MOJO_HANDLE_INVALID);

  ChannelInfo* client_channel_info = NULL;
  MojoHandle client_mp = CreateChannel(client_handle.Pass(),
                                       io_thread_task_runner(),
                                       base::Bind(&StoreChannelInfo,
                                                  &client_channel_info));
  EXPECT_NE(client_mp, MOJO_HANDLE_INVALID);

  // We can write to a message pipe handle immediately.
  const char kHello[] = "hello";
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoWriteMessage(server_mp, kHello,
                             static_cast<uint32_t>(sizeof(kHello)), NULL, 0u,
                             MOJO_WRITE_MESSAGE_FLAG_NONE));

  // Now wait for the other side to become readable.
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoWait(client_mp, MOJO_WAIT_FLAG_READABLE,
                     MOJO_DEADLINE_INDEFINITE));

  char buffer[1000] = {};
  uint32_t num_bytes = static_cast<uint32_t>(sizeof(kHello));
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoReadMessage(client_mp, buffer, &num_bytes, NULL, NULL,
                            MOJO_READ_MESSAGE_FLAG_NONE));
  EXPECT_EQ(sizeof(kHello), num_bytes);
  EXPECT_EQ(0, memcmp(buffer, kHello, num_bytes));

  // TODO(vtl): FIXME -- This rapid-fire closing leads to a warning: "Received a
  // message for nonexistent local destination ID 1". This is due to a race
  // condition (in channel.cc/message_pipe.cc).
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(server_mp));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(client_mp));

  EXPECT_TRUE(server_channel_info != NULL);
  system::test::PostTaskAndWait(io_thread_task_runner(),
                                FROM_HERE,
                                base::Bind(&DestroyChannelOnIOThread,
                                           server_channel_info));

  EXPECT_TRUE(client_channel_info != NULL);
  system::test::PostTaskAndWait(io_thread_task_runner(),
                                FROM_HERE,
                                base::Bind(&DestroyChannelOnIOThread,
                                           client_channel_info));
#endif  // !defined(OS_WIN)

  test::Shutdown();
}

// TODO(vtl): Test immediate write & close.
// TODO(vtl): Test broken-connection cases.

}  // namespace
}  // namespace embedder
}  // namespace mojo
