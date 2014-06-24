// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/websocket_endpoint_lock_manager.h"

#include "net/base/net_errors.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/stream_socket.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

// A StreamSocket implementation with no functionality at all.
// TODO(ricea): If you need to use this in another file, please move it to
// socket_test_util.h.
class FakeStreamSocket : public StreamSocket {
 public:
  FakeStreamSocket() {}

  // StreamSocket implementation
  virtual int Connect(const CompletionCallback& callback) OVERRIDE {
    return ERR_FAILED;
  }

  virtual void Disconnect() OVERRIDE { return; }

  virtual bool IsConnected() const OVERRIDE { return false; }

  virtual bool IsConnectedAndIdle() const OVERRIDE { return false; }

  virtual int GetPeerAddress(IPEndPoint* address) const OVERRIDE {
    return ERR_FAILED;
  }

  virtual int GetLocalAddress(IPEndPoint* address) const OVERRIDE {
    return ERR_FAILED;
  }

  virtual const BoundNetLog& NetLog() const OVERRIDE { return bound_net_log_; }

  virtual void SetSubresourceSpeculation() OVERRIDE { return; }
  virtual void SetOmniboxSpeculation() OVERRIDE { return; }

  virtual bool WasEverUsed() const OVERRIDE { return false; }

  virtual bool UsingTCPFastOpen() const OVERRIDE { return false; }

  virtual bool WasNpnNegotiated() const OVERRIDE { return false; }

  virtual NextProto GetNegotiatedProtocol() const OVERRIDE {
    return kProtoUnknown;
  }

  virtual bool GetSSLInfo(SSLInfo* ssl_info) OVERRIDE { return false; }

  // Socket implementation
  virtual int Read(IOBuffer* buf,
                   int buf_len,
                   const CompletionCallback& callback) OVERRIDE {
    return ERR_FAILED;
  }

  virtual int Write(IOBuffer* buf,
                    int buf_len,
                    const CompletionCallback& callback) OVERRIDE {
    return ERR_FAILED;
  }

  virtual int SetReceiveBufferSize(int32 size) OVERRIDE { return ERR_FAILED; }

  virtual int SetSendBufferSize(int32 size) OVERRIDE { return ERR_FAILED; }

 private:
  BoundNetLog bound_net_log_;

  DISALLOW_COPY_AND_ASSIGN(FakeStreamSocket);
};

class FakeWaiter : public WebSocketEndpointLockManager::Waiter {
 public:
  FakeWaiter() : called_(false) {}

  virtual void GotEndpointLock() OVERRIDE {
    CHECK(!called_);
    called_ = true;
  }

  bool called() const { return called_; }

 private:
  bool called_;
};

class WebSocketEndpointLockManagerTest : public ::testing::Test {
 protected:
  WebSocketEndpointLockManagerTest()
      : instance_(WebSocketEndpointLockManager::GetInstance()) {}
  virtual ~WebSocketEndpointLockManagerTest() {
    // If this check fails then subsequent tests may fail.
    CHECK(instance_->IsEmpty());
  }

  WebSocketEndpointLockManager* instance() const { return instance_; }

  IPEndPoint DummyEndpoint() {
    IPAddressNumber ip_address_number;
    CHECK(ParseIPLiteralToNumber("127.0.0.1", &ip_address_number));
    return IPEndPoint(ip_address_number, 80);
  }

  void UnlockDummyEndpoint(int times) {
    for (int i = 0; i < times; ++i) {
      instance()->UnlockEndpoint(DummyEndpoint());
    }
  }

  WebSocketEndpointLockManager* const instance_;
};

TEST_F(WebSocketEndpointLockManagerTest, GetInstanceWorks) {
  // All the work is done by the test framework.
}

TEST_F(WebSocketEndpointLockManagerTest, LockEndpointReturnsOkOnce) {
  FakeWaiter waiters[2];
  EXPECT_EQ(OK, instance()->LockEndpoint(DummyEndpoint(), &waiters[0]));
  EXPECT_EQ(ERR_IO_PENDING,
            instance()->LockEndpoint(DummyEndpoint(), &waiters[1]));

  UnlockDummyEndpoint(2);
}

TEST_F(WebSocketEndpointLockManagerTest, GotEndpointLockNotCalledOnOk) {
  FakeWaiter waiter;
  EXPECT_EQ(OK, instance()->LockEndpoint(DummyEndpoint(), &waiter));
  EXPECT_FALSE(waiter.called());

  UnlockDummyEndpoint(1);
}

TEST_F(WebSocketEndpointLockManagerTest, GotEndpointLockNotCalledImmediately) {
  FakeWaiter waiters[2];
  EXPECT_EQ(OK, instance()->LockEndpoint(DummyEndpoint(), &waiters[0]));
  EXPECT_EQ(ERR_IO_PENDING,
            instance()->LockEndpoint(DummyEndpoint(), &waiters[1]));
  EXPECT_FALSE(waiters[1].called());

  UnlockDummyEndpoint(2);
}

TEST_F(WebSocketEndpointLockManagerTest, GotEndpointLockCalledWhenUnlocked) {
  FakeWaiter waiters[2];
  EXPECT_EQ(OK, instance()->LockEndpoint(DummyEndpoint(), &waiters[0]));
  EXPECT_EQ(ERR_IO_PENDING,
            instance()->LockEndpoint(DummyEndpoint(), &waiters[1]));
  instance()->UnlockEndpoint(DummyEndpoint());
  EXPECT_TRUE(waiters[1].called());

  UnlockDummyEndpoint(1);
}

TEST_F(WebSocketEndpointLockManagerTest,
       EndpointUnlockedIfWaiterAlreadyDeleted) {
  FakeWaiter first_lock_holder;
  EXPECT_EQ(OK, instance()->LockEndpoint(DummyEndpoint(), &first_lock_holder));

  {
    FakeWaiter short_lived_waiter;
    EXPECT_EQ(ERR_IO_PENDING,
              instance()->LockEndpoint(DummyEndpoint(), &short_lived_waiter));
  }

  instance()->UnlockEndpoint(DummyEndpoint());

  FakeWaiter second_lock_holder;
  EXPECT_EQ(OK, instance()->LockEndpoint(DummyEndpoint(), &second_lock_holder));

  UnlockDummyEndpoint(1);
}

TEST_F(WebSocketEndpointLockManagerTest, RememberSocketWorks) {
  FakeWaiter waiters[2];
  FakeStreamSocket dummy_socket;
  EXPECT_EQ(OK, instance()->LockEndpoint(DummyEndpoint(), &waiters[0]));
  EXPECT_EQ(ERR_IO_PENDING,
            instance()->LockEndpoint(DummyEndpoint(), &waiters[1]));

  instance()->RememberSocket(&dummy_socket, DummyEndpoint());
  instance()->UnlockSocket(&dummy_socket);
  EXPECT_TRUE(waiters[1].called());

  UnlockDummyEndpoint(1);
}

// Calling UnlockSocket() on the same socket a second time should be harmless.
TEST_F(WebSocketEndpointLockManagerTest, UnlockSocketTwice) {
  FakeWaiter waiter;
  FakeStreamSocket dummy_socket;
  EXPECT_EQ(OK, instance()->LockEndpoint(DummyEndpoint(), &waiter));
  instance()->RememberSocket(&dummy_socket, DummyEndpoint());
  instance()->UnlockSocket(&dummy_socket);
  instance()->UnlockSocket(&dummy_socket);
}

}  // namespace

}  // namespace net
