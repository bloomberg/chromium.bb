// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WORKER_WORKER_THREAD_H_
#define CONTENT_WORKER_WORKER_THREAD_H_
#pragma once

#include <set>

#include "content/common/child_thread.h"

class AppCacheDispatcher;
class DBMessageFilter;
class WebDatabaseObserverImpl;
class WebSharedWorkerStub;
struct WorkerProcessMsg_CreateWorker_Params;
class WorkerWebKitPlatformSupportImpl;

class WorkerThread : public ChildThread {
 public:
  WorkerThread();
  virtual ~WorkerThread();

  // Returns the one worker thread.
  static WorkerThread* current();

  // Invoked from stub constructors/destructors. Stubs own themselves.
  void AddWorkerStub(WebSharedWorkerStub* stub);
  void RemoveWorkerStub(WebSharedWorkerStub* stub);

  AppCacheDispatcher* appcache_dispatcher() {
    return appcache_dispatcher_.get();
  }

 private:
  virtual bool OnControlMessageReceived(const IPC::Message& msg) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

  void OnCreateWorker(const WorkerProcessMsg_CreateWorker_Params& params);

  scoped_ptr<WorkerWebKitPlatformSupportImpl> webkit_platform_support_;
  scoped_ptr<AppCacheDispatcher> appcache_dispatcher_;
  scoped_ptr<WebDatabaseObserverImpl> web_database_observer_impl_;
  scoped_refptr<DBMessageFilter> db_message_filter_;

  typedef std::set<WebSharedWorkerStub*> WorkerStubsList;
  WorkerStubsList worker_stubs_;

  DISALLOW_COPY_AND_ASSIGN(WorkerThread);
};

#endif  // CONTENT_WORKER_WORKER_THREAD_H_
