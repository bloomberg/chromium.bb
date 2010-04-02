// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/worker/worker_thread.h"

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/thread_local.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/db_message_filter.h"
#include "chrome/common/web_database_observer_impl.h"
#include "chrome/common/worker_messages.h"
#include "chrome/worker/webworker_stub.h"
#include "chrome/worker/websharedworker_stub.h"
#include "chrome/worker/worker_webkitclient_impl.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDatabase.h"
#include "third_party/WebKit/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/WebKit/chromium/public/WebRuntimeFeatures.h"

using WebKit::WebRuntimeFeatures;

static base::LazyInstance<base::ThreadLocalPointer<WorkerThread> > lazy_tls(
    base::LINKER_INITIALIZED);


WorkerThread::WorkerThread() {
  lazy_tls.Pointer()->Set(this);
  webkit_client_.reset(new WorkerWebKitClientImpl);
  WebKit::initialize(webkit_client_.get());

  web_database_observer_impl_.reset(new WebDatabaseObserverImpl(this));
  WebKit::WebDatabase::setObserver(web_database_observer_impl_.get());

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  WebKit::WebRuntimeFeatures::enableDatabase(
      !command_line.HasSwitch(switches::kDisableDatabases));

  db_message_filter_ = new DBMessageFilter();
  channel()->AddFilter(db_message_filter_.get());

#if defined(OS_WIN)
  // We don't yet support notifications on non-Windows, so hide it from pages.
  WebRuntimeFeatures::enableNotifications(
      !command_line.HasSwitch(switches::kDisableDesktopNotifications));
#endif

  WebRuntimeFeatures::enableSockets(
      !command_line.HasSwitch(switches::kDisableWebSockets));
}

WorkerThread::~WorkerThread() {
  // Shutdown in reverse of the initialization order.
  channel()->RemoveFilter(db_message_filter_.get());
  db_message_filter_ = NULL;

  WebKit::shutdown();
  lazy_tls.Pointer()->Set(NULL);
}

WorkerThread* WorkerThread::current() {
  return lazy_tls.Pointer()->Get();
}

void WorkerThread::OnControlMessageReceived(const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(WorkerThread, msg)
    IPC_MESSAGE_HANDLER(WorkerProcessMsg_CreateWorker, OnCreateWorker)
  IPC_END_MESSAGE_MAP()
}

void WorkerThread::OnCreateWorker(const GURL& url,
                                  bool is_shared,
                                  const string16& name,
                                  int route_id) {
  // WebWorkerStub and WebSharedWorkerStub own themselves.
  if (is_shared)
    new WebSharedWorkerStub(name, route_id);
  else
    new WebWorkerStub(url, route_id);
}

// The browser process is likely dead. Terminate all workers.
void WorkerThread::OnChannelError() {
  set_on_channel_error_called(true);

  for (WorkerStubsList::iterator it = worker_stubs_.begin();
       it != worker_stubs_.end(); ++it) {
    (*it)->OnChannelError();
  }
}

void WorkerThread::RemoveWorkerStub(WebWorkerStubBase* stub) {
  worker_stubs_.erase(stub);
}

void WorkerThread::AddWorkerStub(WebWorkerStubBase* stub) {
  worker_stubs_.insert(stub);
}

