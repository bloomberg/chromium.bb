// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WORKER_HOST_WORKER_SERVICE_H_
#define CHROME_BROWSER_WORKER_HOST_WORKER_SERVICE_H_
#pragma once

#include <list>

#include "base/basictypes.h"
#include "base/singleton.h"
#include "chrome/browser/worker_host/worker_process_host.h"
#include "chrome/common/notification_registrar.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_message.h"

class ChromeURLRequestContext;
class ResourceDispatcherHost;

class WorkerService : public NotificationObserver {
 public:
  // Returns the WorkerService singleton.
  static WorkerService* GetInstance();

  // Initialize the WorkerService.  OK to be called multiple times.
  void Initialize(ResourceDispatcherHost* rdh);

  // Creates a decidated worker.  Returns true on success.
  bool CreateDedicatedWorker(const GURL &url,
                             bool is_off_the_record,
                             unsigned long long document_id,
                             int renderer_pid,
                             int render_view_route_id,
                             IPC::Message::Sender* sender,
                             int sender_route_id,
                             int parent_process_id,
                             int parent_appcache_host_id,
                             ChromeURLRequestContext* request_context);

  // Creates a shared worker.  Returns true on success.
  bool CreateSharedWorker(const GURL &url,
                          bool is_off_the_record,
                          const string16& name,
                          unsigned long long document_id,
                          int renderer_pid,
                          int render_view_route_id,
                          IPC::Message::Sender* sender,
                          int sender_route_id,
                          int64 main_resource_appcache_id,
                          ChromeURLRequestContext* request_context);

  // Validates the passed URL and checks for the existence of matching shared
  // worker. Returns true if the url was found, and sets the url_mismatch out
  // param to true/false depending on whether there's a url mismatch with an
  // existing shared worker with the same name.
  bool LookupSharedWorker(const GURL &url,
                          const string16& name,
                          bool off_the_record,
                          unsigned long long document_id,
                          int renderer_pid,
                          int render_view_route_id,
                          IPC::Message::Sender* sender,
                          int sender_route_id,
                          bool* url_mismatch);

  // Notification from the renderer that a given document has detached, so any
  // associated shared workers can be shut down.
  void DocumentDetached(IPC::Message::Sender* sender,
                        unsigned long long document_id);

  // Cancel creation of a dedicated worker that hasn't started yet.
  void CancelCreateDedicatedWorker(IPC::Message::Sender* sender,
                                   int sender_route_id);

  // Called by the worker creator when a message arrives that should be
  // forwarded to the worker process.
  void ForwardMessage(const IPC::Message& message,
                      IPC::Message::Sender* sender);

  int next_worker_route_id() { return ++next_worker_route_id_; }

  // TODO(dimich): This code assumes there is 1 worker per worker process, which
  // is how it is today until V8 can run in separate threads.
  const WorkerProcessHost::WorkerInstance* FindWorkerInstance(
      int worker_process_id);

  WorkerProcessHost::WorkerInstance* FindSharedWorkerInstance(
      const GURL& url, const string16& name, bool off_the_record);

  // Used when multiple workers can run in the same process.
  static const int kMaxWorkerProcessesWhenSharing;

  // Used when we run each worker in a separate process.
  static const int kMaxWorkersWhenSeparate;
  static const int kMaxWorkersPerTabWhenSeparate;

 private:
  friend struct DefaultSingletonTraits<WorkerService>;

  WorkerService();
  ~WorkerService();

  bool CreateWorker(const GURL &url,
                    bool is_shared,
                    bool is_off_the_record,
                    const string16& name,
                    unsigned long long document_id,
                    int renderer_pid,
                    int render_view_route_id,
                    IPC::Message::Sender* sender,
                    int sender_route_id,
                    int parent_process_id,
                    int parent_appcache_host_id,
                    int64 main_resource_appcache_id,
                    ChromeURLRequestContext* request_context);

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
      int renderer_id, int render_view_route_id, bool* hit_total_worker_limit);

  // NotificationObserver interface.
  void Observe(NotificationType type,
               const NotificationSource& source,
               const NotificationDetails& details);

  // Notifies us that a process that's talking to a worker has shut down.
  void SenderShutdown(IPC::Message::Sender* sender);

  // Notifies us that a worker process has closed.
  void WorkerProcessDestroyed(WorkerProcessHost* process);

  // APIs for manipulating our set of pending shared worker instances.
  WorkerProcessHost::WorkerInstance* CreatePendingInstance(
      const GURL& url, const string16& name, bool off_the_record);
  WorkerProcessHost::WorkerInstance* FindPendingInstance(
      const GURL& url, const string16& name, bool off_the_record);
  void RemovePendingInstances(
      const GURL& url, const string16& name, bool off_the_record);

  NotificationRegistrar registrar_;
  int next_worker_route_id_;
  ResourceDispatcherHost* resource_dispatcher_host_;

  WorkerProcessHost::Instances queued_workers_;

  // These are shared workers that have been looked up, but not created yet.
  // We need to keep a list of these to synchronously detect shared worker
  // URL mismatches when two pages launch shared workers simultaneously.
  WorkerProcessHost::Instances pending_shared_workers_;

  DISALLOW_COPY_AND_ASSIGN(WorkerService);
};

#endif  // CHROME_BROWSER_WORKER_HOST_WORKER_SERVICE_H_
