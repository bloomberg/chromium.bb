// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_network_session.h"

#include "base/ref_counted.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/mock_network_change_notifier.h"
#include "net/base/ssl_config_service_defaults.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/proxy/proxy_service.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/tcp_client_socket_pool.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_session_pool.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

TEST(HttpNetworkSessionTest, FlushOnNetworkChange) {
  MockNetworkChangeNotifier mock_notifier;
  scoped_refptr<MockCachingHostResolver> mock_resolver(
      new MockCachingHostResolver);
  MockClientSocketFactory mock_factory;
  scoped_ptr<HttpAuthHandlerFactory> auth_handler_factory(
      HttpAuthHandlerFactory::CreateDefault());
  scoped_refptr<HttpNetworkSession> session(
      new HttpNetworkSession(&mock_notifier,
		             mock_resolver,
			     ProxyService::CreateNull(),
			     &mock_factory,
			     new SSLConfigServiceDefaults,
                             auth_handler_factory.get()));

  scoped_refptr<TCPClientSocketPool> tcp_socket_pool(
      session->tcp_socket_pool());

  // Start up one socket to leave idle in the pool.
  StaticSocketDataProvider data;
  mock_factory.AddSocketDataProvider(&data);
  TestCompletionCallback callback;
  ClientSocketHandle handle;
  TCPSocketParams dest("www.google.com", 80, LOW, GURL(), false);
  int rv = handle.Init(
      "1", dest, LOW, &callback, tcp_socket_pool, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());
  EXPECT_EQ(OK, callback.WaitForResult());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());

  // Need to run all pending to release the socket back to the pool.
  handle.Reset();
  MessageLoop::current()->RunAllPending();
  
  // Now we should have 1 idle socket.
  EXPECT_EQ(1, tcp_socket_pool->IdleSocketCount());

  // Start up another socket to keep as active.  Hold onto it until after the IP
  // address change causes a session flush.
  StaticSocketDataProvider data2;
  mock_factory.AddSocketDataProvider(&data2);
  rv = handle.Init("2", dest, LOW, &callback, tcp_socket_pool, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());
  EXPECT_EQ(OK, callback.WaitForResult());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());

  HostPortPair host_port_pair("www.google.com", 80);
  
  scoped_refptr<SpdySessionPool>
      spdy_session_pool(session->spdy_session_pool());
  scoped_refptr<SpdySession> spdy_session(session->spdy_session_pool()->Get(
      host_port_pair, session, BoundNetLog()));

  EXPECT_TRUE(spdy_session_pool->HasSession(host_port_pair));

  // After an IP address change, we should have 0 idle sockets in the old pool
  // and we should have replaced all the pools.
  mock_notifier.NotifyIPAddressChange();
  EXPECT_EQ(0, tcp_socket_pool->IdleSocketCount());
  EXPECT_NE(tcp_socket_pool, session->tcp_socket_pool());
  EXPECT_EQ(0, session->tcp_socket_pool()->IdleSocketCount());
  EXPECT_FALSE(session->spdy_session_pool()->HasSession(host_port_pair));
  EXPECT_NE(session->spdy_session_pool(), spdy_session_pool);

  // Release all the handles to old objects.  Run all pending tasks afterwards
  // to flush pending tasks such as releasing sockets.
  tcp_socket_pool = NULL;
  spdy_session_pool = NULL;
  handle.Reset();
  MessageLoop::current()->RunAllPending();
}

}  // namespace
}  // namespace net
