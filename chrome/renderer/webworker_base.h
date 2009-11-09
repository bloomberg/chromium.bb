// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_WEBWORKER_BASE_H_
#define CHROME_RENDERER_WEBWORKER_BASE_H_

#include <vector>

#include "base/basictypes.h"
#include "ipc/ipc_channel.h"

class ChildThread;
class GURL;

// WebWorkerBase is the common base class used by both WebWorkerProxy and
// WebSharedWorker. It contains logic to support starting up both dedicated
// and shared workers, and handling message queueing while waiting for the
// worker process to start.
class WebWorkerBase : public IPC::Channel::Listener {
 public:
  WebWorkerBase(ChildThread* child_thread,
                int route_id,
                int render_view_route_id);

  virtual ~WebWorkerBase();

  // Creates and initializes a new worker context.
  void CreateWorkerContext(const GURL& script_url,
                           bool is_shared,
                           const string16& name,
                           const string16& user_agent,
                           const string16& source_code);

  // Returns true if the worker is running (can send messages to it).
  bool IsStarted();

  // Disconnects the worker (stops listening for incoming messages).
  virtual void Disconnect();

  // Sends a message to the worker thread (forwarded via the RenderViewHost).
  // If WorkerStarted() has not yet been called, message is queued.
  bool Send(IPC::Message*);

  // Returns true if there are queued messages.
  bool HasQueuedMessages() { return queued_messages_.size() != 0; }

  // Sends any messages currently in the queue.
  void SendQueuedMessages();

 protected:
  // Routing id associated with this worker - used to receive messages from the
  // worker, and also to route messages to the worker (WorkerService contains
  // a map that maps between these renderer-side route IDs and worker-side
  // routing ids).
  int route_id_;

  // The routing id for the RenderView that created this worker.
  int render_view_route_id_;

  ChildThread* child_thread_;

 private:
  // Stores messages that were sent before the StartWorkerContext message.
  std::vector<IPC::Message*> queued_messages_;
};

#endif  // CHROME_RENDERER_WEBWORKER_BASE_H_
