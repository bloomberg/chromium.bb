// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <queue>

#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "net/base/rand_callback.h"
#include "net/base/test_completion_callback.h"
#include "net/dns/mdns_client_impl.h"
#include "net/dns/record_rdata.h"
#include "net/udp/udp_client_socket.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::StrictMock;
using ::testing::NiceMock;
using ::testing::Exactly;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::_;

namespace net {

namespace {

const char kSamplePacket1[] = {
  // Header
  0x00, 0x00,               // ID is zeroed out
  0x81, 0x80,               // Standard query response, RA, no error
  0x00, 0x00,               // No questions (for simplicity)
  0x00, 0x02,               // 2 RRs (answers)
  0x00, 0x00,               // 0 authority RRs
  0x00, 0x00,               // 0 additional RRs

  // Answer 1
  0x07, '_', 'p', 'r', 'i', 'v', 'e', 't',
  0x04, '_', 't', 'c', 'p',
  0x05, 'l', 'o', 'c', 'a', 'l',
  0x00,
  0x00, 0x0c,        // TYPE is PTR.
  0x00, 0x01,        // CLASS is IN.
  0x00, 0x00,        // TTL (4 bytes) is 1 second;
  0x00, 0x01,
  0x00, 0x08,        // RDLENGTH is 8 bytes.
  0x05, 'h', 'e', 'l', 'l', 'o',
  0xc0, 0x0c,

  // Answer 2
  0x08, '_', 'p', 'r', 'i', 'n', 't', 'e', 'r',
  0xc0, 0x14,         // Pointer to "._tcp.local"
  0x00, 0x0c,        // TYPE is PTR.
  0x00, 0x01,        // CLASS is IN.
  0x00, 0x01,        // TTL (4 bytes) is 20 hours, 47 minutes, 49 seconds.
  0x24, 0x75,
  0x00, 0x08,        // RDLENGTH is 8 bytes.
  0x05, 'h', 'e', 'l', 'l', 'o',
  0xc0, 0x32
};

const char kCorruptedPacketBadQuestion[] = {
  // Header
  0x00, 0x00,               // ID is zeroed out
  0x81, 0x80,               // Standard query response, RA, no error
  0x00, 0x01,               // One question
  0x00, 0x02,               // 2 RRs (answers)
  0x00, 0x00,               // 0 authority RRs
  0x00, 0x00,               // 0 additional RRs

  // Question is corrupted and cannot be read.
  0x99, 'h', 'e', 'l', 'l', 'o',
  0x00,
  0x00, 0x00,
  0x00, 0x00,

  // Answer 1
  0x07, '_', 'p', 'r', 'i', 'v', 'e', 't',
  0x04, '_', 't', 'c', 'p',
  0x05, 'l', 'o', 'c', 'a', 'l',
  0x00,
  0x00, 0x0c,        // TYPE is PTR.
  0x00, 0x01,        // CLASS is IN.
  0x00, 0x01,        // TTL (4 bytes) is 20 hours, 47 minutes, 48 seconds.
  0x24, 0x74,
  0x00, 0x99,        // RDLENGTH is impossible
  0x05, 'h', 'e', 'l', 'l', 'o',
  0xc0, 0x0c,

  // Answer 2
  0x08, '_', 'p', 'r',  // Useless trailing data.
};

const char kCorruptedPacketUnsalvagable[] = {
  // Header
  0x00, 0x00,               // ID is zeroed out
  0x81, 0x80,               // Standard query response, RA, no error
  0x00, 0x00,               // No questions (for simplicity)
  0x00, 0x02,               // 2 RRs (answers)
  0x00, 0x00,               // 0 authority RRs
  0x00, 0x00,               // 0 additional RRs

  // Answer 1
  0x07, '_', 'p', 'r', 'i', 'v', 'e', 't',
  0x04, '_', 't', 'c', 'p',
  0x05, 'l', 'o', 'c', 'a', 'l',
  0x00,
  0x00, 0x0c,        // TYPE is PTR.
  0x00, 0x01,        // CLASS is IN.
  0x00, 0x01,        // TTL (4 bytes) is 20 hours, 47 minutes, 48 seconds.
  0x24, 0x74,
  0x00, 0x99,        // RDLENGTH is impossible
  0x05, 'h', 'e', 'l', 'l', 'o',
  0xc0, 0x0c,

  // Answer 2
  0x08, '_', 'p', 'r',  // Useless trailing data.
};

const char kCorruptedPacketSalvagable[] = {
  // Header
  0x00, 0x00,               // ID is zeroed out
  0x81, 0x80,               // Standard query response, RA, no error
  0x00, 0x00,               // No questions (for simplicity)
  0x00, 0x02,               // 2 RRs (answers)
  0x00, 0x00,               // 0 authority RRs
  0x00, 0x00,               // 0 additional RRs

  // Answer 1
  0x07, '_', 'p', 'r', 'i', 'v', 'e', 't',
  0x04, '_', 't', 'c', 'p',
  0x05, 'l', 'o', 'c', 'a', 'l',
  0x00,
  0x00, 0x0c,        // TYPE is PTR.
  0x00, 0x01,        // CLASS is IN.
  0x00, 0x01,        // TTL (4 bytes) is 20 hours, 47 minutes, 48 seconds.
  0x24, 0x74,
  0x00, 0x08,        // RDLENGTH is 8 bytes.
  0x99, 'h', 'e', 'l', 'l', 'o',   // Bad RDATA format.
  0xc0, 0x0c,

  // Answer 2
  0x08, '_', 'p', 'r', 'i', 'n', 't', 'e', 'r',
  0xc0, 0x14,         // Pointer to "._tcp.local"
  0x00, 0x0c,        // TYPE is PTR.
  0x00, 0x01,        // CLASS is IN.
  0x00, 0x01,        // TTL (4 bytes) is 20 hours, 47 minutes, 49 seconds.
  0x24, 0x75,
  0x00, 0x08,        // RDLENGTH is 8 bytes.
  0x05, 'h', 'e', 'l', 'l', 'o',
  0xc0, 0x32
};

const char kSamplePacket2[] = {
  // Header
  0x00, 0x00,               // ID is zeroed out
  0x81, 0x80,               // Standard query response, RA, no error
  0x00, 0x00,               // No questions (for simplicity)
  0x00, 0x02,               // 2 RRs (answers)
  0x00, 0x00,               // 0 authority RRs
  0x00, 0x00,               // 0 additional RRs

  // Answer 1
  0x07, '_', 'p', 'r', 'i', 'v', 'e', 't',
  0x04, '_', 't', 'c', 'p',
  0x05, 'l', 'o', 'c', 'a', 'l',
  0x00,
  0x00, 0x0c,        // TYPE is PTR.
  0x00, 0x01,        // CLASS is IN.
  0x00, 0x01,        // TTL (4 bytes) is 20 hours, 47 minutes, 48 seconds.
  0x24, 0x74,
  0x00, 0x08,        // RDLENGTH is 8 bytes.
  0x05, 'z', 'z', 'z', 'z', 'z',
  0xc0, 0x0c,

  // Answer 2
  0x08, '_', 'p', 'r', 'i', 'n', 't', 'e', 'r',
  0xc0, 0x14,         // Pointer to "._tcp.local"
  0x00, 0x0c,        // TYPE is PTR.
  0x00, 0x01,        // CLASS is IN.
  0x00, 0x01,        // TTL (4 bytes) is 20 hours, 47 minutes, 48 seconds.
  0x24, 0x74,
  0x00, 0x08,        // RDLENGTH is 8 bytes.
  0x05, 'z', 'z', 'z', 'z', 'z',
  0xc0, 0x32
};

const char kQueryPacketPrivet[] = {
  // Header
  0x00, 0x00,               // ID is zeroed out
  0x00, 0x00,               // No flags.
  0x00, 0x01,               // One question.
  0x00, 0x00,               // 0 RRs (answers)
  0x00, 0x00,               // 0 authority RRs
  0x00, 0x00,               // 0 additional RRs

  // Question
  // This part is echoed back from the respective query.
  0x07, '_', 'p', 'r', 'i', 'v', 'e', 't',
  0x04, '_', 't', 'c', 'p',
  0x05, 'l', 'o', 'c', 'a', 'l',
  0x00,
  0x00, 0x0c,        // TYPE is PTR.
  0x00, 0x01,        // CLASS is IN.
};

const char kSamplePacketAdditionalOnly[] = {
  // Header
  0x00, 0x00,               // ID is zeroed out
  0x81, 0x80,               // Standard query response, RA, no error
  0x00, 0x00,               // No questions (for simplicity)
  0x00, 0x00,               // 2 RRs (answers)
  0x00, 0x00,               // 0 authority RRs
  0x00, 0x01,               // 0 additional RRs

  // Answer 1
  0x07, '_', 'p', 'r', 'i', 'v', 'e', 't',
  0x04, '_', 't', 'c', 'p',
  0x05, 'l', 'o', 'c', 'a', 'l',
  0x00,
  0x00, 0x0c,        // TYPE is PTR.
  0x00, 0x01,        // CLASS is IN.
  0x00, 0x01,        // TTL (4 bytes) is 20 hours, 47 minutes, 48 seconds.
  0x24, 0x74,
  0x00, 0x08,        // RDLENGTH is 8 bytes.
  0x05, 'h', 'e', 'l', 'l', 'o',
  0xc0, 0x0c,
};

class MockDatagramServerSocket : public DatagramServerSocket {
 public:
  // DatagramServerSocket implementation:
  int Listen(const IPEndPoint& address) {
    return ListenInternal(address.ToString());
  }

  MOCK_METHOD1(ListenInternal, int(const std::string& address));

  MOCK_METHOD4(RecvFrom, int(IOBuffer* buffer, int size,
                             IPEndPoint* address,
                             const CompletionCallback& callback));

  int SendTo(IOBuffer* buf, int buf_len, const IPEndPoint& address,
             const CompletionCallback& callback) {
    return SendToInternal(std::string(buf->data(), buf_len), address.ToString(),
                          callback);
  }

  MOCK_METHOD3(SendToInternal, int(const std::string& packet,
                                   const std::string address,
                                   const CompletionCallback& callback));

  MOCK_METHOD1(SetReceiveBufferSize, bool(int32 size));
  MOCK_METHOD1(SetSendBufferSize, bool(int32 size));

  MOCK_METHOD0(Close, void());

  MOCK_CONST_METHOD1(GetPeerAddress, int(IPEndPoint* address));
  MOCK_CONST_METHOD1(GetLocalAddress, int(IPEndPoint* address));
  MOCK_CONST_METHOD0(NetLog, const BoundNetLog&());

  MOCK_METHOD0(AllowAddressReuse, void());
  MOCK_METHOD0(AllowBroadcast, void());

  int JoinGroup(const IPAddressNumber& group_address) const {
    return JoinGroupInternal(IPAddressToString(group_address));
  }

  MOCK_CONST_METHOD1(JoinGroupInternal, int(const std::string& group));

  int LeaveGroup(const IPAddressNumber& group_address) const {
    return LeaveGroupInternal(IPAddressToString(group_address));
  }

  MOCK_CONST_METHOD1(LeaveGroupInternal, int(const std::string& group));

  MOCK_METHOD1(SetMulticastTimeToLive, int(int ttl));

  MOCK_METHOD1(SetMulticastLoopbackMode, int(bool loopback));

  void SetResponsePacket(std::string response_packet) {
    response_packet_ = response_packet;
  }

  int HandleRecvNow(IOBuffer* buffer, int size, IPEndPoint* address,
                    const CompletionCallback& callback) {
    int size_returned =
        std::min(response_packet_.size(), static_cast<size_t>(size));
    memcpy(buffer->data(), response_packet_.data(), size_returned);
    return size_returned;
  }

  int HandleRecvLater(IOBuffer* buffer, int size, IPEndPoint* address,
                      const CompletionCallback& callback) {
    int rv = HandleRecvNow(buffer, size, address, callback);
    base::MessageLoop::current()->PostTask(FROM_HERE, base::Bind(callback, rv));
    return ERR_IO_PENDING;
  }

 private:
  std::string response_packet_;
};

class MockSocketFactory
    : public MDnsConnection::SocketFactory {
 public:
  MockSocketFactory() {
  }

  virtual ~MockSocketFactory() {
  }

  virtual scoped_ptr<DatagramServerSocket> CreateSocket() OVERRIDE {
    scoped_ptr<MockDatagramServerSocket> new_socket(
        new NiceMock<MockDatagramServerSocket>);

    ON_CALL(*new_socket, SendToInternal(_, _, _))
        .WillByDefault(Invoke(
            this,
            &MockSocketFactory::SendToInternal));

    ON_CALL(*new_socket, RecvFrom(_, _, _, _))
        .WillByDefault(Invoke(
            this,
            &MockSocketFactory::RecvFromInternal));

    return new_socket.PassAs<DatagramServerSocket>();
  }

  void SimulateReceive(const char* packet, int size) {
    DCHECK(recv_buffer_size_ >= size);
    DCHECK(recv_buffer_.get());
    DCHECK(!recv_callback_.is_null());

    memcpy(recv_buffer_->data(), packet, size);
    CompletionCallback recv_callback = recv_callback_;
    recv_callback_.Reset();
    recv_callback.Run(size);
  }

  MOCK_METHOD1(OnSendTo, void(const std::string&));

 private:
  int SendToInternal(const std::string& packet, const std::string& address,
                     const CompletionCallback& callback) {
    OnSendTo(packet);
    return packet.size();
  }

  // The latest receive callback is always saved, since the MDnsConnection
  // does not care which socket a packet is received on.
  int RecvFromInternal(IOBuffer* buffer, int size,
                       IPEndPoint* address,
                       const CompletionCallback& callback) {
    recv_buffer_ = buffer;
    recv_buffer_size_ = size;
    recv_callback_ = callback;
    return ERR_IO_PENDING;
  }

  scoped_refptr<IOBuffer> recv_buffer_;
  int recv_buffer_size_;
  CompletionCallback recv_callback_;
};

class PtrRecordCopyContainer {
 public:
  PtrRecordCopyContainer() {}
  ~PtrRecordCopyContainer() {}

  bool is_set() const { return set_; }

  void SaveWithDummyArg(int unused, const RecordParsed* value) {
    Save(value);
  }

  void Save(const RecordParsed* value) {
    set_ = true;
    name_ = value->name();
    ptrdomain_ = value->rdata<PtrRecordRdata>()->ptrdomain();
    ttl_ = value->ttl();
  }

  bool IsRecordWith(std::string name, std::string ptrdomain) {
    return set_ && name_ == name && ptrdomain_ == ptrdomain;
  }

  const std::string& name() { return name_; }
  const std::string& ptrdomain() { return ptrdomain_; }
  int ttl() { return ttl_; }

 private:
  bool set_;
  std::string name_;
  std::string ptrdomain_;
  int ttl_;
};

class MDnsTest : public ::testing::Test {
 public:
  MDnsTest();
  virtual ~MDnsTest();
  virtual void TearDown() OVERRIDE;
  void DeleteTransaction();
  void DeleteBothListeners();
  void RunFor(base::TimeDelta time_period);
  void Stop();

  MOCK_METHOD2(MockableRecordCallback, void(MDnsTransaction::Result result,
                                            const RecordParsed* record));

 protected:
  void ExpectPacket(const char* packet, unsigned size);
  void SimulatePacketReceive(const char* packet, unsigned size);

  scoped_ptr<MDnsClientImpl> test_client_;
  IPEndPoint mdns_ipv4_endpoint_;
  StrictMock<MockSocketFactory>* socket_factory_;

  // Transactions and listeners that can be deleted by class methods for
  // reentrancy tests.
  scoped_ptr<MDnsTransaction> transaction_;
  scoped_ptr<MDnsListener> listener1_;
  scoped_ptr<MDnsListener> listener2_;
};

class MockListenerDelegate : public MDnsListener::Delegate {
 public:
  MOCK_METHOD2(OnRecordUpdate,
               void(MDnsListener::UpdateType update,
                    const RecordParsed* records));
  MOCK_METHOD2(OnNsecRecord, void(const std::string&, unsigned));
  MOCK_METHOD0(OnCachePurged, void());
};

MDnsTest::MDnsTest() {
  socket_factory_ = new StrictMock<MockSocketFactory>();
  test_client_.reset(new MDnsClientImpl(
      scoped_ptr<MDnsConnection::SocketFactory>(socket_factory_)));
}

MDnsTest::~MDnsTest() {
}

void MDnsTest::TearDown() {
  base::MessageLoop::current()->RunUntilIdle();

  ASSERT_FALSE(test_client_->IsListeningForTests());

  base::MessageLoop::current()->AssertIdle();
}

void MDnsTest::SimulatePacketReceive(const char* packet, unsigned size) {
  socket_factory_->SimulateReceive(packet, size);
}

void MDnsTest::ExpectPacket(
    const char* packet,
    unsigned size) {
  EXPECT_CALL(*socket_factory_, OnSendTo(std::string(packet, size)))
      .Times(2);
}

void MDnsTest::DeleteTransaction() {
  transaction_.reset();
}

void MDnsTest::DeleteBothListeners() {
  listener1_.reset();
  listener2_.reset();
}

void MDnsTest::RunFor(base::TimeDelta time_period) {
  base::CancelableCallback<void()> callback(base::Bind(&MDnsTest::Stop,
                                                       base::Unretained(this)));
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE, callback.callback(), time_period);

  base::MessageLoop::current()->Run();
  callback.Cancel();
}

void MDnsTest::Stop() {
  base::MessageLoop::current()->Quit();
}

TEST_F(MDnsTest, PassiveListeners) {
  StrictMock<MockListenerDelegate> delegate_privet;
  StrictMock<MockListenerDelegate> delegate_printer;
  StrictMock<MockListenerDelegate> delegate_ptr;

  PtrRecordCopyContainer record_privet;
  PtrRecordCopyContainer record_printer;

  scoped_ptr<MDnsListener> listener_privet = test_client_->CreateListener(
      dns_protocol::kTypePTR, "_privet._tcp.local", &delegate_privet);
  scoped_ptr<MDnsListener> listener_printer = test_client_->CreateListener(
      dns_protocol::kTypePTR, "_printer._tcp.local", &delegate_printer);
  scoped_ptr<MDnsListener> listener_ptr = test_client_->CreateListener(
      dns_protocol::kTypePTR, "", &delegate_ptr);

  ASSERT_TRUE(listener_privet->Start());
  ASSERT_TRUE(listener_printer->Start());
  ASSERT_TRUE(listener_ptr->Start());

  ASSERT_TRUE(test_client_->IsListeningForTests());

  // Send the same packet twice to ensure no records are double-counted.

  EXPECT_CALL(delegate_privet, OnRecordUpdate(MDnsListener::RECORD_ADDED, _))
      .Times(Exactly(1))
      .WillOnce(Invoke(
          &record_privet,
          &PtrRecordCopyContainer::SaveWithDummyArg));

  EXPECT_CALL(delegate_printer, OnRecordUpdate(MDnsListener::RECORD_ADDED, _))
      .Times(Exactly(1))
      .WillOnce(Invoke(
          &record_printer,
          &PtrRecordCopyContainer::SaveWithDummyArg));

  EXPECT_CALL(delegate_ptr, OnRecordUpdate(MDnsListener::RECORD_ADDED, _))
      .Times(Exactly(2));

  SimulatePacketReceive(kSamplePacket1, sizeof(kSamplePacket1));
  SimulatePacketReceive(kSamplePacket1, sizeof(kSamplePacket1));

  EXPECT_TRUE(record_privet.IsRecordWith("_privet._tcp.local",
                                         "hello._privet._tcp.local"));

  EXPECT_TRUE(record_printer.IsRecordWith("_printer._tcp.local",
                                          "hello._printer._tcp.local"));

  listener_privet.reset();
  listener_printer.reset();

  ASSERT_TRUE(test_client_->IsListeningForTests());

  EXPECT_CALL(delegate_ptr, OnRecordUpdate(MDnsListener::RECORD_ADDED, _))
      .Times(Exactly(2));

  SimulatePacketReceive(kSamplePacket2, sizeof(kSamplePacket2));
}

TEST_F(MDnsTest, PassiveListenersCacheCleanup) {
  StrictMock<MockListenerDelegate> delegate_privet;

  PtrRecordCopyContainer record_privet;
  PtrRecordCopyContainer record_privet2;

  scoped_ptr<MDnsListener> listener_privet = test_client_->CreateListener(
      dns_protocol::kTypePTR, "_privet._tcp.local", &delegate_privet);

  ASSERT_TRUE(listener_privet->Start());

  ASSERT_TRUE(test_client_->IsListeningForTests());

  EXPECT_CALL(delegate_privet, OnRecordUpdate(MDnsListener::RECORD_ADDED, _))
      .Times(Exactly(1))
      .WillOnce(Invoke(
          &record_privet,
          &PtrRecordCopyContainer::SaveWithDummyArg));

  SimulatePacketReceive(kSamplePacket1, sizeof(kSamplePacket1));

  EXPECT_TRUE(record_privet.IsRecordWith("_privet._tcp.local",
                                         "hello._privet._tcp.local"));

  // Expect record is removed when its TTL expires.
  EXPECT_CALL(delegate_privet, OnRecordUpdate(MDnsListener::RECORD_REMOVED, _))
      .Times(Exactly(1))
      .WillOnce(DoAll(InvokeWithoutArgs(this, &MDnsTest::Stop),
                      Invoke(&record_privet2,
                             &PtrRecordCopyContainer::SaveWithDummyArg)));

  RunFor(base::TimeDelta::FromSeconds(record_privet.ttl() + 1));

  EXPECT_TRUE(record_privet2.IsRecordWith("_privet._tcp.local",
                                          "hello._privet._tcp.local"));
}

TEST_F(MDnsTest, MalformedPacket) {
  StrictMock<MockListenerDelegate> delegate_printer;

  PtrRecordCopyContainer record_printer;

  scoped_ptr<MDnsListener> listener_printer = test_client_->CreateListener(
      dns_protocol::kTypePTR, "_printer._tcp.local", &delegate_printer);

  ASSERT_TRUE(listener_printer->Start());

  ASSERT_TRUE(test_client_->IsListeningForTests());

  EXPECT_CALL(delegate_printer, OnRecordUpdate(MDnsListener::RECORD_ADDED, _))
      .Times(Exactly(1))
      .WillOnce(Invoke(
          &record_printer,
          &PtrRecordCopyContainer::SaveWithDummyArg));

  // First, send unsalvagable packet to ensure we can deal with it.
  SimulatePacketReceive(kCorruptedPacketUnsalvagable,
                        sizeof(kCorruptedPacketUnsalvagable));

  // Regression test: send a packet where the question cannot be read.
  SimulatePacketReceive(kCorruptedPacketBadQuestion,
                        sizeof(kCorruptedPacketBadQuestion));

  // Then send salvagable packet to ensure we can extract useful records.
  SimulatePacketReceive(kCorruptedPacketSalvagable,
                        sizeof(kCorruptedPacketSalvagable));

  EXPECT_TRUE(record_printer.IsRecordWith("_printer._tcp.local",
                                          "hello._printer._tcp.local"));
}

TEST_F(MDnsTest, TransactionWithEmptyCache) {
  ExpectPacket(kQueryPacketPrivet, sizeof(kQueryPacketPrivet));

  scoped_ptr<MDnsTransaction> transaction_privet =
      test_client_->CreateTransaction(
          dns_protocol::kTypePTR, "_privet._tcp.local",
          MDnsTransaction::QUERY_NETWORK |
          MDnsTransaction::QUERY_CACHE |
          MDnsTransaction::SINGLE_RESULT,
          base::Bind(&MDnsTest::MockableRecordCallback,
                     base::Unretained(this)));

  ASSERT_TRUE(transaction_privet->Start());

  EXPECT_TRUE(test_client_->IsListeningForTests());

  PtrRecordCopyContainer record_privet;

  EXPECT_CALL(*this, MockableRecordCallback(MDnsTransaction::RESULT_RECORD, _))
      .Times(Exactly(1))
      .WillOnce(Invoke(&record_privet,
                       &PtrRecordCopyContainer::SaveWithDummyArg));

  SimulatePacketReceive(kSamplePacket1, sizeof(kSamplePacket1));

  EXPECT_TRUE(record_privet.IsRecordWith("_privet._tcp.local",
                                         "hello._privet._tcp.local"));
}

TEST_F(MDnsTest, TransactionCacheOnlyNoResult) {
  scoped_ptr<MDnsTransaction> transaction_privet =
      test_client_->CreateTransaction(
          dns_protocol::kTypePTR, "_privet._tcp.local",
          MDnsTransaction::QUERY_CACHE |
          MDnsTransaction::SINGLE_RESULT,
          base::Bind(&MDnsTest::MockableRecordCallback,
                     base::Unretained(this)));

  EXPECT_CALL(*this,
              MockableRecordCallback(MDnsTransaction::RESULT_NO_RESULTS, _))
      .Times(Exactly(1));

  ASSERT_TRUE(transaction_privet->Start());

  EXPECT_FALSE(test_client_->IsListeningForTests());
}

TEST_F(MDnsTest, TransactionWithCache) {
  // Listener to force the client to listen
  StrictMock<MockListenerDelegate> delegate_irrelevant;
  scoped_ptr<MDnsListener> listener_irrelevant = test_client_->CreateListener(
      dns_protocol::kTypeA, "codereview.chromium.local",
      &delegate_irrelevant);

  ASSERT_TRUE(listener_irrelevant->Start());

  EXPECT_TRUE(test_client_->IsListeningForTests());

  SimulatePacketReceive(kSamplePacket1, sizeof(kSamplePacket1));


  PtrRecordCopyContainer record_privet;

  EXPECT_CALL(*this, MockableRecordCallback(MDnsTransaction::RESULT_RECORD, _))
      .WillOnce(Invoke(&record_privet,
                       &PtrRecordCopyContainer::SaveWithDummyArg));

  scoped_ptr<MDnsTransaction> transaction_privet =
      test_client_->CreateTransaction(
          dns_protocol::kTypePTR, "_privet._tcp.local",
          MDnsTransaction::QUERY_NETWORK |
          MDnsTransaction::QUERY_CACHE |
          MDnsTransaction::SINGLE_RESULT,
          base::Bind(&MDnsTest::MockableRecordCallback,
                     base::Unretained(this)));

  ASSERT_TRUE(transaction_privet->Start());

  EXPECT_TRUE(record_privet.IsRecordWith("_privet._tcp.local",
                                         "hello._privet._tcp.local"));
}

TEST_F(MDnsTest, AdditionalRecords) {
  StrictMock<MockListenerDelegate> delegate_privet;

  PtrRecordCopyContainer record_privet;

  scoped_ptr<MDnsListener> listener_privet = test_client_->CreateListener(
      dns_protocol::kTypePTR, "_privet._tcp.local",
      &delegate_privet);

  ASSERT_TRUE(listener_privet->Start());

  ASSERT_TRUE(test_client_->IsListeningForTests());

  EXPECT_CALL(delegate_privet, OnRecordUpdate(MDnsListener::RECORD_ADDED, _))
      .Times(Exactly(1))
      .WillOnce(Invoke(
          &record_privet,
          &PtrRecordCopyContainer::SaveWithDummyArg));

  SimulatePacketReceive(kSamplePacketAdditionalOnly, sizeof(kSamplePacket1));

  EXPECT_TRUE(record_privet.IsRecordWith("_privet._tcp.local",
                                         "hello._privet._tcp.local"));
}

TEST_F(MDnsTest, TransactionTimeout) {
  ExpectPacket(kQueryPacketPrivet, sizeof(kQueryPacketPrivet));

  scoped_ptr<MDnsTransaction> transaction_privet =
      test_client_->CreateTransaction(
          dns_protocol::kTypePTR, "_privet._tcp.local",
          MDnsTransaction::QUERY_NETWORK |
          MDnsTransaction::QUERY_CACHE |
          MDnsTransaction::SINGLE_RESULT,
          base::Bind(&MDnsTest::MockableRecordCallback,
                     base::Unretained(this)));

  ASSERT_TRUE(transaction_privet->Start());

  EXPECT_TRUE(test_client_->IsListeningForTests());

  EXPECT_CALL(*this,
              MockableRecordCallback(MDnsTransaction::RESULT_NO_RESULTS, NULL))
      .Times(Exactly(1))
      .WillOnce(InvokeWithoutArgs(this, &MDnsTest::Stop));

  RunFor(base::TimeDelta::FromSeconds(4));
}

TEST_F(MDnsTest, TransactionMultipleRecords) {
  ExpectPacket(kQueryPacketPrivet, sizeof(kQueryPacketPrivet));

  scoped_ptr<MDnsTransaction> transaction_privet =
      test_client_->CreateTransaction(
          dns_protocol::kTypePTR, "_privet._tcp.local",
          MDnsTransaction::QUERY_NETWORK |
          MDnsTransaction::QUERY_CACHE ,
          base::Bind(&MDnsTest::MockableRecordCallback,
                     base::Unretained(this)));

  ASSERT_TRUE(transaction_privet->Start());

  EXPECT_TRUE(test_client_->IsListeningForTests());

  PtrRecordCopyContainer record_privet;
  PtrRecordCopyContainer record_privet2;

  EXPECT_CALL(*this, MockableRecordCallback(MDnsTransaction::RESULT_RECORD, _))
      .Times(Exactly(2))
      .WillOnce(Invoke(&record_privet,
                       &PtrRecordCopyContainer::SaveWithDummyArg))
      .WillOnce(Invoke(&record_privet2,
                       &PtrRecordCopyContainer::SaveWithDummyArg));

  SimulatePacketReceive(kSamplePacket1, sizeof(kSamplePacket1));
  SimulatePacketReceive(kSamplePacket2, sizeof(kSamplePacket2));

  EXPECT_TRUE(record_privet.IsRecordWith("_privet._tcp.local",
                                         "hello._privet._tcp.local"));

  EXPECT_TRUE(record_privet2.IsRecordWith("_privet._tcp.local",
                                          "zzzzz._privet._tcp.local"));

  EXPECT_CALL(*this, MockableRecordCallback(MDnsTransaction::RESULT_DONE, NULL))
      .WillOnce(InvokeWithoutArgs(this, &MDnsTest::Stop));

  RunFor(base::TimeDelta::FromSeconds(4));
}

TEST_F(MDnsTest, TransactionReentrantDelete) {
  ExpectPacket(kQueryPacketPrivet, sizeof(kQueryPacketPrivet));

  transaction_ = test_client_->CreateTransaction(
      dns_protocol::kTypePTR, "_privet._tcp.local",
      MDnsTransaction::QUERY_NETWORK |
      MDnsTransaction::QUERY_CACHE |
      MDnsTransaction::SINGLE_RESULT,
      base::Bind(&MDnsTest::MockableRecordCallback,
                 base::Unretained(this)));

  ASSERT_TRUE(transaction_->Start());

  EXPECT_TRUE(test_client_->IsListeningForTests());

  EXPECT_CALL(*this, MockableRecordCallback(MDnsTransaction::RESULT_NO_RESULTS,
                                            NULL))
      .Times(Exactly(1))
      .WillOnce(DoAll(InvokeWithoutArgs(this, &MDnsTest::DeleteTransaction),
                      InvokeWithoutArgs(this, &MDnsTest::Stop)));

  RunFor(base::TimeDelta::FromSeconds(4));

  EXPECT_EQ(NULL, transaction_.get());
}

TEST_F(MDnsTest, TransactionReentrantDeleteFromCache) {
  StrictMock<MockListenerDelegate> delegate_irrelevant;
  scoped_ptr<MDnsListener> listener_irrelevant = test_client_->CreateListener(
      dns_protocol::kTypeA, "codereview.chromium.local",
      &delegate_irrelevant);
  ASSERT_TRUE(listener_irrelevant->Start());

  ASSERT_TRUE(test_client_->IsListeningForTests());

  SimulatePacketReceive(kSamplePacket1, sizeof(kSamplePacket1));

  transaction_ = test_client_->CreateTransaction(
      dns_protocol::kTypePTR, "_privet._tcp.local",
      MDnsTransaction::QUERY_NETWORK |
      MDnsTransaction::QUERY_CACHE,
      base::Bind(&MDnsTest::MockableRecordCallback,
                 base::Unretained(this)));

  EXPECT_CALL(*this, MockableRecordCallback(MDnsTransaction::RESULT_RECORD, _))
      .Times(Exactly(1))
      .WillOnce(InvokeWithoutArgs(this, &MDnsTest::DeleteTransaction));

  ASSERT_TRUE(transaction_->Start());

  EXPECT_EQ(NULL, transaction_.get());
}

// In order to reliably test reentrant listener deletes, we create two listeners
// and have each of them delete both, so we're guaranteed to try and deliver a
// callback to at least one deleted listener.

TEST_F(MDnsTest, ListenerReentrantDelete) {
  StrictMock<MockListenerDelegate> delegate_privet;

  listener1_ = test_client_->CreateListener(
      dns_protocol::kTypePTR, "_privet._tcp.local",
      &delegate_privet);

  listener2_ = test_client_->CreateListener(
      dns_protocol::kTypePTR, "_privet._tcp.local",
      &delegate_privet);

  ASSERT_TRUE(listener1_->Start());

  ASSERT_TRUE(listener2_->Start());

  EXPECT_CALL(delegate_privet, OnRecordUpdate(MDnsListener::RECORD_ADDED, _))
      .Times(Exactly(1))
      .WillOnce(InvokeWithoutArgs(this, &MDnsTest::DeleteBothListeners));

  EXPECT_TRUE(test_client_->IsListeningForTests());

  SimulatePacketReceive(kSamplePacket1, sizeof(kSamplePacket1));

  EXPECT_EQ(NULL, listener1_.get());
  EXPECT_EQ(NULL, listener2_.get());
}

// Note: These tests assume that the ipv4 socket will always be created first.
// This is a simplifying assumption based on the way the code works now.

class SimpleMockSocketFactory
    : public MDnsConnection::SocketFactory {
 public:
  SimpleMockSocketFactory() {
  }
  virtual ~SimpleMockSocketFactory() {
  }

  virtual scoped_ptr<DatagramServerSocket> CreateSocket() OVERRIDE {
    scoped_ptr<MockDatagramServerSocket> socket(
        new StrictMock<MockDatagramServerSocket>);
    sockets_.push(socket.get());
    return socket.PassAs<DatagramServerSocket>();
  }

  MockDatagramServerSocket* PopFirstSocket() {
    MockDatagramServerSocket* socket = sockets_.front();
    sockets_.pop();
    return socket;
  }

  size_t num_sockets() {
    return sockets_.size();
  }

 private:
  std::queue<MockDatagramServerSocket*> sockets_;
};

class MockMDnsConnectionDelegate : public MDnsConnection::Delegate {
 public:
  virtual void HandlePacket(DnsResponse* response, int size) {
    HandlePacketInternal(std::string(response->io_buffer()->data(), size));
  }

  MOCK_METHOD1(HandlePacketInternal, void(std::string packet));

  MOCK_METHOD1(OnConnectionError, void(int error));
};

class MDnsConnectionTest : public ::testing::Test {
 public:
  MDnsConnectionTest() : connection_(&factory_, &delegate_) {
  }

 protected:
  // Follow successful connection initialization.
  virtual void SetUp() OVERRIDE {
    ASSERT_EQ(2u, factory_.num_sockets());

    socket_ipv4_ = factory_.PopFirstSocket();
    socket_ipv6_ = factory_.PopFirstSocket();
  }

  bool InitConnection() {
    EXPECT_CALL(*socket_ipv4_, AllowAddressReuse());
    EXPECT_CALL(*socket_ipv6_, AllowAddressReuse());

    EXPECT_CALL(*socket_ipv4_, SetMulticastLoopbackMode(false));
    EXPECT_CALL(*socket_ipv6_, SetMulticastLoopbackMode(false));

    EXPECT_CALL(*socket_ipv4_, ListenInternal("0.0.0.0:5353"))
        .WillOnce(Return(OK));
    EXPECT_CALL(*socket_ipv6_, ListenInternal("[::]:5353"))
        .WillOnce(Return(OK));

    EXPECT_CALL(*socket_ipv4_, JoinGroupInternal("224.0.0.251"))
        .WillOnce(Return(OK));
    EXPECT_CALL(*socket_ipv6_, JoinGroupInternal("ff02::fb"))
        .WillOnce(Return(OK));

    return connection_.Init() == OK;
  }

  StrictMock<MockMDnsConnectionDelegate> delegate_;

  MockDatagramServerSocket* socket_ipv4_;
  MockDatagramServerSocket* socket_ipv6_;
  SimpleMockSocketFactory factory_;
  MDnsConnection connection_;
  TestCompletionCallback callback_;
};

TEST_F(MDnsConnectionTest, ReceiveSynchronous) {
  std::string sample_packet =
      std::string(kSamplePacket1, sizeof(kSamplePacket1));

  socket_ipv6_->SetResponsePacket(sample_packet);
  EXPECT_CALL(*socket_ipv4_, RecvFrom(_, _, _, _))
      .WillOnce(Return(ERR_IO_PENDING));
  EXPECT_CALL(*socket_ipv6_, RecvFrom(_, _, _, _))
      .WillOnce(
          Invoke(socket_ipv6_, &MockDatagramServerSocket::HandleRecvNow))
      .WillOnce(Return(ERR_IO_PENDING));

  EXPECT_CALL(delegate_, HandlePacketInternal(sample_packet));

  ASSERT_TRUE(InitConnection());
}

TEST_F(MDnsConnectionTest, ReceiveAsynchronous) {
  std::string sample_packet =
      std::string(kSamplePacket1, sizeof(kSamplePacket1));
  socket_ipv6_->SetResponsePacket(sample_packet);
  EXPECT_CALL(*socket_ipv4_, RecvFrom(_, _, _, _))
      .WillOnce(Return(ERR_IO_PENDING));
  EXPECT_CALL(*socket_ipv6_, RecvFrom(_, _, _, _))
      .WillOnce(
          Invoke(socket_ipv6_, &MockDatagramServerSocket::HandleRecvLater))
      .WillOnce(Return(ERR_IO_PENDING));

  ASSERT_TRUE(InitConnection());

  EXPECT_CALL(delegate_, HandlePacketInternal(sample_packet));

  base::MessageLoop::current()->RunUntilIdle();
}

TEST_F(MDnsConnectionTest, Send) {
  std::string sample_packet =
      std::string(kSamplePacket1, sizeof(kSamplePacket1));

  scoped_refptr<IOBufferWithSize> buf(
      new IOBufferWithSize(sizeof kSamplePacket1));
  memcpy(buf->data(), kSamplePacket1, sizeof(kSamplePacket1));

  EXPECT_CALL(*socket_ipv4_, RecvFrom(_, _, _, _))
      .WillOnce(Return(ERR_IO_PENDING));
  EXPECT_CALL(*socket_ipv6_, RecvFrom(_, _, _, _))
      .WillOnce(Return(ERR_IO_PENDING));

  ASSERT_TRUE(InitConnection());

  EXPECT_CALL(*socket_ipv4_,
              SendToInternal(sample_packet, "224.0.0.251:5353", _));
  EXPECT_CALL(*socket_ipv6_,
              SendToInternal(sample_packet, "[ff02::fb]:5353", _));

  connection_.Send(buf, buf->size());
}

TEST_F(MDnsConnectionTest, Error) {
  CompletionCallback callback;

  EXPECT_CALL(*socket_ipv4_, RecvFrom(_, _, _, _))
      .WillOnce(Return(ERR_IO_PENDING));
  EXPECT_CALL(*socket_ipv6_, RecvFrom(_, _, _, _))
      .WillOnce(DoAll(SaveArg<3>(&callback),  Return(ERR_IO_PENDING)));

  ASSERT_TRUE(InitConnection());

  EXPECT_CALL(delegate_, OnConnectionError(ERR_SOCKET_NOT_CONNECTED));
  callback.Run(ERR_SOCKET_NOT_CONNECTED);
}

}  // namespace

}  // namespace net
