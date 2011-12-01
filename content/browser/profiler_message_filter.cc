// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/profiler_message_filter.h"

#include "base/tracked_objects.h"
#include "base/values.h"
#include "content/browser/profiler_controller_impl.h"
#include "content/common/child_process_messages.h"

using content::BrowserThread;

ProfilerMessageFilter::ProfilerMessageFilter() {
}

ProfilerMessageFilter::~ProfilerMessageFilter() {
}

void ProfilerMessageFilter::OnChannelConnected(int32 peer_pid) {
  BrowserMessageFilter::OnChannelConnected(peer_pid);

  bool enable = tracked_objects::ThreadData::tracking_status();
  Send(new ChildProcessMsg_SetProfilerStatus(enable));
}

bool ProfilerMessageFilter::OnMessageReceived(const IPC::Message& message,
                                              bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(ProfilerMessageFilter, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(ChildProcessHostMsg_ChildProfilerData,
                        OnChildProfilerData)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

void ProfilerMessageFilter::OnChildProfilerData(
    int sequence_number,
    const base::DictionaryValue& profiler_data) {
  base::DictionaryValue* dictionary_value = new base::DictionaryValue;
  dictionary_value->MergeDictionary(&profiler_data);
  // OnProfilerDataCollected assumes the ownership of profiler_data.
  content::ProfilerControllerImpl::GetInstance()->OnProfilerDataCollected(
      sequence_number, dictionary_value);
}
