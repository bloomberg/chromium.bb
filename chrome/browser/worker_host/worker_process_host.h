// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WORKER_HOST_WORKER_PROCESS_HOST_H_
#define CHROME_BROWSER_WORKER_HOST_WORKER_PROCESS_HOST_H_

#include <list>

#include "base/basictypes.h"
#include "base/task.h"
#include "chrome/common/child_process_host.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_channel.h"

class WorkerProcessHost : public ChildProcessHost {
 public:
  // Contains information about each worker instance, needed to forward messages
  // between the renderer and worker processes.
  struct WorkerInstance {
    GURL url;
    bool is_shared;
    string16 name;
    int renderer_id;
    int render_view_route_id;
    int worker_route_id;
    IPC::Message::Sender* sender;
    int sender_id;
    int sender_route_id;
  };

  WorkerProcessHost(ResourceDispatcherHost* resource_dispatcher_host_);
  ~WorkerProcessHost();

  // Starts the process.  Returns true iff it succeeded.
  bool Init();

  // Creates a worker object in the process.
  void CreateWorker(const WorkerInstance& instance);

  // Returns true iff the given message from a renderer process was forwarded to
  // the worker.
  bool FilterMessage(const IPC::Message& message, int sender_pid);

  void SenderShutdown(IPC::Message::Sender* sender);

 protected:
  friend class WorkerService;

  typedef std::list<WorkerInstance> Instances;
  const Instances& instances() const { return instances_; }

 private:
  // ResourceDispatcherHost::Receiver implementation:
  virtual URLRequestContext* GetRequestContext(
      uint32 request_id,
      const ViewHostMsg_Resource_Request& request_data);

  // Called when a message arrives from the worker process.
  void OnMessageReceived(const IPC::Message& message);

  // Given a Sender, returns the callback that generates a new routing id.
  static CallbackWithReturnValue<int>::Type* GetNextRouteIdCallback(
      IPC::Message::Sender* sender);

  // Relays a message to the given endpoint.  Takes care of parsing the message
  // if it contains a message port and sending it a valid route id.
  static void RelayMessage(const IPC::Message& message,
                           IPC::Message::Sender* sender,
                           int route_id,
                           CallbackWithReturnValue<int>::Type* next_route_id);

  virtual bool CanShutdown() { return instances_.empty(); }

  // Updates the title shown in the task manager.
  void UpdateTitle();

  void OnCreateWorker(const GURL& url,
                      bool is_shared,
                      const string16& name,
                      int render_view_route_id,
                      int* route_id);
  void OnCancelCreateDedicatedWorker(int route_id);
  void OnForwardToWorker(const IPC::Message& message);

  Instances instances_;

  // A callback to create a routing id for the associated worker process.
  scoped_ptr<CallbackWithReturnValue<int>::Type> next_route_id_callback_;

  DISALLOW_COPY_AND_ASSIGN(WorkerProcessHost);
};

#endif  // CHROME_BROWSER_WORKER_HOST_WORKER_PROCESS_HOST_H_
