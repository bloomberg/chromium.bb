// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_connection_helper.h"

#include <vector>

#include "net/base/net_errors.h"
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

class TestConnection : public QuicConnection {
 public:
  TestConnection(QuicGuid guid,
                 IPEndPoint address,
                 QuicConnectionHelper* helper)
      : QuicConnection(guid, address, helper) {
  }

  void SendAck() {
    QuicConnectionPeer::SendAck(this);
  }

  void SetScheduler(QuicSendScheduler* scheduler) {
    QuicConnectionPeer::SetScheduler(this, scheduler);
  }
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
        framer_(QuicDecrypter::Create(kNULL), QuicEncrypter::Create(kNULL)),
        creator_(guid_, &framer_),
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

    MockUDPClientSocket* socket =
        new MockUDPClientSocket(socket_data_.get(), net_log_.net_log());
    socket->Connect(IPEndPoint());
    runner_ = new TestTaskRunner(&clock_);
    helper_.reset(new QuicConnectionHelper(runner_.get(), &clock_,
                                           &random_generator_, socket));
    scheduler_ = new testing::StrictMock<MockScheduler>();
    EXPECT_CALL(*scheduler_, TimeUntilSend(_)).
        WillRepeatedly(testing::Return(QuicTime::Delta()));
    connection_.reset(new TestConnection(guid_, IPEndPoint(), helper_.get()));
    connection_->set_visitor(&visitor_);
    connection_->SetScheduler(scheduler_);
  }

  // Returns a newly created packet to send kData on stream 1.
  QuicEncryptedPacket* ConstructDataPacket(
      QuicPacketSequenceNumber sequence_number) {
    InitializeHeader(sequence_number);

    return ConstructPacket(header_, QuicFrame(&frame_));
  }

  // Returns a newly created packet to send ack data.
  QuicEncryptedPacket* ConstructAckPacket(
      QuicPacketSequenceNumber sequence_number) {
    InitializeHeader(sequence_number);

    QuicAckFrame ack(0, sequence_number);

    QuicCongestionFeedbackFrame feedback;
    feedback.type = kTCP;
    feedback.tcp.accumulated_number_of_lost_packets = 0;
    feedback.tcp.receive_window = 16000;

    QuicFrames frames;
    frames.push_back(QuicFrame(&ack));
    frames.push_back(QuicFrame(&feedback));
    scoped_ptr<QuicPacket> packet(
        framer_.ConstructFrameDataPacket(header_, frames));
    return framer_.EncryptPacket(*packet);
  }

  // Returns a newly created packet to send a connection close frame.
  QuicEncryptedPacket* ConstructClosePacket(
      QuicPacketSequenceNumber sequence_number,
      QuicPacketSequenceNumber least_waiting) {
    InitializeHeader(sequence_number);

    QuicFrames frames;
    QuicAckFrame ack(0, least_waiting);
    QuicConnectionCloseFrame close;
    close.error_code = QUIC_CONNECTION_TIMED_OUT;
    close.ack_frame = ack;

    return ConstructPacket(header_, QuicFrame(&close));
  }

  testing::StrictMock<MockScheduler>* scheduler_;
  scoped_refptr<TestTaskRunner> runner_;
  scoped_ptr<QuicConnectionHelper> helper_;
  scoped_array<MockWrite> mock_writes_;
  MockClock clock_;
  MockRandom random_generator_;
  scoped_ptr<TestConnection> connection_;
  testing::StrictMock<MockConnectionVisitor> visitor_;

 private:
  void InitializeHeader(QuicPacketSequenceNumber sequence_number) {
    header_.guid = guid_;
    header_.packet_sequence_number = sequence_number;
    header_.flags = PACKET_FLAGS_NONE;
    header_.fec_group = 0;
  }

  QuicEncryptedPacket* ConstructPacket(const QuicPacketHeader& header,
                                       const QuicFrame& frame) {
    QuicFrames frames;
    frames.push_back(frame);
    scoped_ptr<QuicPacket> packet(
        framer_.ConstructFrameDataPacket(header_, frames));
    return framer_.EncryptPacket(*packet);
  }

  QuicGuid guid_;
  QuicFramer framer_;
  QuicPacketCreator creator_;
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

TEST_F(QuicConnectionHelperTest, IsSendAlarmSet) {
  EXPECT_FALSE(helper_->IsSendAlarmSet());
}

TEST_F(QuicConnectionHelperTest, SetSendAlarm) {
  helper_->SetSendAlarm(QuicTime::Delta());
  EXPECT_TRUE(helper_->IsSendAlarmSet());
}

TEST_F(QuicConnectionHelperTest, UnregisterSendAlarmIfRegistered) {
  helper_->SetSendAlarm(QuicTime::Delta());
  helper_->UnregisterSendAlarmIfRegistered() ;
  EXPECT_FALSE(helper_->IsSendAlarmSet());
}

TEST_F(QuicConnectionHelperTest, SetAckAlarm) {
  AddWrite(SYNCHRONOUS, ConstructAckPacket(1));
  Initialize();

  // The timeout alarm task is always posted.
  ASSERT_EQ(1u, runner_->GetPostedTasks().size());

  QuicTime::Delta delta(QuicTime::Delta::FromMilliseconds(10));
  helper_->SetAckAlarm(delta);

  // Verify that the ack alarm task has been posted.
  ASSERT_EQ(2u, runner_->GetPostedTasks().size());
  EXPECT_EQ(base::TimeDelta::FromMicroseconds(delta.ToMicroseconds()),
            runner_->GetPostedTasks()[1].delay);

  EXPECT_CALL(*scheduler_, SentPacket(1, _, false));
  runner_->RunNextTask();
  EXPECT_EQ(QuicTime().Add(delta), clock_.Now());
}

TEST_F(QuicConnectionHelperTest, ClearAckAlarm) {
  Initialize();

  // Verify that the timeout alarm task has been posted.
  ASSERT_EQ(1u, runner_->GetPostedTasks().size());

  QuicTime::Delta delta(QuicTime::Delta::FromMilliseconds(10));
  helper_->SetAckAlarm(delta);

  helper_->ClearAckAlarm();

  // When the AckAlarm actually fires, no ack will be sent.
  runner_->RunNextTask();
  EXPECT_EQ(QuicTime().Add(delta), clock_.Now());
}

TEST_F(QuicConnectionHelperTest, ResetAckAlarm) {
  AddWrite(SYNCHRONOUS, ConstructAckPacket(1));
  Initialize();

  // Verify that the timeout alarm task has been posted.
  ASSERT_EQ(1u, runner_->GetPostedTasks().size());

  QuicTime::Delta delta1(QuicTime::Delta::FromMilliseconds(10));
  QuicTime::Delta delta2(QuicTime::Delta::FromMilliseconds(20));
  helper_->SetAckAlarm(delta1);
  helper_->ClearAckAlarm();
  helper_->SetAckAlarm(delta2);
  // We should only have 1 ack alarm task posted.
  ASSERT_EQ(2u, runner_->GetPostedTasks().size());

  // The task will execute at delta1, but will not send and ack,
  // but it will reschedule itself for delta2
  runner_->RunNextTask();
  EXPECT_EQ(QuicTime().Add(delta1), clock_.Now());

  // Verify that the ack alarm task has been re-posted.
  ASSERT_EQ(2u, runner_->GetPostedTasks().size());

  EXPECT_CALL(*scheduler_, SentPacket(1, _, false));
  runner_->RunNextTask();
  EXPECT_EQ(QuicTime().Add(delta2), clock_.Now());
}

TEST_F(QuicConnectionHelperTest, TestResend) {
  AddWrite(SYNCHRONOUS, ConstructDataPacket(1));
  AddWrite(SYNCHRONOUS, ConstructDataPacket(2));
  Initialize();

  QuicTime::Delta kDefaultResendTime = QuicTime::Delta::FromMilliseconds(500);
  QuicTime start = clock_.Now();

  EXPECT_CALL(*scheduler_, SentPacket(1, _, false));
  // Send a packet.
  connection_->SendStreamData(1, kData, 0, false, NULL);
  EXPECT_CALL(*scheduler_, SentPacket(2, _, true));
  // Since no ack was received, the resend alarm will fire and resend it.
  runner_->RunNextTask();

  EXPECT_EQ(kDefaultResendTime, clock_.Now().Subtract(start));
  EXPECT_TRUE(AtEof());
}

TEST_F(QuicConnectionHelperTest, InitialTimeout) {
  AddWrite(SYNCHRONOUS, ConstructClosePacket(1, 0));
  Initialize();

  // Verify that a single task was posted.
  ASSERT_EQ(1u, runner_->GetPostedTasks().size());
  EXPECT_EQ(base::TimeDelta::FromMicroseconds(kDefaultTimeoutUs),
            runner_->GetPostedTasks().front().delay);

  EXPECT_CALL(*scheduler_, SentPacket(1, _, false));
  // After we run the next task, we should close the connection.
  EXPECT_CALL(visitor_, ConnectionClose(QUIC_CONNECTION_TIMED_OUT, false));

  runner_->RunNextTask();
  EXPECT_EQ(QuicTime::FromMicroseconds(kDefaultTimeoutUs), clock_.Now());
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
  MessageLoop::current()->RunUntilIdle();
  EXPECT_TRUE(AtEof());
}

TEST_F(QuicConnectionHelperTest, TimeoutAfterSend) {
  AddWrite(SYNCHRONOUS, ConstructAckPacket(1));
  AddWrite(SYNCHRONOUS, ConstructClosePacket(2, 1));
  Initialize();

  EXPECT_TRUE(connection_->connected());
  EXPECT_EQ(0u, clock_.Now().ToMicroseconds());

  // When we send a packet, the timeout will change to 5000 + kDefaultTimeout.
  clock_.AdvanceTime(QuicTime::Delta::FromMicroseconds(5000));
  EXPECT_EQ(5000u, clock_.Now().ToMicroseconds());
  EXPECT_CALL(*scheduler_, SentPacket(1, _, false));

  // Send an ack so we don't set the resend alarm.
  connection_->SendAck();

  // The original alarm will fire.  We should not time out because we had a
  // network event at t=5000.  The alarm will reregister.
  runner_->RunNextTask();

  EXPECT_EQ(QuicTime::FromMicroseconds(kDefaultTimeoutUs), clock_.Now());
  EXPECT_TRUE(connection_->connected());

  // This time, we should time out.
  EXPECT_CALL(visitor_, ConnectionClose(QUIC_CONNECTION_TIMED_OUT, false));
  EXPECT_CALL(*scheduler_, SentPacket(2, _, false));
  runner_->RunNextTask();
  EXPECT_EQ(kDefaultTimeoutUs + 5000, clock_.Now().ToMicroseconds());
  EXPECT_FALSE(connection_->connected());
  EXPECT_TRUE(AtEof());
}

TEST_F(QuicConnectionHelperTest, SendSchedulerDelayThenSend) {
  AddWrite(SYNCHRONOUS, ConstructDataPacket(1));
  Initialize();

  // Test that if we send a packet with a delay, it ends up queued.
  EXPECT_CALL(*scheduler_, TimeUntilSend(false)).WillOnce(testing::Return(
      QuicTime::Delta::FromMicroseconds(1)));

  connection_->SendStreamData(1, kData, 0, false, NULL);
  EXPECT_CALL(*scheduler_, SentPacket(1, _, false));
  EXPECT_EQ(1u, connection_->NumQueuedPackets());

  // Advance the clock to fire the alarm, and configure the scheduler
  // to permit the packet to be sent.
  EXPECT_CALL(*scheduler_, TimeUntilSend(false)).WillOnce(testing::Return(
      QuicTime::Delta()));
  EXPECT_CALL(visitor_, OnCanWrite()).WillOnce(testing::Return(true));
  runner_->RunNextTask();
  EXPECT_EQ(0u, connection_->NumQueuedPackets());
  EXPECT_TRUE(AtEof());
}

}  // namespace test
}  // namespace net
