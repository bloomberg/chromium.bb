// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/histogram_message_filter.h"

#include "base/process_util.h"
#include "content/browser/histogram_controller.h"
#include "content/browser/tcmalloc_internals_request_job.h"
#include "content/common/child_process_messages.h"

namespace content {

HistogramMessageFilter::HistogramMessageFilter() {}

void HistogramMessageFilter::OnChannelConnected(int32 peer_pid) {
  BrowserMessageFilter::OnChannelConnected(peer_pid);
}

bool HistogramMessageFilter::OnMessageReceived(const IPC::Message& message,
                                              bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(HistogramMessageFilter, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(ChildProcessHostMsg_ChildHistogramData,
                        OnChildHistogramData)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

HistogramMessageFilter::~HistogramMessageFilter() {}

void HistogramMessageFilter::OnChildHistogramData(
    int sequence_number,
    const std::vector<std::string>& pickled_histograms) {
  HistogramController::GetInstance()->OnHistogramDataCollected(
      sequence_number, pickled_histograms);
}
}
