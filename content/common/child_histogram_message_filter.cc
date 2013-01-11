// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/child_histogram_message_filter.h"

#include <ctype.h>

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/metrics/statistics_recorder.h"
#include "base/pickle.h"
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
      FROM_HERE, base::Bind(&ChildHistogramMessageFilter::UploadAllHistograms,
                            this, sequence_number));
}

void ChildHistogramMessageFilter::OnGetChildHistogramData(int sequence_number) {
  UploadAllHistograms(sequence_number);
}

void ChildHistogramMessageFilter::UploadAllHistograms(int sequence_number) {
  DCHECK_EQ(0u, pickled_histograms_.size());

  base::StatisticsRecorder::CollectHistogramStats("ChildProcess");

  // Push snapshots into our pickled_histograms_ vector.
  // Note: Before serializing, we set the kIPCSerializationSourceFlag for all
  // the histograms, so that the receiving process can distinguish them from the
  // local histograms.
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
    const base::HistogramSamples& snapshot) {
  DCHECK_NE(0, snapshot.TotalCount());

  Pickle pickle;
  histogram.SerializeInfo(&pickle);
  snapshot.Serialize(&pickle);

  pickled_histograms_.push_back(
      std::string(static_cast<const char*>(pickle.data()), pickle.size()));
}

void ChildHistogramMessageFilter::InconsistencyDetected(
    base::Histogram::Inconsistencies problem) {
  UMA_HISTOGRAM_ENUMERATION("Histogram.InconsistenciesChildProcess",
                            problem, base::Histogram::NEVER_EXCEEDED_VALUE);
}

void ChildHistogramMessageFilter::UniqueInconsistencyDetected(
    base::Histogram::Inconsistencies problem) {
  UMA_HISTOGRAM_ENUMERATION("Histogram.InconsistenciesChildProcessUnique",
                            problem, base::Histogram::NEVER_EXCEEDED_VALUE);
}

void ChildHistogramMessageFilter::InconsistencyDetectedInLoggedCount(
    int amount) {
  UMA_HISTOGRAM_COUNTS("Histogram.InconsistentSnapshotChildProcess",
                       std::abs(amount));
}

}  // namespace content
