// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEMORY_MEMORY_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_MEMORY_MEMORY_MESSAGE_FILTER_H_

#include "base/memory/memory_pressure_listener.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_message_filter.h"

namespace content {

// This class sends memory messages from the browser process.
// See also: child_memory_message_filter.h
class CONTENT_EXPORT MemoryMessageFilter : public BrowserMessageFilter {
 public:
  MemoryMessageFilter();

  // BrowserMessageFilter implementation.
  void OnFilterAdded(IPC::Sender* sender) override;
  void OnChannelClosing() override;
  bool OnMessageReceived(const IPC::Message& message) override;

  void SendSetPressureNotificationsSuppressed(bool suppressed);
  void SendSimulatePressureNotification(
      base::MemoryPressureListener::MemoryPressureLevel level);
  void SendPressureNotification(
      base::MemoryPressureListener::MemoryPressureLevel level);

 protected:
  ~MemoryMessageFilter() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MemoryMessageFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEMORY_MEMORY_MESSAGE_FILTER_H_
