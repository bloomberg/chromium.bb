// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/socket/udp_socket.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "net/base/io_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

// UDPSocketUnitTest exists solely to make it easier to pass a specific
// gtest_filter argument during development.
class UDPSocketUnitTest : public BrowserWithTestWindowTest {
};

static void OnConnected(int result) {
  DCHECK(result == 0);
}

static void OnCompleted(int bytes_read,
                        scoped_refptr<net::IOBuffer> io_buffer,
                        const std::string& address,
                        int port) {
  // Do nothing; don't care.
}

TEST(UDPSocketUnitTest, TestUDPSocketRecvFrom) {
  MessageLoopForIO io_loop;  // for RecvFrom to do its threaded work.
  UDPSocket socket("abcdefghijklmnopqrst", NULL);

  // Confirm that we can call two RecvFroms in quick succession without
  // triggering crbug.com/146606.
  socket.Connect("127.0.0.1", 40000, base::Bind(&OnConnected));
  socket.RecvFrom(4096, base::Bind(&OnCompleted));
  socket.RecvFrom(4096, base::Bind(&OnCompleted));
}

}  // namespace extensions
