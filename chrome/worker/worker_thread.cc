// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/worker/worker_thread.h"

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/threading/thread_local.h"
#include "chrome/common/appcache/appcache_dispatcher.h"
#include "chrome/common/web_database_observer_impl.h"
#include "chrome/worker/webworker_stub.h"
#include "chrome/worker/websharedworker_stub.h"
#include "chrome/worker/worker_webkitclient_impl.h"
#include "content/common/content_switches.h"
#include "content/common/db_message_filter.h"
#include "content/common/worker_messages.h"
#include "ipc/ipc_sync_channel.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebBlobRegistry.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDatabase.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRuntimeFeatures.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebRuntimeFeatures;

static base::LazyInstance<base::ThreadLocalPointer<WorkerThread> > lazy_tls(
    base::LINKER_INITIALIZED);


WorkerThread::WorkerThread() {
  lazy_tls.Pointer()->Set(this);
  webkit_client_.reset(new WorkerWebKitClientImpl);
  WebKit::initialize(webkit_client_.get());

  appcache_dispatcher_.reset(new AppCacheDispatcher(this));

  web_database_observer_impl_.reset(new WebDatabaseObserverImpl(this));
  WebKit::WebDatabase::setObserver(web_database_observer_impl_.get());
  db_message_filter_ = new DBMessageFilter();
  channel()->AddFilter(db_message_filter_.get());

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  webkit_glue::EnableWebCoreLogChannels(
      command_line.GetSwitchValueASCII(switches::kWebCoreLogChannels));

  WebKit::WebRuntimeFeatures::enableDatabase(
      !command_line.HasSwitch(switches::kDisableDatabases));

  WebKit::WebRuntimeFeatures::enableApplicationCache(
      !command_line.HasSwitch(switches::kDisableApplicationCache));

#if defined(OS_WIN)
  // We don't yet support notifications on non-Windows, so hide it from pages.
  WebRuntimeFeatures::enableNotifications(
      !command_line.HasSwitch(switches::kDisableDesktopNotifications));
#endif

  WebRuntimeFeatures::enableSockets(
      !command_line.HasSwitch(switches::kDisableWebSockets));

  WebRuntimeFeatures::enableFileSystem(
      !command_line.HasSwitch(switches::kDisableFileSystem));

  WebRuntimeFeatures::enableWebGL(
      !command_line.HasSwitch(switches::kDisable3DAPIs) &&
      !command_line.HasSwitch(switches::kDisableExperimentalWebGL));
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

bool WorkerThread::OnControlMessageReceived(const IPC::Message& msg) {
  // Appcache messages are handled by a delegate.
  if (appcache_dispatcher_->OnMessageReceived(msg))
    return true;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WorkerThread, msg)
    IPC_MESSAGE_HANDLER(WorkerProcessMsg_CreateWorker, OnCreateWorker)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void WorkerThread::OnCreateWorker(
    const WorkerProcessMsg_CreateWorker_Params& params) {
  WorkerAppCacheInitInfo appcache_init_info(
      params.is_shared, params.creator_process_id,
      params.creator_appcache_host_id,
      params.shared_worker_appcache_id);

  // WebWorkerStub and WebSharedWorkerStub own themselves.
  if (params.is_shared)
    new WebSharedWorkerStub(params.name, params.route_id, appcache_init_info);
  else
    new WebWorkerStub(params.url, params.route_id, appcache_init_info);
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
