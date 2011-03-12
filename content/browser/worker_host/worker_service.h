// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WORKER_HOST_WORKER_SERVICE_H_
#define CONTENT_BROWSER_WORKER_HOST_WORKER_SERVICE_H_
#pragma once

#include "base/basictypes.h"
#include "base/singleton.h"
#include "content/browser/worker_host/worker_process_host.h"
#include "googleurl/src/gurl.h"

class URLRequestContextGetter;
struct ViewHostMsg_CreateWorker_Params;

// A singelton for managing HTML5 web workers.
class WorkerService {
 public:
  // Returns the WorkerService singleton.
  static WorkerService* GetInstance();

  // These methods correspond to worker related IPCs.
  void CreateWorker(const ViewHostMsg_CreateWorker_Params& params,
                    int route_id,
                    WorkerMessageFilter* filter,
                    URLRequestContextGetter* request_context);
  void LookupSharedWorker(const ViewHostMsg_CreateWorker_Params& params,
                          int route_id,
                          WorkerMessageFilter* filter,
                          bool incognito,
                          bool* exists,
                          bool* url_error);
  void CancelCreateDedicatedWorker(int route_id, WorkerMessageFilter* filter);
  void ForwardToWorker(const IPC::Message& message,
                       WorkerMessageFilter* filter);
  void DocumentDetached(unsigned long long document_id,
                        WorkerMessageFilter* filter);

  void OnWorkerMessageFilterClosing(WorkerMessageFilter* filter);

  int next_worker_route_id() { return ++next_worker_route_id_; }

  // Given a worker's process id, return the IDs of the renderer process and
  // render view that created it.  For shared workers, this returns the first
  // parent.
  // TODO(dimich): This code assumes there is 1 worker per worker process, which
  // is how it is today until V8 can run in separate threads.
  bool GetRendererForWorker(int worker_process_id,
                            int* render_process_id,
                            int* render_view_id) const;
  const WorkerProcessHost::WorkerInstance* FindWorkerInstance(
      int worker_process_id);

  // Used when multiple workers can run in the same process.
  static const int kMaxWorkerProcessesWhenSharing;

  // Used when we run each worker in a separate process.
  static const int kMaxWorkersWhenSeparate;
  static const int kMaxWorkersPerTabWhenSeparate;

 private:
  friend struct DefaultSingletonTraits<WorkerService>;

  WorkerService();
  ~WorkerService();

  // Given a WorkerInstance, create an associated worker process.
  bool CreateWorkerFromInstance(WorkerProcessHost::WorkerInstance instance);

  // Returns a WorkerProcessHost object if one exists for the given domain, or
  // NULL if there are no such workers yet.
  WorkerProcessHost* GetProcessForDomain(const GURL& url);

  // Returns a WorkerProcessHost based on a strategy of creating one worker per
  // core.
  WorkerProcessHost* GetProcessToFillUpCores();

  // Returns the WorkerProcessHost from the existing set that has the least
  // number of worker instance running.
  WorkerProcessHost* GetLeastLoadedWorker();

  // Checks if we can create a worker process based on the process limit when
  // we're using a strategy of one process per core.
  bool CanCreateWorkerProcess(
      const WorkerProcessHost::WorkerInstance& instance);

  // Checks if the tab associated with the passed RenderView can create a
  // worker process based on the process limit when we're using a strategy of
  // one worker per process.
  bool TabCanCreateWorkerProcess(
      int render_process_id, int render_route_id, bool* hit_total_worker_limit);

  // Tries to see if any of the queued workers can be created.
  void TryStartingQueuedWorker();

  // APIs for manipulating our set of pending shared worker instances.
  WorkerProcessHost::WorkerInstance* CreatePendingInstance(
      const GURL& url, const string16& name, bool incognito);
  WorkerProcessHost::WorkerInstance* FindPendingInstance(
      const GURL& url, const string16& name, bool incognito);
  void RemovePendingInstances(
      const GURL& url, const string16& name, bool incognito);

  WorkerProcessHost::WorkerInstance* FindSharedWorkerInstance(
      const GURL& url, const string16& name, bool incognito);

  NotificationRegistrar registrar_;
  int next_worker_route_id_;

  WorkerProcessHost::Instances queued_workers_;

  // These are shared workers that have been looked up, but not created yet.
  // We need to keep a list of these to synchronously detect shared worker
  // URL mismatches when two pages launch shared workers simultaneously.
  WorkerProcessHost::Instances pending_shared_workers_;

  DISALLOW_COPY_AND_ASSIGN(WorkerService);
};

#endif  // CONTENT_BROWSER_WORKER_HOST_WORKER_SERVICE_H_
