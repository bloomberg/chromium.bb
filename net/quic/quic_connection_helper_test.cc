// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_connection_helper.h"

#include <vector>

#include "net/base/net_errors.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/test_tools/mock_clock.h"
#include "net/quic/test_tools/quic_connection_peer.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/quic/test_tools/test_task_runner.h"
#include "net/socket/socket_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace net {
namespace test {

const char kData[] = "foo";
const bool kFromPeer = true;

class TestDelegate : public QuicAlarm::Delegate {
 public:
  TestDelegate() : fired_(false) {}

  virtual QuicTime OnAlarm() OVERRIDE {
    fired_ = true;
    return QuicTime::Zero();
  }

  bool fired() const { return fired_; }

 private:
  bool fired_;
};

class TestConnection : public QuicConnection {
 public:
  TestConnection(QuicGuid guid,
                 IPEndPoint address,
                 QuicConnectionHelper* helper)
      : QuicConnection(guid, address, helper, false, QuicVersionMax()) {
  }

  void SendAck() {
    QuicConnectionPeer::SendAck(this);
  }

  void SetSendAlgorithm(SendAlgorithmInterface* send_algorithm) {
    QuicConnectionPeer::SetSendAlgorithm(this, send_algorithm);
  }

  using QuicConnection::SendOrQueuePacket;
};

class QuicConnectionHelperTest : public ::testing::Test {
 protected:
  // Holds a packet to be written to the wire, and the IO mode that should
  // be used by the mock socket when performing the write.
  struct PacketToWrite {
    PacketToWrite(IoMode mode, QuicEncryptedPacket* packet)
        : mode(mode),
          packet(packet) {
    }
    IoMode mode;
    QuicEncryptedPacket* packet;
  };

  QuicConnectionHelperTest()
      : guid_(2),
        framer_(QuicVersionMax(), QuicTime::Zero(), false),
        net_log_(BoundNetLog()),
        frame_(1, false, 0, kData) {
    Initialize();
  }

  ~QuicConnectionHelperTest() {
    for (size_t i = 0; i < writes_.size(); i++) {
      delete writes_[i].packet;
    }
  }

  // Adds a packet to the list of expected writes.
  void AddWrite(IoMode mode, QuicEncryptedPacket* packet) {
    writes_.push_back(PacketToWrite(mode, packet));
  }

  // Returns the packet to be written at position |pos|.
  QuicEncryptedPacket* GetWrite(size_t pos) {
    return writes_[pos].packet;
  }

  bool AtEof() {
    return socket_data_->at_read_eof() && socket_data_->at_write_eof();
  }

  // Configures the test fixture to use the list of expected writes.
  void Initialize() {
    mock_writes_.reset(new MockWrite[writes_.size()]);
    for (size_t i = 0; i < writes_.size(); i++) {
      mock_writes_[i] = MockWrite(writes_[i].mode,
                                  writes_[i].packet->data(),
                                  writes_[i].packet->length());
    };

    socket_data_.reset(new StaticSocketDataProvider(NULL, 0, mock_writes_.get(),
                                                    writes_.size()));

    socket_.reset(new MockUDPClientSocket(socket_data_.get(),
                                          net_log_.net_log()));
    socket_->Connect(IPEndPoint());
    runner_ = new TestTaskRunner(&clock_);
    helper_ = new QuicConnectionHelper(runner_.get(), &clock_,
                                       &random_generator_, socket_.get());
    send_algorithm_ = new testing::StrictMock<MockSendAlgorithm>();
    EXPECT_CALL(*send_algorithm_, TimeUntilSend(_, _, _, _)).
        WillRepeatedly(testing::Return(QuicTime::Delta::Zero()));
    connection_.reset(new TestConnection(guid_, IPEndPoint(), helper_));
    connection_->set_visitor(&visitor_);
    connection_->SetSendAlgorithm(send_algorithm_);
  }

  // Returns a newly created packet to send kData on stream 1.
  QuicEncryptedPacket* ConstructDataPacket(
      QuicPacketSequenceNumber sequence_number) {
    InitializeHeader(sequence_number);

    return ConstructPacket(header_, QuicFrame(&frame_));
  }

  // Returns a newly created packet to send kData on stream 1.
  QuicPacket* ConstructRawDataPacket(
      QuicPacketSequenceNumber sequence_number) {
    InitializeHeader(sequence_number);

    QuicFrames frames;
    frames.push_back(QuicFrame(&frame_));
    return framer_.BuildUnsizedDataPacket(header_, frames).packet;
  }

  // Returns a newly created packet to send ack data.
  QuicEncryptedPacket* ConstructAckPacket(
      QuicPacketSequenceNumber sequence_number) {
    InitializeHeader(sequence_number);

    QuicAckFrame ack(0, QuicTime::Zero(), sequence_number);
    ack.sent_info.entropy_hash = 0;
    ack.received_info.entropy_hash = 0;

    QuicCongestionFeedbackFrame feedback;
    feedback.type = kTCP;
    feedback.tcp.accumulated_number_of_lost_packets = 0;
    feedback.tcp.receive_window = 16000 << 4;

    QuicFrames frames;
    frames.push_back(QuicFrame(&ack));
    frames.push_back(QuicFrame(&feedback));
    scoped_ptr<QuicPacket> packet(
        framer_.BuildUnsizedDataPacket(header_, frames).packet);
    return framer_.EncryptPacket(
        ENCRYPTION_NONE, header_.packet_sequence_number, *packet);
  }

  // Returns a newly created packet to send a connection close frame.
  QuicEncryptedPacket* ConstructClosePacket(
      QuicPacketSequenceNumber sequence_number,
      QuicPacketSequenceNumber least_waiting) {
    InitializeHeader(sequence_number);

    QuicFrames frames;
    QuicAckFrame ack(0, QuicTime::Zero(), least_waiting + 1);
    ack.sent_info.entropy_hash = 0;
    ack.received_info.entropy_hash = 0;
    QuicConnectionCloseFrame close;
    close.error_code = QUIC_CONNECTION_TIMED_OUT;
    close.ack_frame = ack;

    return ConstructPacket(header_, QuicFrame(&close));
  }

  testing::StrictMock<MockSendAlgorithm>* send_algorithm_;
  scoped_refptr<TestTaskRunner> runner_;
  QuicConnectionHelper* helper_;
  scoped_ptr<MockUDPClientSocket> socket_;
  scoped_ptr<MockWrite[]> mock_writes_;
  MockClock clock_;
  MockRandom random_generator_;
  scoped_ptr<TestConnection> connection_;
  testing::StrictMock<MockConnectionVisitor> visitor_;

 private:
  void InitializeHeader(QuicPacketSequenceNumber sequence_number) {
    header_.public_header.guid = guid_;
    header_.public_header.reset_flag = false;
    header_.public_header.version_flag = true;
    header_.packet_sequence_number = sequence_number;
    header_.entropy_flag = false;
    header_.fec_flag = false;
    header_.is_in_fec_group = NOT_IN_FEC_GROUP;
    header_.fec_group = 0;
  }

  QuicEncryptedPacket* ConstructPacket(const QuicPacketHeader& header,
                                       const QuicFrame& frame) {
    QuicFrames frames;
    frames.push_back(frame);
    scoped_ptr<QuicPacket> packet(
        framer_.BuildUnsizedDataPacket(header_, frames).packet);
    return framer_.EncryptPacket(
        ENCRYPTION_NONE, header_.packet_sequence_number, *packet);
  }

  QuicGuid guid_;
  QuicFramer framer_;
  QuicPacketHeader header_;
  BoundNetLog net_log_;
  QuicStreamFrame frame_;
  scoped_ptr<StaticSocketDataProvider> socket_data_;
  std::vector<PacketToWrite> writes_;
};

TEST_F(QuicConnectionHelperTest, GetClock) {
  EXPECT_EQ(&clock_, helper_->GetClock());
}

TEST_F(QuicConnectionHelperTest, GetRandomGenerator) {
  EXPECT_EQ(&random_generator_, helper_->GetRandomGenerator());
}

TEST_F(QuicConnectionHelperTest, CreateAlarm) {
  TestDelegate* delegate = new TestDelegate();
  scoped_ptr<QuicAlarm> alarm(helper_->CreateAlarm(delegate));

  // The timeout alarm task is always posted.
  ASSERT_EQ(1u, runner_->GetPostedTasks().size());

  QuicTime::Delta delta = QuicTime::Delta::FromMicroseconds(1);
  alarm->Set(clock_.Now().Add(delta));

  // Verify that the alarm task has been posted.
  ASSERT_EQ(2u, runner_->GetPostedTasks().size());
  EXPECT_EQ(base::TimeDelta::FromMicroseconds(delta.ToMicroseconds()),
            runner_->GetPostedTasks()[1].delay);

  runner_->RunNextTask();
  EXPECT_EQ(QuicTime::Zero().Add(delta), clock_.Now());
  EXPECT_TRUE(delegate->fired());
}

TEST_F(QuicConnectionHelperTest, CreateAlarmAndCancel) {
  TestDelegate* delegate = new TestDelegate();
  scoped_ptr<QuicAlarm> alarm(helper_->CreateAlarm(delegate));

  QuicTime::Delta delta = QuicTime::Delta::FromMicroseconds(1);
  alarm->Set(clock_.Now().Add(delta));
  alarm->Cancel();

  // The alarm task should still be posted.
  ASSERT_EQ(2u, runner_->GetPostedTasks().size());
  EXPECT_EQ(base::TimeDelta::FromMicroseconds(delta.ToMicroseconds()),
            runner_->GetPostedTasks()[1].delay);

  runner_->RunNextTask();
  EXPECT_EQ(QuicTime::Zero().Add(delta), clock_.Now());
  EXPECT_FALSE(delegate->fired());
}

TEST_F(QuicConnectionHelperTest, CreateAlarmAndReset) {
  TestDelegate* delegate = new TestDelegate();
  scoped_ptr<QuicAlarm> alarm(helper_->CreateAlarm(delegate));

  QuicTime::Delta delta = QuicTime::Delta::FromMicroseconds(1);
  alarm->Set(clock_.Now().Add(delta));
  alarm->Cancel();
  QuicTime::Delta new_delta = QuicTime::Delta::FromMicroseconds(3);
  alarm->Set(clock_.Now().Add(new_delta));

  // The alarm task should still be posted.
  ASSERT_EQ(2u, runner_->GetPostedTasks().size());
  EXPECT_EQ(base::TimeDelta::FromMicroseconds(delta.ToMicroseconds()),
            runner_->GetPostedTasks()[1].delay);

  runner_->RunNextTask();
  EXPECT_EQ(QuicTime::Zero().Add(delta), clock_.Now());
  EXPECT_FALSE(delegate->fired());

  // The alarm task should be posted again.
  ASSERT_EQ(2u, runner_->GetPostedTasks().size());

  runner_->RunNextTask();
  EXPECT_EQ(QuicTime::Zero().Add(new_delta), clock_.Now());
  EXPECT_TRUE(delegate->fired());
}

TEST_F(QuicConnectionHelperTest, TestRetransmission) {
  AddWrite(SYNCHRONOUS, ConstructDataPacket(1));
  AddWrite(SYNCHRONOUS, ConstructDataPacket(2));
  Initialize();

  EXPECT_CALL(*send_algorithm_, RetransmissionDelay()).WillRepeatedly(
      testing::Return(QuicTime::Delta::Zero()));

  QuicTime::Delta kDefaultRetransmissionTime =
      QuicTime::Delta::FromMilliseconds(500);
  QuicTime start = clock_.ApproximateNow();

  EXPECT_CALL(*send_algorithm_, SentPacket(_, 1, _, NOT_RETRANSMISSION));
  EXPECT_CALL(*send_algorithm_, AbandoningPacket(1, _));
  // Send a packet.
  connection_->SendStreamData(1, kData, 0, false);
  EXPECT_CALL(*send_algorithm_, SentPacket(_, 2, _, IS_RETRANSMISSION));
  // Since no ack was received, the retransmission alarm will fire and
  // retransmit it.
  runner_->RunNextTask();

  EXPECT_EQ(kDefaultRetransmissionTime,
            clock_.ApproximateNow().Subtract(start));
  EXPECT_TRUE(AtEof());
}

TEST_F(QuicConnectionHelperTest, TestMultipleRetransmission) {
  AddWrite(SYNCHRONOUS, ConstructDataPacket(1));
  AddWrite(SYNCHRONOUS, ConstructDataPacket(2));
  AddWrite(SYNCHRONOUS, ConstructDataPacket(3));
  Initialize();

  EXPECT_CALL(*send_algorithm_, RetransmissionDelay()).WillRepeatedly(
      testing::Return(QuicTime::Delta::Zero()));

  QuicTime::Delta kDefaultRetransmissionTime =
      QuicTime::Delta::FromMilliseconds(500);
  QuicTime start = clock_.ApproximateNow();

  EXPECT_CALL(*send_algorithm_, SentPacket(_, 1, _, NOT_RETRANSMISSION));
  EXPECT_CALL(*send_algorithm_, AbandoningPacket(1, _));
  // Send a packet.
  connection_->SendStreamData(1, kData, 0, false);
  EXPECT_CALL(*send_algorithm_, SentPacket(_, 2, _, IS_RETRANSMISSION));
  // Since no ack was received, the retransmission alarm will fire and
  // retransmit it.
  runner_->RunNextTask();

  EXPECT_EQ(kDefaultRetransmissionTime,
            clock_.ApproximateNow().Subtract(start));

  // Since no ack was received, the retransmission alarm will fire and
  // retransmit it.
  EXPECT_CALL(*send_algorithm_, SentPacket(_, 3, _, IS_RETRANSMISSION));
  EXPECT_CALL(*send_algorithm_, AbandoningPacket(2, _));
  runner_->RunNextTask();

  EXPECT_EQ(kDefaultRetransmissionTime.Add(kDefaultRetransmissionTime),
            clock_.ApproximateNow().Subtract(start));

  EXPECT_TRUE(AtEof());
}

TEST_F(QuicConnectionHelperTest, InitialTimeout) {
  AddWrite(SYNCHRONOUS, ConstructClosePacket(1, 0));
  Initialize();

  // Verify that a single task was posted.
  ASSERT_EQ(1u, runner_->GetPostedTasks().size());
  EXPECT_EQ(base::TimeDelta::FromSeconds(kDefaultInitialTimeoutSecs),
            runner_->GetPostedTasks().front().delay);

  EXPECT_CALL(*send_algorithm_, SentPacket(_, 1, _, NOT_RETRANSMISSION));
  // After we run the next task, we should close the connection.
  EXPECT_CALL(visitor_, ConnectionClose(QUIC_CONNECTION_TIMED_OUT, false));

  runner_->RunNextTask();
  EXPECT_EQ(QuicTime::Zero().Add(QuicTime::Delta::FromSeconds(
                kDefaultInitialTimeoutSecs)),
            clock_.ApproximateNow());
  EXPECT_FALSE(connection_->connected());
  EXPECT_TRUE(AtEof());
}

TEST_F(QuicConnectionHelperTest, WritePacketToWire) {
  AddWrite(SYNCHRONOUS, ConstructDataPacket(1));
  Initialize();

  int len = GetWrite(0)->length();
  int error = 0;
  EXPECT_EQ(len, helper_->WritePacketToWire(*GetWrite(0), &error));
  EXPECT_EQ(0, error);
  EXPECT_TRUE(AtEof());
}

TEST_F(QuicConnectionHelperTest, WritePacketToWireAsync) {
  AddWrite(ASYNC, ConstructClosePacket(1, 0));
  Initialize();

  EXPECT_CALL(visitor_, OnCanWrite()).WillOnce(testing::Return(true));
  int error = 0;
  EXPECT_EQ(-1, helper_->WritePacketToWire(*GetWrite(0), &error));
  EXPECT_EQ(ERR_IO_PENDING, error);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_TRUE(AtEof());
}

TEST_F(QuicConnectionHelperTest, TimeoutAfterSend) {
  AddWrite(SYNCHRONOUS, ConstructAckPacket(1));
  AddWrite(SYNCHRONOUS, ConstructClosePacket(2, 1));
  Initialize();

  EXPECT_TRUE(connection_->connected());
  QuicTime start = clock_.ApproximateNow();

  // When we send a packet, the timeout will change to 5000 +
  // kDefaultInitialTimeoutSecs.
  clock_.AdvanceTime(QuicTime::Delta::FromMicroseconds(5000));
  EXPECT_EQ(5000u, clock_.ApproximateNow().Subtract(start).ToMicroseconds());
  EXPECT_CALL(*send_algorithm_, SentPacket(_, 1, _, NOT_RETRANSMISSION));

  // Send an ack so we don't set the retransmission alarm.
  connection_->SendAck();

  // The original alarm will fire.  We should not time out because we had a
  // network event at t=5000.  The alarm will reregister.
  runner_->RunNextTask();

  EXPECT_EQ(QuicTime::Zero().Add(QuicTime::Delta::FromSeconds(
                kDefaultInitialTimeoutSecs)),
            clock_.ApproximateNow());
  EXPECT_TRUE(connection_->connected());

  // This time, we should time out.
  EXPECT_CALL(visitor_, ConnectionClose(QUIC_CONNECTION_TIMED_OUT, !kFromPeer));
  EXPECT_CALL(*send_algorithm_, SentPacket(_, 2, _, NOT_RETRANSMISSION));
  runner_->RunNextTask();
  EXPECT_EQ(kDefaultInitialTimeoutSecs * 1000000 + 5000,
            clock_.ApproximateNow().Subtract(
                QuicTime::Zero()).ToMicroseconds());
  EXPECT_FALSE(connection_->connected());
  EXPECT_TRUE(AtEof());
}

TEST_F(QuicConnectionHelperTest, SendSchedulerDelayThenSend) {
  AddWrite(SYNCHRONOUS, ConstructDataPacket(1));
  Initialize();

  // Test that if we send a packet with a delay, it ends up queued.
  EXPECT_CALL(*send_algorithm_, RetransmissionDelay()).WillRepeatedly(
      testing::Return(QuicTime::Delta::Zero()));
  EXPECT_CALL(
      *send_algorithm_, TimeUntilSend(_, NOT_RETRANSMISSION, _, _)).WillOnce(
          testing::Return(QuicTime::Delta::FromMicroseconds(1)));

  QuicPacket* packet = ConstructRawDataPacket(1);
  connection_->SendOrQueuePacket(
      ENCRYPTION_NONE, 1, packet, 0, HAS_RETRANSMITTABLE_DATA);
  EXPECT_CALL(*send_algorithm_, SentPacket(_, 1, _, NOT_RETRANSMISSION));
  EXPECT_EQ(1u, connection_->NumQueuedPackets());

  // Advance the clock to fire the alarm, and configure the scheduler
  // to permit the packet to be sent.
  EXPECT_CALL(*send_algorithm_,
              TimeUntilSend(_, NOT_RETRANSMISSION, _, _)).WillRepeatedly(
      testing::Return(QuicTime::Delta::Zero()));
  EXPECT_CALL(visitor_, OnCanWrite()).WillOnce(testing::Return(true));
  runner_->RunNextTask();
  EXPECT_EQ(0u, connection_->NumQueuedPackets());
  EXPECT_TRUE(AtEof());
}

}  // namespace test
}  // namespace net
