// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WORKER_HOST_WORKER_PROCESS_HOST_H_
#define CONTENT_BROWSER_WORKER_HOST_WORKER_PROCESS_HOST_H_
#pragma once

#include <list>
#include <utility>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "content/browser/browser_child_process_host.h"
#include "content/common/content_export.h"
#include "content/browser/worker_host/worker_document_set.h"
#include "googleurl/src/gurl.h"

class ResourceDispatcherHost;

namespace content {
class ResourceContext;
}  // namespace content

// The WorkerProcessHost is the interface that represents the browser side of
// the browser <-> worker communication channel. There will be one
// WorkerProcessHost per worker process.  Currently each worker runs in its own
// process, but that may change.  However, we do assume (by storing a
// net::URLRequestContext) that a WorkerProcessHost serves a single
// BrowserContext.
class WorkerProcessHost : public BrowserChildProcessHost {
 public:
  // Contains information about each worker instance, needed to forward messages
  // between the renderer and worker processes.
  class WorkerInstance {
   public:
    WorkerInstance(const GURL& url,
                   const string16& name,
                   int worker_route_id,
                   int parent_process_id,
                   int64 main_resource_appcache_id,
                   const content::ResourceContext* resource_context);
    // Used for pending instances. Rest of the parameters are ignored.
    WorkerInstance(const GURL& url,
                   bool shared,
                   const string16& name,
                   const content::ResourceContext* resource_context);
    ~WorkerInstance();

    // Unique identifier for a worker client.
    typedef std::pair<WorkerMessageFilter*, int> FilterInfo;

    // APIs to manage the filter list for a given instance.
    void AddFilter(WorkerMessageFilter* filter, int route_id);
    void RemoveFilter(WorkerMessageFilter* filter, int route_id);
    void RemoveFilters(WorkerMessageFilter* filter);
    bool HasFilter(WorkerMessageFilter* filter, int route_id) const;
    bool RendererIsParent(int render_process_id, int render_view_id) const;
    int NumFilters() const { return filters_.size(); }
    // Returns the single filter (must only be one).
    FilterInfo GetFilter() const;

    typedef std::list<FilterInfo> FilterList;
    const FilterList& filters() const { return filters_; }

    // Checks if this WorkerInstance matches the passed url/name params
    // (per the comparison algorithm in the WebWorkers spec). This API only
    // applies to shared workers.
    bool Matches(
        const GURL& url,
        const string16& name,
        const content::ResourceContext* resource_context) const;

    // Shares the passed instance's WorkerDocumentSet with this instance. This
    // instance's current WorkerDocumentSet is dereferenced (and freed if this
    // is the only reference) as a result.
    void ShareDocumentSet(const WorkerInstance& instance) {
      worker_document_set_ = instance.worker_document_set_;
    };

    // Accessors
    bool closed() const { return closed_; }
    void set_closed(bool closed) { closed_ = closed; }
    const GURL& url() const { return url_; }
    const string16 name() const { return name_; }
    int worker_route_id() const { return worker_route_id_; }
    int parent_process_id() const { return parent_process_id_; }
    int64 main_resource_appcache_id() const {
      return main_resource_appcache_id_;
    }
    WorkerDocumentSet* worker_document_set() const {
      return worker_document_set_;
    }
    const content::ResourceContext* resource_context() const {
      return resource_context_;
    }

   private:
    // Set of all filters (clients) associated with this worker.
    GURL url_;
    bool closed_;
    string16 name_;
    int worker_route_id_;
    int parent_process_id_;
    int64 main_resource_appcache_id_;
    FilterList filters_;
    scoped_refptr<WorkerDocumentSet> worker_document_set_;
    const content::ResourceContext* const resource_context_;
  };

  WorkerProcessHost(
      const content::ResourceContext* resource_context,
      ResourceDispatcherHost* resource_dispatcher_host);
  virtual ~WorkerProcessHost();

  // Starts the process.  Returns true iff it succeeded.
  // |render_process_id| is the renderer process responsible for starting this
  // worker.
  bool Init(int render_process_id);

  // Creates a worker object in the process.
  void CreateWorker(const WorkerInstance& instance);

  // Returns true iff the given message from a renderer process was forwarded to
  // the worker.
  bool FilterMessage(const IPC::Message& message, WorkerMessageFilter* filter);

  void FilterShutdown(WorkerMessageFilter* filter);

  // Shuts down any shared workers that are no longer referenced by active
  // documents.
  void DocumentDetached(WorkerMessageFilter* filter,
                        unsigned long long document_id);

  // Terminates the given worker, i.e. based on a UI action.
  CONTENT_EXPORT void TerminateWorker(int worker_route_id);

  typedef std::list<WorkerInstance> Instances;
  const Instances& instances() const { return instances_; }

  const content::ResourceContext* resource_context() const {
    return resource_context_;
  }

 protected:
  friend class WorkerService;

  Instances& mutable_instances() { return instances_; }

 private:
  // Called when the process has been launched successfully.
  virtual void OnProcessLaunched() OVERRIDE;

  // Creates and adds the message filters.
  void CreateMessageFilters(int render_process_id);

  // IPC::Channel::Listener implementation:
  // Called when a message arrives from the worker process.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  void OnWorkerContextClosed(int worker_route_id);
  void OnAllowDatabase(int worker_route_id,
                       const GURL& url,
                       const string16& name,
                       const string16& display_name,
                       unsigned long estimated_size,
                       bool* result);
  void OnAllowFileSystem(int worker_route_id,
                         const GURL& url,
                         bool* result);

  // Relays a message to the given endpoint.  Takes care of parsing the message
  // if it contains a message port and sending it a valid route id.
  void RelayMessage(const IPC::Message& message,
                    WorkerMessageFilter* filter,
                    int route_id);

  virtual bool CanShutdown() OVERRIDE;

  // Updates the title shown in the task manager.
  void UpdateTitle();

  Instances instances_;

  const content::ResourceContext* const resource_context_;

  // A reference to the filter associated with this worker process.  We need to
  // keep this around since we'll use it when forward messages to the worker
  // process.
  scoped_refptr<WorkerMessageFilter> worker_message_filter_;

  ResourceDispatcherHost* const resource_dispatcher_host_;

  DISALLOW_COPY_AND_ASSIGN(WorkerProcessHost);
};

#endif  // CONTENT_BROWSER_WORKER_HOST_WORKER_PROCESS_HOST_H_
