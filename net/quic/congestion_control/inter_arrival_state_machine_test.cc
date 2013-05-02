// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "net/quic/congestion_control/inter_arrival_state_machine.h"
#include "net/quic/test_tools/mock_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test {

class InterArrivalStateMachineTest : public ::testing::Test {
 protected:
  InterArrivalStateMachineTest() {
  }

  virtual void SetUp() {
    state_machine_.reset(new InterArrivalStateMachine(&clock_));
  }

  MockClock clock_;
  scoped_ptr<InterArrivalStateMachine> state_machine_;
};

TEST_F(InterArrivalStateMachineTest, SimplePacketLoss) {
  QuicTime::Delta rtt = QuicTime::Delta::FromMilliseconds(100);
  state_machine_->set_rtt(rtt);
  state_machine_->IncreaseBitrateDecision();

  clock_.AdvanceTime(rtt);
  state_machine_->PacketLossEvent();
  state_machine_->DecreaseBitrateDecision();
  EXPECT_EQ(kInterArrivalStateStable,
            state_machine_->GetInterArrivalState());

  // Make sure we switch to state packet loss.
  clock_.AdvanceTime(rtt);
  state_machine_->PacketLossEvent();
  state_machine_->DecreaseBitrateDecision();
  EXPECT_EQ(kInterArrivalStatePacketLoss,
            state_machine_->GetInterArrivalState());

  // Make sure we stay in state packet loss.
  clock_.AdvanceTime(rtt);
  state_machine_->PacketLossEvent();
  state_machine_->DecreaseBitrateDecision();
  EXPECT_EQ(kInterArrivalStatePacketLoss,
            state_machine_->GetInterArrivalState());

  clock_.AdvanceTime(rtt);
  state_machine_->PacketLossEvent();
  state_machine_->DecreaseBitrateDecision();
  EXPECT_EQ(kInterArrivalStatePacketLoss,
            state_machine_->GetInterArrivalState());
}

TEST_F(InterArrivalStateMachineTest, SimpleDelay) {
  QuicTime::Delta rtt = QuicTime::Delta::FromMilliseconds(100);
  state_machine_->set_rtt(rtt);
  state_machine_->IncreaseBitrateDecision();

  clock_.AdvanceTime(rtt);
  state_machine_->IncreasingDelayEvent();
  state_machine_->DecreaseBitrateDecision();
  EXPECT_EQ(kInterArrivalStateStable,
            state_machine_->GetInterArrivalState());

  // Make sure we switch to state delay.
  clock_.AdvanceTime(rtt);
  state_machine_->IncreasingDelayEvent();
  state_machine_->DecreaseBitrateDecision();
  EXPECT_EQ(kInterArrivalStateDelay,
            state_machine_->GetInterArrivalState());

  clock_.AdvanceTime(rtt);
  state_machine_->IncreasingDelayEvent();
  state_machine_->DecreaseBitrateDecision();
  EXPECT_EQ(kInterArrivalStateDelay,
            state_machine_->GetInterArrivalState());

  // Make sure we switch to state competing flow(s).
  clock_.AdvanceTime(rtt);
  state_machine_->IncreasingDelayEvent();
  state_machine_->DecreaseBitrateDecision();
  EXPECT_EQ(kInterArrivalStateCompetingFlow,
            state_machine_->GetInterArrivalState());

  // Make sure we stay in state competing flow(s).
  clock_.AdvanceTime(rtt);
  state_machine_->IncreasingDelayEvent();
  state_machine_->DecreaseBitrateDecision();
  EXPECT_EQ(kInterArrivalStateCompetingFlow,
            state_machine_->GetInterArrivalState());

  clock_.AdvanceTime(rtt);
  state_machine_->PacketLossEvent();
  state_machine_->DecreaseBitrateDecision();
  EXPECT_EQ(kInterArrivalStateCompetingFlow,
            state_machine_->GetInterArrivalState());

  clock_.AdvanceTime(rtt);
  state_machine_->PacketLossEvent();
  state_machine_->DecreaseBitrateDecision();
  EXPECT_EQ(kInterArrivalStateCompetingFlow,
            state_machine_->GetInterArrivalState());

  // Make sure we switch to state competing TCP flow(s).
  clock_.AdvanceTime(rtt);
  state_machine_->PacketLossEvent();
  state_machine_->DecreaseBitrateDecision();
  EXPECT_EQ(kInterArrivalStateCompetingTcpFLow,
            state_machine_->GetInterArrivalState());

  // Make sure we stay in state competing TCP flow(s).
  clock_.AdvanceTime(rtt);
  state_machine_->PacketLossEvent();
  state_machine_->DecreaseBitrateDecision();
  EXPECT_EQ(kInterArrivalStateCompetingTcpFLow,
            state_machine_->GetInterArrivalState());
}

}  // namespace test
}  // namespace net
