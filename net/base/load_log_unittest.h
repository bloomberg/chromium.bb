// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_LOAD_LOG_UNITTEST_H_
#define NET_BASE_LOAD_LOG_UNITTEST_H_

#include <cstddef>
#include "net/base/load_log.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

// Create a timestamp with internal value of |t| milliseconds from the epoch.
inline base::TimeTicks MakeTime(int t) {
  base::TimeTicks ticks;  // initialized to 0.
  ticks += base::TimeDelta::FromMilliseconds(t);
  return ticks;
}

inline ::testing::AssertionResult LogContainsEventHelper(
    const LoadLog& log,
    int i,  // Negative indices are reverse indices.
    const base::TimeTicks& expected_time,
    bool check_time,
    LoadLog::EventType expected_event,
    LoadLog::EventPhase expected_phase) {
  // Negative indices are reverse indices.
  size_t j = (i < 0) ? log.entries().size() + i : i;
  if (j >= log.entries().size())
    return ::testing::AssertionFailure() << j << " is out of bounds.";
  const LoadLog::Entry& entry = log.entries()[j];
  if (entry.type != LoadLog::Entry::TYPE_EVENT) {
    return ::testing::AssertionFailure() << "Not a TYPE_EVENT entry";
  }
  if (expected_event != entry.event.type) {
    return ::testing::AssertionFailure()
        << "Actual event: " << LoadLog::EventTypeToString(entry.event.type)
        << ". Expected event: " << LoadLog::EventTypeToString(expected_event)
        << ".";
  }
  if (expected_phase != entry.event.phase) {
    return ::testing::AssertionFailure()
        << "Actual phase: " << entry.event.phase
        << ". Expected phase: " << expected_phase << ".";
  }
  if (check_time) {
    if (expected_time != entry.time) {
      return ::testing::AssertionFailure()
          << "Actual time: " << entry.time.ToInternalValue()
          << ". Expected time: " << expected_time.ToInternalValue()
          << ".";
    }
  }
  return ::testing::AssertionSuccess();
}

inline ::testing::AssertionResult LogContainsEventAtTime(
    const LoadLog& log,
    int i,  // Negative indices are reverse indices.
    const base::TimeTicks& expected_time,
    LoadLog::EventType expected_event,
    LoadLog::EventPhase expected_phase) {
  return LogContainsEventHelper(log, i, expected_time, true,
                                expected_event, expected_phase);
}

// Version without timestamp.
inline ::testing::AssertionResult LogContainsEvent(
    const LoadLog& log,
    int i,  // Negative indices are reverse indices.
    LoadLog::EventType expected_event,
    LoadLog::EventPhase expected_phase) {
  return LogContainsEventHelper(log, i, base::TimeTicks(), false,
                                expected_event, expected_phase);
}

// Version for PHASE_BEGIN (and no timestamp).
inline ::testing::AssertionResult LogContainsBeginEvent(
    const LoadLog& log,
    int i,  // Negative indices are reverse indices.
    LoadLog::EventType expected_event) {
  return LogContainsEvent(log, i, expected_event, LoadLog::PHASE_BEGIN);
}

// Version for PHASE_END (and no timestamp).
inline ::testing::AssertionResult LogContainsEndEvent(
    const LoadLog& log,
    int i,  // Negative indices are reverse indices.
    LoadLog::EventType expected_event) {
  return LogContainsEvent(log, i, expected_event, LoadLog::PHASE_END);
}

// Expect that the log contains an event, but don't care about where
// as long as the index where it is found is greater than min_index.
// Returns the position where the event was found.
inline size_t ExpectLogContainsSomewhere(const LoadLog* log,
                                         size_t min_index,
                                         LoadLog::EventType expected_event,
                                         LoadLog::EventPhase expected_phase) {
  size_t i = 0;
  for (; i < log->entries().size(); ++i) {
    const LoadLog::Entry& entry = log->entries()[i];
    if (entry.type == LoadLog::Entry::TYPE_EVENT &&
        entry.event.type == expected_event &&
        entry.event.phase == expected_phase)
      break;
  }
  EXPECT_LT(i, log->entries().size());
  EXPECT_GE(i, min_index);
  return i;
}

}  // namespace net

#endif  // NET_BASE_LOAD_LOG_UNITTEST_H_
