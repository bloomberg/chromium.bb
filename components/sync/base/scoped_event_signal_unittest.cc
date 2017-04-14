// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/scoped_event_signal.h"

#include <memory>
#include <utility>

#include "base/synchronization/waitable_event.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

TEST(ScopedEventSignalTest, SignalAtEndOfScope) {
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);

  {
    ScopedEventSignal scoped_event_signal(&event);
    EXPECT_FALSE(event.IsSignaled());
  }

  EXPECT_TRUE(event.IsSignaled());
}

TEST(ScopedEventSignalTest, MoveConstructor) {
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);

  {
    ScopedEventSignal scoped_event_signal(&event);
    EXPECT_FALSE(event.IsSignaled());

    {
      ScopedEventSignal other_scoped_event_signal(
          std::move(scoped_event_signal));
      EXPECT_FALSE(event.IsSignaled());
    }

    // |event| is signaled when |other_scoped_event_signal| is destroyed.
    EXPECT_TRUE(event.IsSignaled());

    event.Reset();
  }

  // |event| is not signaled when |scoped_signal_event| is destroyed.
  EXPECT_FALSE(event.IsSignaled());
}

TEST(ScopedEventSignalTest, MoveAssignOperator) {
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);

  {
    ScopedEventSignal scoped_event_signal_a(&event);
    EXPECT_FALSE(event.IsSignaled());

    {
      base::WaitableEvent other_event(
          base::WaitableEvent::ResetPolicy::MANUAL,
          base::WaitableEvent::InitialState::NOT_SIGNALED);
      ScopedEventSignal scoped_event_signal_b(&other_event);

      // Move |scoped_event_signal_b| to |scoped_event_signal_c| because the
      // assignment operator cannot be used on a ScopedEventSignal which is
      // already associated with an event.
      ScopedEventSignal scoped_event_signal_c(std::move(scoped_event_signal_b));

      scoped_event_signal_b = std::move(scoped_event_signal_a);
      EXPECT_FALSE(event.IsSignaled());
    }

    // |event| is signaled when |scoped_event_signal_b| is destroyed.
    EXPECT_TRUE(event.IsSignaled());

    event.Reset();
  }

  // |event| is not signaled when |scoped_signal_event_a| is destroyed.
  EXPECT_FALSE(event.IsSignaled());
}

}  // namespace syncer
