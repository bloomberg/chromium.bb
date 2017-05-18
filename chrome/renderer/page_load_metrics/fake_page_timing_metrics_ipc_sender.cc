// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/page_load_metrics/page_load_metrics_messages.h"
#include "chrome/renderer/page_load_metrics/fake_page_timing_metrics_ipc_sender.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace page_load_metrics {

FakePageTimingMetricsIPCSender::FakePageTimingMetricsIPCSender() {}

FakePageTimingMetricsIPCSender::~FakePageTimingMetricsIPCSender() {
  VerifyExpectedTimings();
}

void FakePageTimingMetricsIPCSender::ExpectPageLoadTiming(
    const mojom::PageLoadTiming& timing) {
  VerifyExpectedTimings();
  expected_timings_.push_back(timing.Clone());
}

void FakePageTimingMetricsIPCSender::VerifyExpectedTimings() const {
  // Ideally we'd just call ASSERT_EQ(actual_timings_, expected_timings_) here,
  // but this causes the generated gtest code to fail to build on Windows. See
  // the comments in the header file for additional details.
  ASSERT_EQ(actual_timings_.size(), expected_timings_.size());
  for (size_t i = 0; i < actual_timings_.size(); ++i) {
    if (actual_timings_.at(i)->Equals(*expected_timings_.at(i)))
      continue;
    ADD_FAILURE() << "Actual timing != expected timing at index " << i;
  }
}

bool FakePageTimingMetricsIPCSender::Send(IPC::Message* message) {
  IPC_BEGIN_MESSAGE_MAP(FakePageTimingMetricsIPCSender, *message)
    IPC_MESSAGE_HANDLER(PageLoadMetricsMsg_TimingUpdated, OnTimingUpdated)
    IPC_MESSAGE_UNHANDLED(ADD_FAILURE())
  IPC_END_MESSAGE_MAP()

  delete message;
  return true;
}

void FakePageTimingMetricsIPCSender::OnTimingUpdated(
    const mojom::PageLoadTiming& timing,
    mojom::PageLoadMetadata metadata) {
  actual_timings_.push_back(timing.Clone());
  VerifyExpectedTimings();
}

}  // namespace page_load_metrics
