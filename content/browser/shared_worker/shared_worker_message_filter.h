// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_MESSAGE_FILTER_H_

#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_message_filter.h"

namespace content {

// Handles SharedWorker related IPC messages for one renderer process by
// forwarding them to the SharedWorkerServiceImpl singleton.
class CONTENT_EXPORT SharedWorkerMessageFilter : public BrowserMessageFilter {
 public:
  using NextRoutingIDCallback = base::Callback<int(void)>;

  SharedWorkerMessageFilter(int render_process_id,
                            const NextRoutingIDCallback& callback);

  int GetNextRoutingID();
  int render_process_id() const { return render_process_id_; }

 protected:
  // This is protected, so we can define sub classes for testing.
  ~SharedWorkerMessageFilter() override;

  // BrowserMessageFilter implementation.
  void OnFilterAdded(IPC::Channel* channel) override;
  void OnFilterRemoved() override;
  void OnChannelClosing() override;
  bool OnMessageReceived(const IPC::Message& message) override;

 private:
  const int render_process_id_;
  NextRoutingIDCallback next_routing_id_callback_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(SharedWorkerMessageFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_MESSAGE_FILTER_H_
