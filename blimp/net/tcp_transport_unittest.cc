// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "blimp/net/blimp_connection.h"
#include "blimp/net/tcp_client_transport.h"
#include "blimp/net/tcp_engine_transport.h"
#include "net/base/address_list.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blimp {

namespace {

// Integration test for TCPEngineTransport and TCPClientTransport.
class TCPTransportTest : public testing::Test {
 protected:
  TCPTransportTest() {
    net::IPEndPoint local_address;
    ParseAddress("127.0.0.1", 0, &local_address);
    server_.reset(new TCPEngineTransport(local_address, nullptr));
  }

  net::AddressList GetLocalAddressList() const {
    net::IPEndPoint local_address;
    server_->GetLocalAddressForTesting(&local_address);
    return net::AddressList(local_address);
  }

  void ParseAddress(const std::string& ip_str,
                    uint16 port,
                    net::IPEndPoint* address) {
    net::IPAddressNumber ip_number;
    bool rv = net::ParseIPLiteralToNumber(ip_str, &ip_number);
    if (!rv)
      return;
    *address = net::IPEndPoint(ip_number, port);
  }

  base::MessageLoopForIO message_loop_;
  scoped_ptr<TCPEngineTransport> server_;
};

TEST_F(TCPTransportTest, Connect) {
  net::TestCompletionCallback accept_callback;
  ASSERT_EQ(net::ERR_IO_PENDING, server_->Connect(accept_callback.callback()));

  net::TestCompletionCallback connect_callback;
  TCPClientTransport client(GetLocalAddressList(), nullptr);
  ASSERT_EQ(net::ERR_IO_PENDING, client.Connect(connect_callback.callback()));

  EXPECT_EQ(net::OK, connect_callback.WaitForResult());
  EXPECT_EQ(net::OK, accept_callback.WaitForResult());
  EXPECT_TRUE(server_->TakeConnection() != nullptr);
}

TEST_F(TCPTransportTest, TwoClientConnections) {
  net::TestCompletionCallback accept_callback1;
  ASSERT_EQ(net::ERR_IO_PENDING, server_->Connect(accept_callback1.callback()));

  net::TestCompletionCallback connect_callback1;
  TCPClientTransport client1(GetLocalAddressList(), nullptr);
  ASSERT_EQ(net::ERR_IO_PENDING, client1.Connect(connect_callback1.callback()));

  net::TestCompletionCallback connect_callback2;
  TCPClientTransport client2(GetLocalAddressList(), nullptr);
  ASSERT_EQ(net::ERR_IO_PENDING, client2.Connect(connect_callback2.callback()));

  EXPECT_EQ(net::OK, connect_callback1.WaitForResult());
  EXPECT_EQ(net::OK, accept_callback1.WaitForResult());
  EXPECT_TRUE(server_->TakeConnection() != nullptr);

  net::TestCompletionCallback accept_callback2;
  ASSERT_EQ(net::OK, server_->Connect(accept_callback2.callback()));
  EXPECT_EQ(net::OK, connect_callback2.WaitForResult());
  EXPECT_EQ(net::OK, accept_callback2.WaitForResult());
  EXPECT_TRUE(server_->TakeConnection() != nullptr);
}

// TODO(haibinlu): add a test case for message exchange.

}  // namespace

}  // namespace blimp
