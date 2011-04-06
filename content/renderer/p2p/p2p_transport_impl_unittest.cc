// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/test/test_timeouts.h"
#include "content/renderer/p2p/p2p_transport_impl.h"
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
using testing::Invoke;

using webkit_glue::P2PTransport;

namespace {
const char kTestAddress1[] = "192.168.15.12";
const char kTestAddress2[] = "192.168.15.33";

const char kTransportName1[] = "tr1";
const char kTransportName2[] = "tr2";

const char kTestConfig[] = "";

// Send 10 packets 10 bytes each. Packets are sent with 10ms delay
// between packets (about 100 ms for 10 messages).
const int kMessageSize = 10;
const int kMessages = 10;
const int kUdpWriteDelayMs = 10;

class ChannelTester : public base::RefCountedThreadSafe<ChannelTester> {
 public:
  ChannelTester(MessageLoop* message_loop,
                net::Socket* write_socket,
                net::Socket* read_socket)
      : message_loop_(message_loop),
        write_socket_(write_socket),
        read_socket_(read_socket),
        done_(false),
        ALLOW_THIS_IN_INITIALIZER_LIST(
            write_cb_(this, &ChannelTester::OnWritten)),
        ALLOW_THIS_IN_INITIALIZER_LIST(
            read_cb_(this, &ChannelTester::OnRead)),
        write_errors_(0),
        read_errors_(0),
        packets_sent_(0),
        packets_received_(0),
        broken_packets_(0) {
  }

  virtual ~ChannelTester() { }

  void Start() {
    message_loop_->PostTask(
        FROM_HERE, NewRunnableMethod(this, &ChannelTester::DoStart));
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
          FROM_HERE, NewRunnableMethod(this, &ChannelTester::DoWrite),
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

  net::CompletionCallbackImpl<ChannelTester> write_cb_;
  net::CompletionCallbackImpl<ChannelTester> read_cb_;
  int write_errors_;
  int read_errors_;
  int packets_sent_;
  int packets_received_;
  int broken_packets_;
};

}  // namespace

class MockP2PEventHandler : public P2PTransport::EventHandler {
 public:
  MOCK_METHOD1(OnCandidateReady, void(const std::string& address));
  MOCK_METHOD1(OnStateChange, void(P2PTransport::State state));
};

class P2PTransportImplTest : public testing::Test {
 public:

 protected:
  void SetUp() OVERRIDE {
    socket_manager_ = new jingle_glue::FakeSocketManager();

    net::IPAddressNumber ip;
    ASSERT(net::ParseIPLiteralToNumber(kTestAddress1, &ip));
    network_manager1_.reset(new jingle_glue::FakeNetworkManager(ip));
    socket_factory1_.reset(
        new jingle_glue::FakeSocketFactory(socket_manager_, ip));
    transport1_.reset(new P2PTransportImpl(network_manager1_.get(),
                                           socket_factory1_.get()));

    ASSERT(net::ParseIPLiteralToNumber(kTestAddress2, &ip));
    network_manager2_.reset(new jingle_glue::FakeNetworkManager(ip));
    socket_factory2_.reset(
        new jingle_glue::FakeSocketFactory(socket_manager_, ip));
    transport2_.reset(new P2PTransportImpl(network_manager2_.get(),
                                           socket_factory2_.get()));
  }

  MessageLoop message_loop_;

  scoped_ptr<jingle_glue::FakeNetworkManager> network_manager1_;
  scoped_ptr<jingle_glue::FakeNetworkManager> network_manager2_;
  scoped_refptr<jingle_glue::FakeSocketManager> socket_manager_;
  scoped_ptr<jingle_glue::FakeSocketFactory> socket_factory1_;
  scoped_ptr<jingle_glue::FakeSocketFactory> socket_factory2_;

  scoped_ptr<P2PTransportImpl> transport1_;
  MockP2PEventHandler event_handler1_;
  scoped_ptr<P2PTransportImpl> transport2_;
  MockP2PEventHandler event_handler2_;
};

TEST_F(P2PTransportImplTest, DISABLED_Create) {
  ASSERT_TRUE(transport1_->Init(
      kTransportName1, kTestConfig, &event_handler1_));
  ASSERT_TRUE(transport2_->Init(
      kTransportName2, kTestConfig, &event_handler2_));

  EXPECT_CALL(event_handler1_, OnCandidateReady(_));
  EXPECT_CALL(event_handler2_, OnCandidateReady(_));

  message_loop_.RunAllPending();
}

ACTION_P(AddRemoteCandidate, transport) {
  EXPECT_TRUE(transport->AddRemoteCandidate(arg0));
}

TEST_F(P2PTransportImplTest, DISABLED_Connect) {
  ASSERT_TRUE(transport1_->Init(
      kTransportName1, kTestConfig, &event_handler1_));
  ASSERT_TRUE(transport2_->Init(
      kTransportName2, kTestConfig, &event_handler2_));

  EXPECT_CALL(event_handler1_, OnCandidateReady(_)).WillRepeatedly(
      AddRemoteCandidate(transport2_.get()));
  EXPECT_CALL(event_handler2_, OnCandidateReady(_)).WillRepeatedly(
      AddRemoteCandidate(transport1_.get()));

  message_loop_.RunAllPending();
}

TEST_F(P2PTransportImplTest, DISABLED_SendData) {
  ASSERT_TRUE(transport1_->Init(
      kTransportName1, kTestConfig, &event_handler1_));
  ASSERT_TRUE(transport2_->Init(
      kTransportName2, kTestConfig, &event_handler2_));

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

  scoped_refptr<ChannelTester> channel_tester = new ChannelTester(
      &message_loop_, transport1_->GetChannel(), transport2_->GetChannel());

  message_loop_.PostDelayedTask(FROM_HERE, new MessageLoop::QuitTask(),
                                TestTimeouts::action_max_timeout_ms());

  channel_tester->Start();
  message_loop_.Run();
  channel_tester->CheckResults();
}
