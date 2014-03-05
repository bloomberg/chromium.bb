// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_HOST_H_
#define CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_HOST_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "content/browser/shared_worker/shared_worker_message_filter.h"

class GURL;

namespace IPC {
class Message;
}

namespace content {
class SharedWorkerMessageFilter;
class SharedWorkerInstance;

// The SharedWorkerHost is the interface that represents the browser side of
// the browser <-> worker communication channel.
class SharedWorkerHost {
 public:
  explicit SharedWorkerHost(SharedWorkerInstance* instance);
  ~SharedWorkerHost();

  // Sends |message| to the SharedWorker.
  bool Send(IPC::Message* message);

  // Starts the SharedWorker in the renderer process which is associated with
  // |filter|.
  void Init(SharedWorkerMessageFilter* filter);

  // Returns true iff the given message from a renderer process was forwarded to
  // the worker.
  bool FilterMessage(const IPC::Message& message,
                     SharedWorkerMessageFilter* filter);

  // Handles the shutdown of the filter. If the worker has no other client,
  // sends TerminateWorkerContext message to shut it down.
  void FilterShutdown(SharedWorkerMessageFilter* filter);

  // Shuts down any shared workers that are no longer referenced by active
  // documents.
  void DocumentDetached(SharedWorkerMessageFilter* filter,
                        unsigned long long document_id);

  void WorkerContextClosed();
  void WorkerScriptLoaded();
  void WorkerScriptLoadFailed();
  void WorkerConnected(int message_port_id);
  void WorkerContextDestroyed();
  void AllowDatabase(const GURL& url,
                     const base::string16& name,
                     const base::string16& display_name,
                     unsigned long estimated_size,
                     bool* result);
  void AllowFileSystem(const GURL& url, bool* result);
  void AllowIndexedDB(const GURL& url,
                      const base::string16& name,
                      bool* result);

  // Terminates the given worker, i.e. based on a UI action.
  void TerminateWorker();

  SharedWorkerInstance* instance() { return instance_.get(); }
  SharedWorkerMessageFilter* container_render_filter() const {
    return container_render_filter_;
  }
  int process_id() const {
    return container_render_filter_->render_process_id();
  }
  int worker_route_id() const { return worker_route_id_; }

 private:
  // Relays |message| to the SharedWorker. Takes care of parsing the message if
  // it contains a message port and sending it a valid route id.
  void RelayMessage(const IPC::Message& message,
                    SharedWorkerMessageFilter* incoming_filter);

  scoped_ptr<SharedWorkerInstance> instance_;
  SharedWorkerMessageFilter* container_render_filter_;
  int worker_route_id_;
  DISALLOW_COPY_AND_ASSIGN(SharedWorkerHost);
};
}  // namespace content

#endif  // CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_HOST_H_
