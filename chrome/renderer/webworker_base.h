// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_WEBWORKER_BASE_H_
#define CHROME_RENDERER_WEBWORKER_BASE_H_
#pragma once

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
  virtual ~WebWorkerBase();

  // Creates and initializes a new dedicated worker context.
  void CreateDedicatedWorkerContext(const GURL& script_url,
                                    const string16& user_agent,
                                    const string16& source_code) {
    CreateWorkerContext(script_url, false, string16(), user_agent,
                        source_code, MSG_ROUTING_NONE, 0);
  }

  // Creates and initializes a new shared worker context.
  void CreateSharedWorkerContext(const GURL& script_url,
                                 const string16& name,
                                 const string16& user_agent,
                                 const string16& source_code,
                                 int pending_route_id,
                                 int64 script_resource_appcache_id) {
    CreateWorkerContext(script_url, true, name, user_agent,
                        source_code, pending_route_id,
                        script_resource_appcache_id);
  }

  // Returns true if the worker is running (can send messages to it).
  bool IsStarted();

  // Disconnects the worker (stops listening for incoming messages).
  void Disconnect();

  // Sends a message to the worker thread (forwarded via the RenderViewHost).
  // If WorkerStarted() has not yet been called, message is queued.
  bool Send(IPC::Message*);

  // Returns true if there are queued messages.
  bool HasQueuedMessages() { return !queued_messages_.empty(); }

  // Sends any messages currently in the queue.
  void SendQueuedMessages();

 protected:
  WebWorkerBase(ChildThread* child_thread,
                unsigned long long document_id,
                int route_id,
                int render_view_route_id,
                int parent_appcache_host_id);

  // Routing id associated with this worker - used to receive messages from the
  // worker, and also to route messages to the worker (WorkerService contains
  // a map that maps between these renderer-side route IDs and worker-side
  // routing ids).
  int route_id_;

  // The routing id for the RenderView that created this worker.
  int render_view_route_id_;

  ChildThread* child_thread_;

 private:
  void CreateWorkerContext(const GURL& script_url,
                           bool is_shared,
                           const string16& name,
                           const string16& user_agent,
                           const string16& source_code,
                           int pending_route_id,
                           int64 script_resource_appcache_id);

  // ID of our parent document (used to shutdown workers when the parent
  // document is detached).
  unsigned long long document_id_;

  // ID of our parent's appcache host, only valid for dedicated workers.
  int parent_appcache_host_id_;

  // Stores messages that were sent before the StartWorkerContext message.
  std::vector<IPC::Message*> queued_messages_;
};

#endif  // CHROME_RENDERER_WEBWORKER_BASE_H_
