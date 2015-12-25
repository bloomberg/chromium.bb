// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_GEOFENCING_GEOFENCING_MESSAGE_FILTER_H_
#define CONTENT_CHILD_GEOFENCING_GEOFENCING_MESSAGE_FILTER_H_

#include "base/macros.h"
#include "content/child/worker_thread_message_filter.h"

namespace content {

class GeofencingMessageFilter : public WorkerThreadMessageFilter {
 public:
  explicit GeofencingMessageFilter(ThreadSafeSender* thread_safe_sender);

 private:
  ~GeofencingMessageFilter() override;

  // WorkerThreadMessageFilter:
  bool ShouldHandleMessage(const IPC::Message& msg) const override;
  void OnFilteredMessageReceived(const IPC::Message& msg) override;
  bool GetWorkerThreadIdForMessage(const IPC::Message& msg,
                                   int* ipc_thread_id) override;

  DISALLOW_COPY_AND_ASSIGN(GeofencingMessageFilter);
};

}  // namespace content

#endif  // CONTENT_CHILD_GEOFENCING_GEOFENCING_MESSAGE_FILTER_H_
