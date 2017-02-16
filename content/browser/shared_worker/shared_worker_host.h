// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_HOST_H_
#define CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_HOST_H_

#include <list>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "content/browser/shared_worker/worker_document_set.h"

class GURL;

namespace IPC {
class Message;
}

namespace content {

class MessagePort;
class SharedWorkerMessageFilter;
class SharedWorkerInstance;

// The SharedWorkerHost is the interface that represents the browser side of
// the browser <-> worker communication channel. This is owned by
// SharedWorkerServiceImpl and destructed when a worker context or worker's
// message filter is closed.
class SharedWorkerHost {
 public:
  SharedWorkerHost(SharedWorkerInstance* instance,
                   SharedWorkerMessageFilter* filter,
                   int worker_route_id);
  ~SharedWorkerHost();

  // Starts the SharedWorker in the renderer process which is associated with
  // |filter_|.
  void Start(bool pause_on_start);

  // Returns true iff the given message from a renderer process was forwarded to
  // the worker.
  bool SendConnectToWorker(int worker_route_id,
                           const MessagePort& port,
                           SharedWorkerMessageFilter* filter);

  // Handles the shutdown of the filter. If the worker has no other client,
  // sends TerminateWorkerContext message to shut it down.
  void FilterShutdown(SharedWorkerMessageFilter* filter);

  // Shuts down any shared workers that are no longer referenced by active
  // documents.
  void DocumentDetached(SharedWorkerMessageFilter* filter,
                        unsigned long long document_id);

  // Removes the references to shared workers from the all documents in the
  // renderer frame. And shuts down any shared workers that are no longer
  // referenced by active documents.
  void RenderFrameDetached(int render_process_id, int render_frame_id);

  void CountFeature(uint32_t feature);
  void WorkerContextClosed();
  void WorkerContextDestroyed();
  void WorkerReadyForInspection();
  void WorkerScriptLoaded();
  void WorkerScriptLoadFailed();
  void WorkerConnected(int connection_request_id);
  void AllowFileSystem(const GURL& url,
                       std::unique_ptr<IPC::Message> reply_msg);
  void AllowIndexedDB(const GURL& url,
                      const base::string16& name,
                      bool* result);

  // Terminates the given worker, i.e. based on a UI action.
  void TerminateWorker();

  void AddFilter(SharedWorkerMessageFilter* filter, int route_id);

  SharedWorkerInstance* instance() { return instance_.get(); }
  WorkerDocumentSet* worker_document_set() const {
    return worker_document_set_.get();
  }
  SharedWorkerMessageFilter* worker_render_filter() const {
    return worker_render_filter_;
  }
  int process_id() const { return worker_process_id_; }
  int worker_route_id() const { return worker_route_id_; }
  bool IsAvailable() const;

 private:
  // Unique identifier for a worker client.
  class FilterInfo {
   public:
    FilterInfo(SharedWorkerMessageFilter* filter, int route_id)
        : filter_(filter), route_id_(route_id), connection_request_id_(0) {}
    SharedWorkerMessageFilter* filter() const { return filter_; }
    int route_id() const { return route_id_; }
    int connection_request_id() const { return connection_request_id_; }
    void set_connection_request_id(int id) { connection_request_id_ = id; }

   private:
    SharedWorkerMessageFilter* filter_;
    const int route_id_;
    int connection_request_id_;
  };

  using FilterList = std::list<FilterInfo>;

  // Return a vector of all the render process/render frame IDs.
  std::vector<std::pair<int, int> > GetRenderFrameIDsForWorker();

  void RemoveFilters(SharedWorkerMessageFilter* filter);
  bool HasFilter(SharedWorkerMessageFilter* filter, int route_id) const;
  void SetConnectionRequestID(SharedWorkerMessageFilter* filter,
                              int route_id,
                              int connection_request_id);
  void AllowFileSystemResponse(std::unique_ptr<IPC::Message> reply_msg,
                               bool allowed);

  // Sends |message| to the SharedWorker.
  bool Send(IPC::Message* message);

  std::unique_ptr<SharedWorkerInstance> instance_;
  scoped_refptr<WorkerDocumentSet> worker_document_set_;
  FilterList filters_;

  // A message filter for a renderer process that hosts a worker. This is always
  // valid because this host is destructed immediately after the filter is
  // closed (see SharedWorkerServiceImpl::OnSharedWorkerMessageFilterClosing).
  SharedWorkerMessageFilter* worker_render_filter_;

  const int worker_process_id_;
  const int worker_route_id_;
  int next_connection_request_id_;
  bool termination_message_sent_ = false;
  bool closed_ = false;
  const base::TimeTicks creation_time_;

  // This is the set of features that this worker has used. The values must be
  // from blink::UseCounter::Feature enum.
  std::set<uint32_t> used_features_;

  base::WeakPtrFactory<SharedWorkerHost> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SharedWorkerHost);
};
}  // namespace content

#endif  // CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_HOST_H_
