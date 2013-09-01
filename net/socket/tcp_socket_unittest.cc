// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/tcp_socket.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "net/base/address_list.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/socket/tcp_client_socket.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace net {

namespace {
const int kListenBacklog = 5;

class TCPSocketTest : public PlatformTest {
 protected:
  TCPSocketTest() : socket_(NULL, NetLog::Source()) {
  }

  void SetUpListenIPv4() {
    IPEndPoint address;
    ParseAddress("127.0.0.1", 0, &address);

    ASSERT_EQ(OK, socket_.Create(ADDRESS_FAMILY_IPV4));
    ASSERT_EQ(OK, socket_.Bind(address));
    ASSERT_EQ(OK, socket_.Listen(kListenBacklog));
    ASSERT_EQ(OK, socket_.GetLocalAddress(&local_address_));
  }

  void SetUpListenIPv6(bool* success) {
    *success = false;
    IPEndPoint address;
    ParseAddress("::1", 0, &address);

    if (socket_.Create(ADDRESS_FAMILY_IPV6) != OK ||
        socket_.Bind(address) != OK ||
        socket_.Listen(kListenBacklog) != OK) {
      LOG(ERROR) << "Failed to listen on ::1 - probably because IPv6 is "
          "disabled. Skipping the test";
      return;
    }
    ASSERT_EQ(OK, socket_.GetLocalAddress(&local_address_));
    *success = true;
  }

  void ParseAddress(const std::string& ip_str, int port, IPEndPoint* address) {
    IPAddressNumber ip_number;
    bool rv = ParseIPLiteralToNumber(ip_str, &ip_number);
    if (!rv)
      return;
    *address = IPEndPoint(ip_number, port);
  }

  AddressList local_address_list() const {
    return AddressList(local_address_);
  }

  TCPSocket socket_;
  IPEndPoint local_address_;
};

// Test listening and accepting with a socket bound to an IPv4 address.
TEST_F(TCPSocketTest, Accept) {
  ASSERT_NO_FATAL_FAILURE(SetUpListenIPv4());

  TestCompletionCallback connect_callback;
  // TODO(yzshen): Switch to use TCPSocket when it supports client socket
  // operations.
  TCPClientSocket connecting_socket(local_address_list(),
                                    NULL, NetLog::Source());
  connecting_socket.Connect(connect_callback.callback());

  TestCompletionCallback accept_callback;
  scoped_ptr<TCPSocket> accepted_socket;
  IPEndPoint accepted_address;
  int result = socket_.Accept(&accepted_socket, &accepted_address,
                              accept_callback.callback());
  if (result == ERR_IO_PENDING)
    result = accept_callback.WaitForResult();
  ASSERT_EQ(OK, result);

  EXPECT_TRUE(accepted_socket.get());

  // Both sockets should be on the loopback network interface.
  EXPECT_EQ(accepted_address.address(), local_address_.address());

  EXPECT_EQ(OK, connect_callback.WaitForResult());
}

// Test Accept() callback.
TEST_F(TCPSocketTest, AcceptAsync) {
  ASSERT_NO_FATAL_FAILURE(SetUpListenIPv4());

  TestCompletionCallback accept_callback;
  scoped_ptr<TCPSocket> accepted_socket;
  IPEndPoint accepted_address;
  ASSERT_EQ(ERR_IO_PENDING,
            socket_.Accept(&accepted_socket, &accepted_address,
                           accept_callback.callback()));

  TestCompletionCallback connect_callback;
  TCPClientSocket connecting_socket(local_address_list(),
                                    NULL, NetLog::Source());
  connecting_socket.Connect(connect_callback.callback());

  EXPECT_EQ(OK, connect_callback.WaitForResult());
  EXPECT_EQ(OK, accept_callback.WaitForResult());

  EXPECT_TRUE(accepted_socket.get());

  // Both sockets should be on the loopback network interface.
  EXPECT_EQ(accepted_address.address(), local_address_.address());
}

// Accept two connections simultaneously.
TEST_F(TCPSocketTest, Accept2Connections) {
  ASSERT_NO_FATAL_FAILURE(SetUpListenIPv4());

  TestCompletionCallback accept_callback;
  scoped_ptr<TCPSocket> accepted_socket;
  IPEndPoint accepted_address;

  ASSERT_EQ(ERR_IO_PENDING,
            socket_.Accept(&accepted_socket, &accepted_address,
                           accept_callback.callback()));

  TestCompletionCallback connect_callback;
  TCPClientSocket connecting_socket(local_address_list(),
                                    NULL, NetLog::Source());
  connecting_socket.Connect(connect_callback.callback());

  TestCompletionCallback connect_callback2;
  TCPClientSocket connecting_socket2(local_address_list(),
                                     NULL, NetLog::Source());
  connecting_socket2.Connect(connect_callback2.callback());

  EXPECT_EQ(OK, accept_callback.WaitForResult());

  TestCompletionCallback accept_callback2;
  scoped_ptr<TCPSocket> accepted_socket2;
  IPEndPoint accepted_address2;

  int result = socket_.Accept(&accepted_socket2, &accepted_address2,
                              accept_callback2.callback());
  if (result == ERR_IO_PENDING)
    result = accept_callback2.WaitForResult();
  ASSERT_EQ(OK, result);

  EXPECT_EQ(OK, connect_callback.WaitForResult());
  EXPECT_EQ(OK, connect_callback2.WaitForResult());

  EXPECT_TRUE(accepted_socket.get());
  EXPECT_TRUE(accepted_socket2.get());
  EXPECT_NE(accepted_socket.get(), accepted_socket2.get());

  EXPECT_EQ(accepted_address.address(), local_address_.address());
  EXPECT_EQ(accepted_address2.address(), local_address_.address());
}

// Test listening and accepting with a socket bound to an IPv6 address.
TEST_F(TCPSocketTest, AcceptIPv6) {
  bool initialized = false;
  ASSERT_NO_FATAL_FAILURE(SetUpListenIPv6(&initialized));
  if (!initialized)
    return;

  TestCompletionCallback connect_callback;
  TCPClientSocket connecting_socket(local_address_list(),
                                    NULL, NetLog::Source());
  connecting_socket.Connect(connect_callback.callback());

  TestCompletionCallback accept_callback;
  scoped_ptr<TCPSocket> accepted_socket;
  IPEndPoint accepted_address;
  int result = socket_.Accept(&accepted_socket, &accepted_address,
                              accept_callback.callback());
  if (result == ERR_IO_PENDING)
    result = accept_callback.WaitForResult();
  ASSERT_EQ(OK, result);

  EXPECT_TRUE(accepted_socket.get());

  // Both sockets should be on the loopback network interface.
  EXPECT_EQ(accepted_address.address(), local_address_.address());

  EXPECT_EQ(OK, connect_callback.WaitForResult());
}

}  // namespace
}  // namespace net
