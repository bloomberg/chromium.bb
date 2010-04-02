// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_WORKER_WORKER_THREAD_H_
#define CHROME_WORKER_WORKER_THREAD_H_

#include <set>

#include "chrome/common/child_thread.h"

class GURL;
class DBMessageFilter;
class WebDatabaseObserverImpl;
class WebWorkerStubBase;
class WorkerWebKitClientImpl;

class WorkerThread : public ChildThread {
 public:
  WorkerThread();
  ~WorkerThread();

  // Returns the one worker thread.
  static WorkerThread* current();

  // Invoked from stub constructors/destructors. Stubs own themselves.
  void AddWorkerStub(WebWorkerStubBase* stub);
  void RemoveWorkerStub(WebWorkerStubBase* stub);

 private:
  scoped_ptr<WebDatabaseObserverImpl> web_database_observer_impl_;

  virtual void OnControlMessageReceived(const IPC::Message& msg);
  virtual void OnChannelError();

  void OnCreateWorker(
      const GURL& url, bool is_shared, const string16& name, int route_id);

  scoped_ptr<WorkerWebKitClientImpl> webkit_client_;

  scoped_refptr<DBMessageFilter> db_message_filter_;

  typedef std::set<WebWorkerStubBase*> WorkerStubsList;
  WorkerStubsList worker_stubs_;

  DISALLOW_COPY_AND_ASSIGN(WorkerThread);
};

#endif  // CHROME_WORKER_WORKER_THREAD_H_
