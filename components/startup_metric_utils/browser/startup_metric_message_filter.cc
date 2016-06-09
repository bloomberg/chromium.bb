// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/startup_metric_utils/browser/startup_metric_message_filter.h"

#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "components/startup_metric_utils/common/startup_metric_messages.h"

namespace startup_metric_utils {

StartupMetricMessageFilter::StartupMetricMessageFilter()
    : content::BrowserMessageFilter(StartupMetricMsgStart) {}

bool StartupMetricMessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(StartupMetricMessageFilter, message)
    IPC_MESSAGE_HANDLER(StartupMetricHostMsg_RecordRendererMainEntryTime,
                        OnRecordRendererMainEntryTime)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void StartupMetricMessageFilter::OnRecordRendererMainEntryTime(
    const base::TimeTicks& renderer_main_entry_time) {
  RecordRendererMainEntryTime(renderer_main_entry_time);
}

}  // namespace startup_metric_utils
