// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PROFILER_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_PROFILER_MESSAGE_FILTER_H_

#include <string>

#include "content/browser/browser_message_filter.h"

namespace base {
class DictionaryValue;
}

// This class sends and receives profiler messages in the browser process.
class ProfilerMessageFilter : public BrowserMessageFilter {
 public:
  ProfilerMessageFilter();
  virtual ~ProfilerMessageFilter();

  // BrowserMessageFilter implementation.
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;

  // BrowserMessageFilter implementation.
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

 private:
  // Message handlers.
  void OnChildProfilerData(int sequence_number,
                           const base::DictionaryValue& profiler_data);

  DISALLOW_COPY_AND_ASSIGN(ProfilerMessageFilter);
};

#endif  // CONTENT_BROWSER_PROFILER_MESSAGE_FILTER_H_

