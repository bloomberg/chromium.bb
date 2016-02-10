// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/callback.h"
#include "mojo/services/network/public/cpp/udp_socket_wrapper.h"
#include "mojo/services/network/public/interfaces/network_service.mojom.h"
#include "mojo/services/network/public/interfaces/udp_socket.mojom.h"
#include "mojo/shell/public/cpp/application_test_base.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace service {
namespace {

NetAddressPtr GetLocalHostWithAnyPort() {
  NetAddressPtr addr(NetAddress::New());
  addr->family = NetAddressFamily::IPV4;
  addr->ipv4 = NetAddressIPv4::New();
  addr->ipv4->port = 0;
  addr->ipv4->addr.resize(4);
  addr->ipv4->addr[0] = 127;
  addr->ipv4->addr[1] = 0;
  addr->ipv4->addr[2] = 0;
  addr->ipv4->addr[3] = 1;

  return addr;
}

Array<uint8_t> CreateTestMessage(uint8_t initial, size_t size) {
  Array<uint8_t> array(size);
  for (size_t i = 0; i < size; ++i)
    array[i] = static_cast<uint8_t>((i + initial) % 256);
  return array;
}

template <typename CallbackType>
class TestCallbackBase {
 public:
  TestCallbackBase() : state_(nullptr), run_loop_(nullptr), ran_(false) {}

  ~TestCallbackBase() {
    state_->set_test_callback(nullptr);
  }

  CallbackType callback() const { return callback_; }

  void WaitForResult() {
    if (ran_)
      return;

    base::RunLoop run_loop;
    run_loop_ = &run_loop;
    run_loop.Run();
    run_loop_ = nullptr;
  }

 protected:
  struct StateBase : public CallbackType::Runnable {
    StateBase() : test_callback_(nullptr) {}
    ~StateBase() override {}

    void set_test_callback(TestCallbackBase* test_callback) {
      test_callback_ = test_callback;
    }

   protected:
    void NotifyRun() const {
      if (test_callback_) {
        test_callback_->ran_ = true;
        if (test_callback_->run_loop_)
          test_callback_->run_loop_->Quit();
      }
    }

    TestCallbackBase* test_callback_;

   private:
    DISALLOW_COPY_AND_ASSIGN(StateBase);
  };

  // Takes ownership of |state|, and guarantees that it lives at least as long
  // as this object.
  void Initialize(StateBase* state) {
    state_ = state;
    state_->set_test_callback(this);
    callback_ = CallbackType(
        static_cast<typename CallbackType::Runnable*>(state_));
  }

 private:
  // The lifespan is managed by |callback_| (and its copies).
  StateBase* state_;
  CallbackType callback_;
  base::RunLoop* run_loop_;
  bool ran_;

  DISALLOW_COPY_AND_ASSIGN(TestCallbackBase);
};

class TestCallback : public TestCallbackBase<Callback<void(NetworkErrorPtr)>> {
 public:
  TestCallback() {
    Initialize(new State());
  }
  ~TestCallback() {}

  const NetworkErrorPtr& result() const { return result_; }

 private:
  struct State: public StateBase {
    ~State() override {}

    void Run(NetworkErrorPtr result) const override {
      if (test_callback_) {
        TestCallback* callback = static_cast<TestCallback*>(test_callback_);
        callback->result_ = std::move(result);
      }
      NotifyRun();
    }
  };

  NetworkErrorPtr result_;
};

class TestCallbackWithAddressAndReceiver
    : public TestCallbackBase<
          Callback<void(NetworkErrorPtr,
                        NetAddressPtr,
                        InterfaceRequest<UDPSocketReceiver>)>> {
 public:
  TestCallbackWithAddressAndReceiver() { Initialize(new State()); }
  ~TestCallbackWithAddressAndReceiver() {}

  const NetworkErrorPtr& result() const { return result_; }
  const NetAddressPtr& net_address() const { return net_address_; }
  InterfaceRequest<UDPSocketReceiver>& receiver() { return receiver_; }

 private:
  struct State : public StateBase {
    ~State() override {}

    void Run(NetworkErrorPtr result,
             NetAddressPtr net_address,
             InterfaceRequest<UDPSocketReceiver> receiver) const override {
      if (test_callback_) {
        TestCallbackWithAddressAndReceiver* callback =
            static_cast<TestCallbackWithAddressAndReceiver*>(test_callback_);
        callback->result_ = std::move(result);
        callback->net_address_ = std::move(net_address);
        callback->receiver_ = std::move(receiver);
      }
      NotifyRun();
    }
  };

  NetworkErrorPtr result_;
  NetAddressPtr net_address_;
  InterfaceRequest<UDPSocketReceiver> receiver_;
};

class TestCallbackWithAddress
    : public TestCallbackBase<Callback<void(NetworkErrorPtr, NetAddressPtr)>> {
 public:
  TestCallbackWithAddress() {
    Initialize(new State());
  }
  ~TestCallbackWithAddress() {}

  const NetworkErrorPtr& result() const { return result_; }
  const NetAddressPtr& net_address() const { return net_address_; }

 private:
  struct State : public StateBase {
    ~State() override {}

    void Run(NetworkErrorPtr result, NetAddressPtr net_address) const override {
      if (test_callback_) {
        TestCallbackWithAddress* callback =
            static_cast<TestCallbackWithAddress*>(test_callback_);
        callback->result_ = std::move(result);
        callback->net_address_ = std::move(net_address);
      }
      NotifyRun();
    }
  };

  NetworkErrorPtr result_;
  NetAddressPtr net_address_;
};

class TestCallbackWithUint32
    : public TestCallbackBase<Callback<void(uint32_t)>> {
 public:
  TestCallbackWithUint32() : result_(0) {
    Initialize(new State());
  }
  ~TestCallbackWithUint32() {}

  uint32_t result() const { return result_; }

 private:
  struct State : public StateBase {
    ~State() override {}

    void Run(uint32_t result) const override {
      if (test_callback_) {
        TestCallbackWithUint32* callback =
            static_cast<TestCallbackWithUint32*>(test_callback_);
        callback->result_ = result;
      }
      NotifyRun();
    }
  };

  uint32_t result_;
};

class TestReceiveCallback
    : public TestCallbackBase<
        Callback<void(NetworkErrorPtr, NetAddressPtr, Array<uint8_t>)>> {
 public:
  TestReceiveCallback() {
    Initialize(new State());
  }
  ~TestReceiveCallback() {}

  const NetworkErrorPtr& result() const { return result_; }
  const NetAddressPtr& src_addr() const { return src_addr_; }
  const Array<uint8_t>& data() const { return data_; }

 private:
  struct State : public StateBase {
    ~State() override {}

    void Run(NetworkErrorPtr result,
             NetAddressPtr src_addr,
             Array<uint8_t> data) const override {
      if (test_callback_) {
        TestReceiveCallback* callback =
            static_cast<TestReceiveCallback*>(test_callback_);
        callback->result_ = std::move(result);
        callback->src_addr_ = std::move(src_addr);
        callback->data_ = std::move(data);
      }
      NotifyRun();
    }
  };

  NetworkErrorPtr result_;
  NetAddressPtr src_addr_;
  Array<uint8_t> data_;
};

struct ReceiveResult {
  NetworkErrorPtr result;
  NetAddressPtr addr;
  Array<uint8_t> data;
};

class UDPSocketReceiverImpl : public UDPSocketReceiver {
 public:
  UDPSocketReceiverImpl() : run_loop_(nullptr), expected_receive_count_(0) {}

  ~UDPSocketReceiverImpl() override {
    while (!results_.empty()) {
      delete results_.front();
      results_.pop();
    }
  }

  std::queue<ReceiveResult*>* results() {
    return &results_;
  }

  void WaitForReceiveResults(size_t count) {
    if (results_.size() == count)
      return;

    expected_receive_count_ = count;
    base::RunLoop run_loop;
    run_loop_ = &run_loop;
    run_loop.Run();
    run_loop_ = nullptr;
  }

 private:
  void OnReceived(NetworkErrorPtr result,
                  NetAddressPtr src_addr,
                  Array<uint8_t> data) override {
    ReceiveResult* entry = new ReceiveResult();
    entry->result = std::move(result);
    entry->addr = std::move(src_addr);
    entry->data = std::move(data);

    results_.push(entry);

    if (results_.size() == expected_receive_count_ && run_loop_) {
      expected_receive_count_ = 0;
      run_loop_->Quit();
    }
  }

  base::RunLoop* run_loop_;
  std::queue<ReceiveResult*> results_;
  size_t expected_receive_count_;

  DISALLOW_COPY_AND_ASSIGN(UDPSocketReceiverImpl);
};

class UDPSocketAppTest : public test::ApplicationTestBase {
 public:
  UDPSocketAppTest() : receiver_binding_(&receiver_) {}
  ~UDPSocketAppTest() override {}

  void SetUp() override {
    ApplicationTestBase::SetUp();
    shell()->ConnectToInterface("mojo:network_service", &network_service_);
    network_service_->CreateUDPSocket(GetProxy(&socket_));
  }

 protected:
  NetworkServicePtr network_service_;
  UDPSocketPtr socket_;
  UDPSocketReceiverImpl receiver_;
  Binding<UDPSocketReceiver> receiver_binding_;

  DISALLOW_COPY_AND_ASSIGN(UDPSocketAppTest);
};

}  // namespace

TEST_F(UDPSocketAppTest, Settings) {
  TestCallback callback1;
  socket_->AllowAddressReuse(callback1.callback());
  callback1.WaitForResult();
  EXPECT_EQ(net::OK, callback1.result()->code);

  // Should fail because the socket hasn't been bound.
  TestCallback callback2;
  socket_->SetSendBufferSize(1024, callback2.callback());
  callback2.WaitForResult();
  EXPECT_NE(net::OK, callback2.result()->code);

  // Should fail because the socket hasn't been bound.
  TestCallback callback3;
  socket_->SetReceiveBufferSize(2048, callback3.callback());
  callback3.WaitForResult();
  EXPECT_NE(net::OK, callback3.result()->code);

  TestCallbackWithAddressAndReceiver callback4;
  socket_->Bind(GetLocalHostWithAnyPort(), callback4.callback());
  callback4.WaitForResult();
  EXPECT_EQ(net::OK, callback4.result()->code);
  EXPECT_NE(0u, callback4.net_address()->ipv4->port);

  // Should fail because the socket has been bound.
  TestCallback callback5;
  socket_->AllowAddressReuse(callback5.callback());
  callback5.WaitForResult();
  EXPECT_NE(net::OK, callback5.result()->code);

  TestCallback callback6;
  socket_->SetSendBufferSize(1024, callback6.callback());
  callback6.WaitForResult();
  EXPECT_EQ(net::OK, callback6.result()->code);

  TestCallback callback7;
  socket_->SetReceiveBufferSize(2048, callback7.callback());
  callback7.WaitForResult();
  EXPECT_EQ(net::OK, callback7.result()->code);

  TestCallbackWithUint32 callback8;
  socket_->NegotiateMaxPendingSendRequests(0, callback8.callback());
  callback8.WaitForResult();
  EXPECT_GT(callback8.result(), 0u);

  TestCallbackWithUint32 callback9;
  socket_->NegotiateMaxPendingSendRequests(16, callback9.callback());
  callback9.WaitForResult();
  EXPECT_GT(callback9.result(), 0u);
}

TEST_F(UDPSocketAppTest, TestReadWrite) {
  TestCallbackWithAddressAndReceiver callback1;
  socket_->Bind(GetLocalHostWithAnyPort(), callback1.callback());
  callback1.WaitForResult();
  ASSERT_EQ(net::OK, callback1.result()->code);
  ASSERT_NE(0u, callback1.net_address()->ipv4->port);

  receiver_binding_.Bind(std::move(callback1.receiver()));

  NetAddressPtr server_addr = callback1.net_address().Clone();

  UDPSocketPtr client_socket;
  network_service_->CreateUDPSocket(GetProxy(&client_socket));

  TestCallbackWithAddressAndReceiver callback2;
  client_socket->Bind(GetLocalHostWithAnyPort(), callback2.callback());
  callback2.WaitForResult();
  ASSERT_EQ(net::OK, callback2.result()->code);
  ASSERT_NE(0u, callback2.net_address()->ipv4->port);

  NetAddressPtr client_addr = callback2.net_address().Clone();

  const size_t kDatagramCount = 6;
  const size_t kDatagramSize = 255;
  socket_->ReceiveMore(kDatagramCount);

  for (size_t i = 0; i < kDatagramCount; ++i) {
    TestCallback callback;
    client_socket->SendTo(
        server_addr.Clone(),
        CreateTestMessage(static_cast<uint8_t>(i), kDatagramSize),
        callback.callback());
    callback.WaitForResult();
    EXPECT_EQ(255, callback.result()->code);
  }

  receiver_.WaitForReceiveResults(kDatagramCount);
  for (size_t i = 0; i < kDatagramCount; ++i) {
    scoped_ptr<ReceiveResult> result(receiver_.results()->front());
    receiver_.results()->pop();

    EXPECT_EQ(static_cast<int>(kDatagramSize), result->result->code);
    EXPECT_TRUE(result->addr.Equals(client_addr));
    EXPECT_TRUE(result->data.Equals(
        CreateTestMessage(static_cast<uint8_t>(i), kDatagramSize)));
  }
}

TEST_F(UDPSocketAppTest, TestConnectedReadWrite) {
  TestCallbackWithAddressAndReceiver callback1;
  socket_->Bind(GetLocalHostWithAnyPort(), callback1.callback());
  callback1.WaitForResult();
  ASSERT_EQ(net::OK, callback1.result()->code);
  ASSERT_NE(0u, callback1.net_address()->ipv4->port);

  receiver_binding_.Bind(std::move(callback1.receiver()));

  NetAddressPtr server_addr = callback1.net_address().Clone();

  UDPSocketPtr client_socket;
  network_service_->CreateUDPSocket(GetProxy(&client_socket));

  TestCallbackWithAddressAndReceiver callback2;
  client_socket->Connect(server_addr.Clone(), callback2.callback());
  callback2.WaitForResult();
  ASSERT_EQ(net::OK, callback2.result()->code);
  ASSERT_NE(0u, callback2.net_address()->ipv4->port);

  UDPSocketReceiverImpl client_socket_receiver;
  Binding<UDPSocketReceiver> client_receiver_binding(
      &client_socket_receiver, std::move(callback2.receiver()));

  NetAddressPtr client_addr = callback2.net_address().Clone();

  const size_t kDatagramCount = 6;
  const size_t kDatagramSize = 255;

  // Test send using a connected socket.
  socket_->ReceiveMore(kDatagramCount);

  for (size_t i = 0; i < kDatagramCount; ++i) {
    TestCallback callback;
    client_socket->SendTo(
        nullptr,
        CreateTestMessage(static_cast<uint8_t>(i), kDatagramSize),
        callback.callback());
    callback.WaitForResult();
    EXPECT_EQ(255, callback.result()->code);
  }

  receiver_.WaitForReceiveResults(kDatagramCount);
  for (size_t i = 0; i < kDatagramCount; ++i) {
    scoped_ptr<ReceiveResult> result(receiver_.results()->front());
    receiver_.results()->pop();

    EXPECT_EQ(static_cast<int>(kDatagramSize), result->result->code);
    EXPECT_TRUE(result->addr.Equals(client_addr));
    EXPECT_TRUE(result->data.Equals(
        CreateTestMessage(static_cast<uint8_t>(i), kDatagramSize)));
  }

  // Test receive using a connected socket.
  client_socket->ReceiveMore(kDatagramCount);

  for (size_t i = 0; i < kDatagramCount; ++i) {
    TestCallback callback;
    socket_->SendTo(
        client_addr.Clone(),
        CreateTestMessage(static_cast<uint8_t>(i), kDatagramSize),
        callback.callback());
    callback.WaitForResult();
    EXPECT_EQ(255, callback.result()->code);
  }

  client_socket_receiver.WaitForReceiveResults(kDatagramCount);
  for (size_t i = 0; i < kDatagramCount; ++i) {
    scoped_ptr<ReceiveResult> result(client_socket_receiver.results()->front());
    client_socket_receiver.results()->pop();

    EXPECT_EQ(static_cast<int>(kDatagramSize), result->result->code);
    EXPECT_FALSE(result->addr);
    EXPECT_TRUE(result->data.Equals(
        CreateTestMessage(static_cast<uint8_t>(i), kDatagramSize)));
  }
}

TEST_F(UDPSocketAppTest, TestWrapperReadWrite) {
  UDPSocketWrapper socket(std::move(socket_), 4, 4);

  TestCallbackWithAddress callback1;
  socket.Bind(GetLocalHostWithAnyPort(), callback1.callback());
  callback1.WaitForResult();
  ASSERT_EQ(net::OK, callback1.result()->code);
  ASSERT_NE(0u, callback1.net_address()->ipv4->port);

  NetAddressPtr server_addr = callback1.net_address().Clone();

  UDPSocketPtr raw_client_socket;
  network_service_->CreateUDPSocket(GetProxy(&raw_client_socket));
  UDPSocketWrapper client_socket(std::move(raw_client_socket), 4, 4);

  TestCallbackWithAddress callback2;
  client_socket.Bind(GetLocalHostWithAnyPort(), callback2.callback());
  callback2.WaitForResult();
  ASSERT_EQ(net::OK, callback2.result()->code);
  ASSERT_NE(0u, callback2.net_address()->ipv4->port);

  NetAddressPtr client_addr = callback2.net_address().Clone();

  const size_t kDatagramCount = 16;
  const size_t kDatagramSize = 255;

  for (size_t i = 1; i < kDatagramCount; ++i) {
    scoped_ptr<TestCallback[]> send_callbacks(new TestCallback[i]);
    scoped_ptr<TestReceiveCallback[]> receive_callbacks(
        new TestReceiveCallback[i]);

    for (size_t j = 0; j < i; ++j) {
      client_socket.SendTo(
          server_addr.Clone(),
          CreateTestMessage(static_cast<uint8_t>(j), kDatagramSize),
          send_callbacks[j].callback());

      socket.ReceiveFrom(receive_callbacks[j].callback());
    }

    receive_callbacks[i - 1].WaitForResult();

    for (size_t j = 0; j < i; ++j) {
      EXPECT_EQ(static_cast<int>(kDatagramSize),
                receive_callbacks[j].result()->code);
      EXPECT_TRUE(receive_callbacks[j].src_addr().Equals(client_addr));
      EXPECT_TRUE(receive_callbacks[j].data().Equals(
          CreateTestMessage(static_cast<uint8_t>(j), kDatagramSize)));
    }
  }
}

TEST_F(UDPSocketAppTest, TestWrapperConnectedReadWrite) {
  UDPSocketWrapper socket(std::move(socket_), 4, 4);

  TestCallbackWithAddress callback1;
  socket.Bind(GetLocalHostWithAnyPort(), callback1.callback());
  callback1.WaitForResult();
  ASSERT_EQ(net::OK, callback1.result()->code);
  ASSERT_NE(0u, callback1.net_address()->ipv4->port);

  NetAddressPtr server_addr = callback1.net_address().Clone();

  UDPSocketPtr raw_client_socket;
  network_service_->CreateUDPSocket(GetProxy(&raw_client_socket));
  UDPSocketWrapper client_socket(std::move(raw_client_socket), 4, 4);

  TestCallbackWithAddress callback2;
  client_socket.Connect(std::move(server_addr), callback2.callback());
  callback2.WaitForResult();
  ASSERT_EQ(net::OK, callback2.result()->code);
  ASSERT_NE(0u, callback2.net_address()->ipv4->port);

  NetAddressPtr client_addr = callback2.net_address().Clone();

  const size_t kDatagramCount = 16;
  const size_t kDatagramSize = 255;

  // Test send using a connected socket.
  for (size_t i = 1; i < kDatagramCount; ++i) {
    scoped_ptr<TestCallback[]> send_callbacks(new TestCallback[i]);
    scoped_ptr<TestReceiveCallback[]> receive_callbacks(
        new TestReceiveCallback[i]);

    for (size_t j = 0; j < i; ++j) {
      client_socket.SendTo(
          nullptr,
          CreateTestMessage(static_cast<uint8_t>(j), kDatagramSize),
          send_callbacks[j].callback());

      socket.ReceiveFrom(receive_callbacks[j].callback());
    }

    receive_callbacks[i - 1].WaitForResult();

    for (size_t j = 0; j < i; ++j) {
      EXPECT_EQ(static_cast<int>(kDatagramSize),
                receive_callbacks[j].result()->code);
      EXPECT_TRUE(receive_callbacks[j].src_addr().Equals(client_addr));
      EXPECT_TRUE(receive_callbacks[j].data().Equals(
          CreateTestMessage(static_cast<uint8_t>(j), kDatagramSize)));
    }
  }

  // Test receive using a connected socket.
  for (size_t i = 1; i < kDatagramCount; ++i) {
    scoped_ptr<TestCallback[]> send_callbacks(new TestCallback[i]);
    scoped_ptr<TestReceiveCallback[]> receive_callbacks(
        new TestReceiveCallback[i]);

    for (size_t j = 0; j < i; ++j) {
      socket.SendTo(
          client_addr.Clone(),
          CreateTestMessage(static_cast<uint8_t>(j), kDatagramSize),
          send_callbacks[j].callback());

      client_socket.ReceiveFrom(receive_callbacks[j].callback());
    }

    receive_callbacks[i - 1].WaitForResult();

    for (size_t j = 0; j < i; ++j) {
      EXPECT_EQ(static_cast<int>(kDatagramSize),
                receive_callbacks[j].result()->code);
      EXPECT_FALSE(receive_callbacks[j].src_addr());
      EXPECT_TRUE(receive_callbacks[j].data().Equals(
          CreateTestMessage(static_cast<uint8_t>(j), kDatagramSize)));
    }
  }
}

}  // namespace service
}  // namespace mojo
