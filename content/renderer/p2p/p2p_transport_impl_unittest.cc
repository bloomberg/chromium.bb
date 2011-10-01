// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/p2p/p2p_transport_impl.h"

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/test/test_timeouts.h"
#include "jingle/glue/fake_network_manager.h"
#include "jingle/glue/fake_socket_factory.h"
#include "jingle/glue/thread_wrapper.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/socket/socket.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AtMost;
using testing::Exactly;
using testing::InvokeWithoutArgs;

using webkit_glue::P2PTransport;

namespace {
const char kTestAddress1[] = "192.168.15.12";
const char kTestAddress2[] = "192.168.15.33";

const char kTransportName1[] = "tr1";
const char kTransportName2[] = "tr2";

// Send 10 packets 10 bytes each. Packets are sent with 10ms delay
// between packets (about 100 ms for 10 messages).
const int kMessageSize = 10;
const int kMessages = 10;
const int kUdpWriteDelayMs = 10;
const int kTcpDataSize = 10 * 1024;
const int kTcpWriteDelayMs = 1;

class UdpChannelTester : public base::RefCountedThreadSafe<UdpChannelTester> {
 public:
  UdpChannelTester(MessageLoop* message_loop,
                net::Socket* write_socket,
                net::Socket* read_socket)
      : message_loop_(message_loop),
        write_socket_(write_socket),
        read_socket_(read_socket),
        done_(false),
        ALLOW_THIS_IN_INITIALIZER_LIST(
            write_cb_(this, &UdpChannelTester::OnWritten)),
        ALLOW_THIS_IN_INITIALIZER_LIST(
            read_cb_(this, &UdpChannelTester::OnRead)),
        write_errors_(0),
        read_errors_(0),
        packets_sent_(0),
        packets_received_(0),
        broken_packets_(0) {
  }

  virtual ~UdpChannelTester() { }

  void Start() {
    message_loop_->PostTask(
        FROM_HERE, NewRunnableMethod(this, &UdpChannelTester::DoStart));
  }

  void CheckResults() {
    EXPECT_EQ(0, write_errors_);
    EXPECT_EQ(0, read_errors_);

    EXPECT_EQ(0, broken_packets_);

    // Verify that we've received at least one packet.
    EXPECT_GT(packets_received_, 0);
    LOG(INFO) << "Received " << packets_received_ << " packets out of "
              << kMessages;
  }

 protected:
  void Done() {
    done_ = true;
    message_loop_->PostTask(FROM_HERE, new MessageLoop::QuitTask());
  }

  void DoStart() {
    DoRead();
    DoWrite();
  }

  void DoWrite() {
    if (packets_sent_ >= kMessages) {
      Done();
      return;
    }

    scoped_refptr<net::IOBuffer> packet(new net::IOBuffer(kMessageSize));
    memset(packet->data(), 123, kMessageSize);
    sent_packets_[packets_sent_] = packet;
    // Put index of this packet in the beginning of the packet body.
    memcpy(packet->data(), &packets_sent_, sizeof(packets_sent_));

    int result = write_socket_->Write(packet, kMessageSize, &write_cb_);
    HandleWriteResult(result);
  }

  void OnWritten(int result) {
    HandleWriteResult(result);
  }

  void HandleWriteResult(int result) {
    if (result <= 0 && result != net::ERR_IO_PENDING) {
      LOG(ERROR) << "Received error " << result << " when trying to write";
      write_errors_++;
      Done();
    } else if (result > 0) {
      EXPECT_EQ(kMessageSize, result);
      packets_sent_++;
      message_loop_->PostDelayedTask(
          FROM_HERE, NewRunnableMethod(this, &UdpChannelTester::DoWrite),
          kUdpWriteDelayMs);
    }
  }

  void DoRead() {
    int result = 1;
    while (result > 0) {
      int kReadSize = kMessageSize * 2;
      read_buffer_ = new net::IOBuffer(kReadSize);

      result = read_socket_->Read(read_buffer_, kReadSize, &read_cb_);
      HandleReadResult(result);
    };
  }

  void OnRead(int result) {
    HandleReadResult(result);
    DoRead();
  }

  void HandleReadResult(int result) {
    if (result <= 0 && result != net::ERR_IO_PENDING) {
      // Error will be received after the socket is closed.
      if (!done_) {
        LOG(ERROR) << "Received error " << result << " when trying to read";
        read_errors_++;
        Done();
      }
    } else if (result > 0) {
      packets_received_++;
      if (kMessageSize != result) {
        // Invalid packet size;
        broken_packets_++;
      } else {
        // Validate packet body.
        int packet_id;
        memcpy(&packet_id, read_buffer_->data(), sizeof(packet_id));
        if (packet_id < 0 || packet_id >= kMessages) {
          broken_packets_++;
        } else {
          if (memcmp(read_buffer_->data(), sent_packets_[packet_id]->data(),
                     kMessageSize) != 0)
            broken_packets_++;
        }
      }
    }
  }

 private:
  MessageLoop* message_loop_;
  net::Socket* write_socket_;
  net::Socket* read_socket_;
  bool done_;

  scoped_refptr<net::IOBuffer> sent_packets_[kMessages];
  scoped_refptr<net::IOBuffer> read_buffer_;

  net::OldCompletionCallbackImpl<UdpChannelTester> write_cb_;
  net::OldCompletionCallbackImpl<UdpChannelTester> read_cb_;
  int write_errors_;
  int read_errors_;
  int packets_sent_;
  int packets_received_;
  int broken_packets_;
};

class TcpChannelTester : public base::RefCountedThreadSafe<TcpChannelTester> {
 public:
  TcpChannelTester(MessageLoop* message_loop,
                net::Socket* write_socket,
                net::Socket* read_socket)
      : message_loop_(message_loop),
        write_socket_(write_socket),
        read_socket_(read_socket),
        done_(false),
        ALLOW_THIS_IN_INITIALIZER_LIST(
            write_cb_(this, &TcpChannelTester::OnWritten)),
        ALLOW_THIS_IN_INITIALIZER_LIST(
            read_cb_(this, &TcpChannelTester::OnRead)),
        write_errors_(0),
        read_errors_(0) {
  }

  virtual ~TcpChannelTester() { }

  void Init() {
    // Initialize |send_buffer_|.
    send_buffer_ = new net::DrainableIOBuffer(new net::IOBuffer(kTcpDataSize),
                                              kTcpDataSize);
    for (int i = 0; i < kTcpDataSize; ++i) {
      send_buffer_->data()[i] = rand() % 256;
    }
  }

  void StartRead() {
    message_loop_->PostTask(
        FROM_HERE, NewRunnableMethod(this, &TcpChannelTester::DoRead));
  }

  void StartWrite() {
    message_loop_->PostTask(
        FROM_HERE, NewRunnableMethod(this, &TcpChannelTester::DoWrite));
  }

  void CheckResults() {
    EXPECT_EQ(0, write_errors_);
    EXPECT_EQ(0, read_errors_);

    EXPECT_EQ(0, send_buffer_->BytesRemaining());

    send_buffer_->SetOffset(0);
    EXPECT_EQ(kTcpDataSize, static_cast<int>(received_data_.size()));
    EXPECT_EQ(0, memcmp(send_buffer_->data(),
                        &received_data_[0], received_data_.size()));
  }

 protected:
  void Done() {
    done_ = true;
    message_loop_->PostTask(FROM_HERE, new MessageLoop::QuitTask());
  }

  void DoWrite() {
    if (send_buffer_->BytesRemaining() == 0) {
      return;
    }

    int result = write_socket_->Write(
        send_buffer_, send_buffer_->BytesRemaining(), &write_cb_);
    HandleWriteResult(result);
  }

  void OnWritten(int result) {
    HandleWriteResult(result);
  }

  void HandleWriteResult(int result) {
    if (result <= 0 && result != net::ERR_IO_PENDING) {
      LOG(ERROR) << "Received error " << result << " when trying to write";
      write_errors_++;
      Done();
    } else if (result > 0) {
      send_buffer_->DidConsume(result);
      message_loop_->PostDelayedTask(
          FROM_HERE, NewRunnableMethod(this, &TcpChannelTester::DoWrite),
          kTcpWriteDelayMs);
    }
  }

  void DoRead() {
    int result = 1;
    while (result > 0) {
      int kReadSize = kMessageSize * 2;
      read_buffer_ = new net::IOBuffer(kReadSize);

      result = read_socket_->Read(read_buffer_, kReadSize, &read_cb_);
      HandleReadResult(result);
    };
  }

  void OnRead(int result) {
    HandleReadResult(result);
    DoRead();
  }

  void HandleReadResult(int result) {
    if (result <= 0 && result != net::ERR_IO_PENDING) {
      // Error will be received after the socket is closed.
      if (!done_) {
        LOG(ERROR) << "Received error " << result << " when trying to read";
        read_errors_++;
        Done();
      }
    } else if (result > 0) {
      received_data_.insert(received_data_.end(), read_buffer_->data(),
                            read_buffer_->data() + result);
      if (static_cast<int>(received_data_.size()) == kTcpDataSize)
        Done();
    }
  }

 private:
  MessageLoop* message_loop_;
  net::Socket* write_socket_;
  net::Socket* read_socket_;
  bool done_;

  scoped_refptr<net::DrainableIOBuffer> send_buffer_;
  scoped_refptr<net::IOBuffer> read_buffer_;

  std::vector<char> sent_data_;
  std::vector<char> received_data_;

  net::OldCompletionCallbackImpl<TcpChannelTester> write_cb_;
  net::OldCompletionCallbackImpl<TcpChannelTester> read_cb_;
  int write_errors_;
  int read_errors_;
};

}  // namespace

namespace content {

class MockP2PEventHandler : public P2PTransport::EventHandler {
 public:
  MOCK_METHOD1(OnCandidateReady, void(const std::string& address));
  MOCK_METHOD1(OnStateChange, void(P2PTransport::State state));
  MOCK_METHOD1(OnError, void(int error));
};

class P2PTransportImplTest : public testing::Test {
 public:

 protected:
  virtual void SetUp() OVERRIDE {
    socket_manager_ = new jingle_glue::FakeSocketManager();

    net::IPAddressNumber ip;
    ASSERT_TRUE(net::ParseIPLiteralToNumber(kTestAddress1, &ip));
    transport1_.reset(new P2PTransportImpl(
        new jingle_glue::FakeNetworkManager(ip),
        new jingle_glue::FakeSocketFactory(socket_manager_, ip)));

    ASSERT_TRUE(net::ParseIPLiteralToNumber(kTestAddress2, &ip));
    transport2_.reset(new P2PTransportImpl(
        new jingle_glue::FakeNetworkManager(ip),
        new jingle_glue::FakeSocketFactory(socket_manager_, ip)));
  }

  void Init(P2PTransport::Protocol protocol) {
    P2PTransport::Config config;
    ASSERT_TRUE(transport1_->Init(
        NULL, kTransportName1, protocol, config, &event_handler1_));
    ASSERT_TRUE(transport2_->Init(
        NULL, kTransportName2, protocol, config, &event_handler2_));
  }

  MessageLoop message_loop_;

  scoped_refptr<jingle_glue::FakeSocketManager> socket_manager_;
  scoped_ptr<P2PTransportImpl> transport1_;
  MockP2PEventHandler event_handler1_;
  scoped_ptr<P2PTransportImpl> transport2_;
  MockP2PEventHandler event_handler2_;
};

TEST_F(P2PTransportImplTest, Create) {
  Init(P2PTransport::PROTOCOL_UDP);

  EXPECT_CALL(event_handler1_, OnCandidateReady(_));
  EXPECT_CALL(event_handler2_, OnCandidateReady(_));

  message_loop_.RunAllPending();
}

ACTION_P(AddRemoteCandidate, transport) {
  EXPECT_TRUE(transport->AddRemoteCandidate(arg0));
}

TEST_F(P2PTransportImplTest, ConnectUdp) {
  Init(P2PTransport::PROTOCOL_UDP);

  EXPECT_CALL(event_handler1_, OnCandidateReady(_)).WillRepeatedly(
      AddRemoteCandidate(transport2_.get()));
  EXPECT_CALL(event_handler2_, OnCandidateReady(_)).WillRepeatedly(
      AddRemoteCandidate(transport1_.get()));

  message_loop_.RunAllPending();
}

TEST_F(P2PTransportImplTest, ConnectTcp) {
  Init(P2PTransport::PROTOCOL_TCP);

  EXPECT_CALL(event_handler1_, OnCandidateReady(_)).WillRepeatedly(
      AddRemoteCandidate(transport2_.get()));
  EXPECT_CALL(event_handler2_, OnCandidateReady(_)).WillRepeatedly(
      AddRemoteCandidate(transport1_.get()));

  message_loop_.RunAllPending();
}

TEST_F(P2PTransportImplTest, SendDataUdp) {
  Init(P2PTransport::PROTOCOL_UDP);

  EXPECT_CALL(event_handler1_, OnCandidateReady(_)).WillRepeatedly(
      AddRemoteCandidate(transport2_.get()));
  EXPECT_CALL(event_handler2_, OnCandidateReady(_)).WillRepeatedly(
      AddRemoteCandidate(transport1_.get()));

  // Transport may first become ether readable or writable, but
  // eventually it must be readable and writable.
  EXPECT_CALL(event_handler1_, OnStateChange(P2PTransport::STATE_READABLE))
      .Times(AtMost(1));
  EXPECT_CALL(event_handler1_, OnStateChange(P2PTransport::STATE_WRITABLE))
      .Times(AtMost(1));
  EXPECT_CALL(event_handler1_, OnStateChange(
      static_cast<P2PTransport::State>(P2PTransport::STATE_READABLE |
                                       P2PTransport::STATE_WRITABLE)))
      .Times(Exactly(1));

  EXPECT_CALL(event_handler2_, OnStateChange(P2PTransport::STATE_READABLE))
      .Times(AtMost(1));
  EXPECT_CALL(event_handler2_, OnStateChange(P2PTransport::STATE_WRITABLE))
      .Times(AtMost(1));
  EXPECT_CALL(event_handler2_, OnStateChange(
      static_cast<P2PTransport::State>(P2PTransport::STATE_READABLE |
                                       P2PTransport::STATE_WRITABLE)))
      .Times(Exactly(1));

  scoped_refptr<UdpChannelTester> channel_tester = new UdpChannelTester(
      &message_loop_, transport1_->GetChannel(), transport2_->GetChannel());

  message_loop_.PostDelayedTask(FROM_HERE, new MessageLoop::QuitTask(),
                                TestTimeouts::action_max_timeout_ms());

  channel_tester->Start();
  message_loop_.Run();
  channel_tester->CheckResults();
}

TEST_F(P2PTransportImplTest, SendDataTcp) {
  Init(P2PTransport::PROTOCOL_TCP);

  EXPECT_CALL(event_handler1_, OnCandidateReady(_)).WillRepeatedly(
      AddRemoteCandidate(transport2_.get()));
  EXPECT_CALL(event_handler2_, OnCandidateReady(_)).WillRepeatedly(
      AddRemoteCandidate(transport1_.get()));

  scoped_refptr<TcpChannelTester> channel_tester = new TcpChannelTester(
      &message_loop_, transport1_->GetChannel(), transport2_->GetChannel());

  // Transport may first become ether readable or writable, but
  // eventually it must be readable and writable.
  EXPECT_CALL(event_handler1_, OnStateChange(P2PTransport::STATE_READABLE))
      .Times(AtMost(1));
  EXPECT_CALL(event_handler1_, OnStateChange(P2PTransport::STATE_WRITABLE))
      .Times(AtMost(1));
  EXPECT_CALL(event_handler1_, OnStateChange(
      static_cast<P2PTransport::State>(P2PTransport::STATE_READABLE |
                                       P2PTransport::STATE_WRITABLE)))
      .Times(Exactly(1))
      .WillOnce(InvokeWithoutArgs(channel_tester.get(),
                                  &TcpChannelTester::StartWrite))
      .RetiresOnSaturation();

  EXPECT_CALL(event_handler2_, OnStateChange(P2PTransport::STATE_READABLE))
      .Times(AtMost(1));
  EXPECT_CALL(event_handler2_, OnStateChange(P2PTransport::STATE_WRITABLE))
      .Times(AtMost(1));
  EXPECT_CALL(event_handler2_, OnStateChange(
      static_cast<P2PTransport::State>(P2PTransport::STATE_READABLE |
                                       P2PTransport::STATE_WRITABLE)))
      .Times(Exactly(1))
      .WillOnce(InvokeWithoutArgs(channel_tester.get(),
                                  &TcpChannelTester::StartRead))
      .RetiresOnSaturation();

  message_loop_.PostDelayedTask(FROM_HERE, new MessageLoop::QuitTask(),
                                TestTimeouts::action_max_timeout_ms());

  channel_tester->Init();
  message_loop_.Run();
  channel_tester->CheckResults();
}

}  // namespace content
