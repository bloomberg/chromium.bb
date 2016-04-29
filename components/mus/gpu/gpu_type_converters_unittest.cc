// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/scoped_file.h"
#include "components/mus/gpu/gpu_type_converters.h"
#include "build/build_config.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

// Test for mojo TypeConverter of mus::mojom::ChannelHandle.
TEST(MusGpuTypeConvertersTest, ChannelHandle) {
  {
    const std::string channel_name = "test_channel_name";
    IPC::ChannelHandle handle(channel_name);

    mus::mojom::ChannelHandlePtr mojo_handle =
        mus::mojom::ChannelHandle::From(handle);
    ASSERT_EQ(mojo_handle->name, channel_name);
    EXPECT_FALSE(mojo_handle->socket.is_valid());

    handle = mojo_handle.To<IPC::ChannelHandle>();
    ASSERT_EQ(handle.name, channel_name);
#if defined(OS_POSIX)
    ASSERT_EQ(handle.socket.fd, -1);
#endif
  }

#if defined(OS_POSIX)
  {
    const std::string channel_name = "test_channel_name";
    int fd1 = -1;
    int fd2 = -1;
    bool result = IPC::SocketPair(&fd1, &fd2);
    EXPECT_TRUE(result);

    base::ScopedFD scoped_fd1(fd1);
    base::ScopedFD scoped_fd2(fd2);
    IPC::ChannelHandle handle(channel_name,
                              base::FileDescriptor(scoped_fd1.get(), false));

    mus::mojom::ChannelHandlePtr mojo_handle =
        mus::mojom::ChannelHandle::From(handle);
    ASSERT_EQ(mojo_handle->name, channel_name);
    EXPECT_TRUE(mojo_handle->socket.is_valid());

    handle = mojo_handle.To<IPC::ChannelHandle>();
    ASSERT_EQ(handle.name, channel_name);
    ASSERT_NE(handle.socket.fd, -1);
    EXPECT_TRUE(handle.socket.auto_close);
    base::ScopedFD socped_fd3(handle.socket.fd);
  }
#endif
}
