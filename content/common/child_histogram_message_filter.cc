// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/child_histogram_message_filter.h"

#include <ctype.h>

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/metrics/statistics_recorder.h"
#include "content/common/child_process.h"
#include "content/common/child_process_messages.h"
#include "content/common/child_thread.h"

namespace content {

ChildHistogramMessageFilter::ChildHistogramMessageFilter()
    : channel_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(histogram_snapshot_manager_(this)) {
}

ChildHistogramMessageFilter::~ChildHistogramMessageFilter() {
}

void ChildHistogramMessageFilter::OnFilterAdded(IPC::Channel* channel) {
  channel_ = channel;
}

void ChildHistogramMessageFilter::OnFilterRemoved() {
}

bool ChildHistogramMessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ChildHistogramMessageFilter, message)
    IPC_MESSAGE_HANDLER(ChildProcessMsg_GetChildHistogramData,
                        OnGetChildHistogramData)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ChildHistogramMessageFilter::SendHistograms(int sequence_number) {
  ChildProcess::current()->io_message_loop_proxy()->PostTask(
      FROM_HERE, base::Bind(&ChildHistogramMessageFilter::UploadAllHistrograms,
                            this, sequence_number));
}

void ChildHistogramMessageFilter::OnGetChildHistogramData(int sequence_number) {
  UploadAllHistrograms(sequence_number);
}

void ChildHistogramMessageFilter::UploadAllHistrograms(int sequence_number) {
  DCHECK_EQ(0u, pickled_histograms_.size());

  base::StatisticsRecorder::CollectHistogramStats("ChildProcess");

  // Push snapshots into our pickled_histograms_ vector.
  histogram_snapshot_manager_.PrepareDeltas(
      base::Histogram::kIPCSerializationSourceFlag, false);

  channel_->Send(new ChildProcessHostMsg_ChildHistogramData(
      sequence_number, pickled_histograms_));

  pickled_histograms_.clear();
  static int count = 0;
  count++;
  DHISTOGRAM_COUNTS("Histogram.ChildProcessHistogramSentCount", count);
}

void ChildHistogramMessageFilter::RecordDelta(
    const base::Histogram& histogram,
    const base::Histogram::SampleSet& snapshot) {
  DCHECK_NE(0, snapshot.TotalCount());
  snapshot.CheckSize(histogram);

  std::string histogram_info =
      base::Histogram::SerializeHistogramInfo(histogram, snapshot);

  pickled_histograms_.push_back(histogram_info);
}

void ChildHistogramMessageFilter::InconsistencyDetected(int problem) {
  UMA_HISTOGRAM_ENUMERATION("Histogram.InconsistenciesChildProcess",
                            problem, base::Histogram::NEVER_EXCEEDED_VALUE);
}

void ChildHistogramMessageFilter::UniqueInconsistencyDetected(int problem) {
  UMA_HISTOGRAM_ENUMERATION("Histogram.InconsistenciesChildProcessUnique",
                            problem, base::Histogram::NEVER_EXCEEDED_VALUE);
}

void ChildHistogramMessageFilter::SnapshotProblemResolved(int amount) {
  UMA_HISTOGRAM_COUNTS("Histogram.InconsistentSnapshotChildProcess",
                       std::abs(amount));
}

}  // namespace content
