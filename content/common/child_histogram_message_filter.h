// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMOM_CHILD_HISTOGRAM_MESSAGE_FILTER_H_
#define CONTENT_COMMOM_CHILD_HISTOGRAM_MESSAGE_FILTER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_flattener.h"
#include "base/metrics/histogram_snapshot_manager.h"
#include "ipc/ipc_channel_proxy.h"

class MessageLoop;

namespace base {
class HistogramSamples;
}  // namespace base

namespace content {

class ChildHistogramMessageFilter : public base::HistogramFlattener,
                                    public IPC::ChannelProxy::MessageFilter {
 public:
  ChildHistogramMessageFilter();

  // IPC::ChannelProxy::MessageFilter implementation.
  virtual void OnFilterAdded(IPC::Channel* channel) OVERRIDE;
  virtual void OnFilterRemoved() OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  void SendHistograms(int sequence_number);

  // HistogramFlattener interface (override) methods.
  virtual void RecordDelta(const base::HistogramBase& histogram,
                           const base::HistogramSamples& snapshot) OVERRIDE;
  virtual void InconsistencyDetected(
      base::HistogramBase::Inconsistency problem) OVERRIDE;
  virtual void UniqueInconsistencyDetected(
      base::HistogramBase::Inconsistency problem) OVERRIDE;
  virtual void InconsistencyDetectedInLoggedCount(int amount) OVERRIDE;

 private:
  typedef std::vector<std::string> HistogramPickledList;

  virtual ~ChildHistogramMessageFilter();

  // Message handlers.
  virtual void OnGetChildHistogramData(int sequence_number);

  // Extract snapshot data and then send it off the the Browser process.
  // Send only a delta to what we have already sent.
  void UploadAllHistograms(int sequence_number);

  IPC::Channel* channel_;

  // Collection of histograms to send to the browser.
  HistogramPickledList pickled_histograms_;

  // |histogram_snapshot_manager_| prepares histogram deltas for transmission.
  base::HistogramSnapshotManager histogram_snapshot_manager_;

  DISALLOW_COPY_AND_ASSIGN(ChildHistogramMessageFilter);
};

}  // namespace content

#endif  // CONTENT_COMMOM_CHILD_HISTOGRAM_MESSAGE_FILTER_H_
