// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WORKER_HOST_WORKER_PROCESS_HOST_H_
#define CHROME_BROWSER_WORKER_HOST_WORKER_PROCESS_HOST_H_
#pragma once

#include <list>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/file_path.h"
#include "base/ref_counted.h"
#include "chrome/browser/browser_child_process_host.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/worker_host/worker_document_set.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_channel.h"

class AppCacheDispatcherHost;
class BlobDispatcherHost;
class ChromeURLRequestContext;
class ChromeURLRequestContextGetter;
class DatabaseDispatcherHost;
class FileSystemDispatcherHost;
class FileUtilitiesDispatcherHost;
class MimeRegistryDispatcher;
namespace webkit_database {
class DatabaseTracker;
}  // namespace webkit_database

struct ViewHostMsg_CreateWorker_Params;

// The WorkerProcessHost is the interface that represents the browser side of
// the browser <-> worker communication channel. There will be one
// WorkerProcessHost per worker process.  Currently each worker runs in its own
// process, but that may change.  However, we do assume [by storing a
// ChromeURLRequestContext] that a WorkerProcessHost serves a single Profile.
class WorkerProcessHost : public BrowserChildProcessHost {
 public:

  // Contains information about each worker instance, needed to forward messages
  // between the renderer and worker processes.
  class WorkerInstance {
   public:
    WorkerInstance(const GURL& url,
                   bool shared,
                   bool off_the_record,
                   const string16& name,
                   int worker_route_id,
                   int parent_process_id,
                   int parent_appcache_host_id,
                   int64 main_resource_appcache_id,
                   ChromeURLRequestContext* request_context);
    ~WorkerInstance();

    // Unique identifier for a worker client.
    typedef std::pair<IPC::Message::Sender*, int> SenderInfo;

    // APIs to manage the sender list for a given instance.
    void AddSender(IPC::Message::Sender* sender, int sender_route_id);
    void RemoveSender(IPC::Message::Sender* sender, int sender_route_id);
    void RemoveSenders(IPC::Message::Sender* sender);
    bool HasSender(IPC::Message::Sender* sender, int sender_route_id) const;
    bool RendererIsParent(int renderer_id, int render_view_route_id) const;
    int NumSenders() const { return senders_.size(); }
    // Returns the single sender (must only be one).
    SenderInfo GetSender() const;

    typedef std::list<SenderInfo> SenderList;
    const SenderList& senders() const { return senders_; }

    // Checks if this WorkerInstance matches the passed url/name params
    // (per the comparison algorithm in the WebWorkers spec). This API only
    // applies to shared workers.
    bool Matches(
        const GURL& url, const string16& name, bool off_the_record) const;

    // Shares the passed instance's WorkerDocumentSet with this instance. This
    // instance's current WorkerDocumentSet is dereferenced (and freed if this
    // is the only reference) as a result.
    void ShareDocumentSet(const WorkerInstance& instance) {
      worker_document_set_ = instance.worker_document_set_;
    };

    // Accessors
    bool shared() const { return shared_; }
    bool off_the_record() const { return off_the_record_; }
    bool closed() const { return closed_; }
    void set_closed(bool closed) { closed_ = closed; }
    const GURL& url() const { return url_; }
    const string16 name() const { return name_; }
    int worker_route_id() const { return worker_route_id_; }
    int parent_process_id() const { return parent_process_id_; }
    int parent_appcache_host_id() const { return parent_appcache_host_id_; }
    int64 main_resource_appcache_id() const {
      return main_resource_appcache_id_;
    }
    WorkerDocumentSet* worker_document_set() const {
      return worker_document_set_;
    }
    ChromeURLRequestContext* request_context() const {
      return request_context_;
    }

   private:
    // Set of all senders (clients) associated with this worker.
    GURL url_;
    bool shared_;
    bool off_the_record_;
    bool closed_;
    string16 name_;
    int worker_route_id_;
    int parent_process_id_;
    int parent_appcache_host_id_;
    int64 main_resource_appcache_id_;
    scoped_refptr<ChromeURLRequestContext> request_context_;
    SenderList senders_;
    scoped_refptr<WorkerDocumentSet> worker_document_set_;
  };

  WorkerProcessHost(
      ResourceDispatcherHost* resource_dispatcher_host,
      ChromeURLRequestContext* request_context);
  ~WorkerProcessHost();

  // Starts the process.  Returns true iff it succeeded.
  bool Init();

  // Creates a worker object in the process.
  void CreateWorker(const WorkerInstance& instance);

  // Returns true iff the given message from a renderer process was forwarded to
  // the worker.
  bool FilterMessage(const IPC::Message& message, IPC::Message::Sender* sender);

  void SenderShutdown(IPC::Message::Sender* sender);

  // Shuts down any shared workers that are no longer referenced by active
  // documents.
  void DocumentDetached(IPC::Message::Sender* sender,
                        unsigned long long document_id);

  ChromeURLRequestContext* request_context() const {
    return request_context_;
  }

 protected:
  friend class WorkerService;

  typedef std::list<WorkerInstance> Instances;
  const Instances& instances() const { return instances_; }
  Instances& mutable_instances() { return instances_; }

 private:
  // ResourceDispatcherHost::Receiver implementation:
  virtual URLRequestContext* GetRequestContext(
      uint32 request_id,
      const ViewHostMsg_Resource_Request& request_data);

  // Called when a message arrives from the worker process.
  virtual void OnMessageReceived(const IPC::Message& message);

  // Called when the process has been launched successfully.
  virtual void OnProcessLaunched();

  // Called when the app invokes close() from within worker context.
  void OnWorkerContextClosed(int worker_route_id);

  // Called if a worker tries to connect to a shared worker.
  void OnLookupSharedWorker(const ViewHostMsg_CreateWorker_Params& params,
                            bool* exists,
                            int* route_id,
                            bool* url_error);

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

  void OnCreateWorker(const ViewHostMsg_CreateWorker_Params& params,
                      int* route_id);
  void OnCancelCreateDedicatedWorker(int route_id);
  void OnForwardToWorker(const IPC::Message& message);

  Instances instances_;

  scoped_refptr<ChromeURLRequestContext> request_context_;
  scoped_ptr<AppCacheDispatcherHost> appcache_dispatcher_host_;
  scoped_refptr<DatabaseDispatcherHost> db_dispatcher_host_;
  scoped_ptr<BlobDispatcherHost> blob_dispatcher_host_;
  scoped_refptr<FileSystemDispatcherHost> file_system_dispatcher_host_;
  scoped_refptr<FileUtilitiesDispatcherHost> file_utilities_dispatcher_host_;
  scoped_refptr<MimeRegistryDispatcher> mime_registry_dispatcher_;

  // A callback to create a routing id for the associated worker process.
  scoped_ptr<CallbackWithReturnValue<int>::Type> next_route_id_callback_;

  DISALLOW_COPY_AND_ASSIGN(WorkerProcessHost);
};

#endif  // CHROME_BROWSER_WORKER_HOST_WORKER_PROCESS_HOST_H_
