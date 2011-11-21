// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/worker/webworkerclient_proxy.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "content/common/file_system/file_system_dispatcher.h"
#include "content/common/file_system/webfilesystem_callback_dispatcher.h"
#include "content/common/webmessageportchannel_impl.h"
#include "content/common/worker_messages.h"
#include "content/public/common/content_switches.h"
// TODO(jam): uncomment this and WebWorkerClientProxy::createWorker when the
// renderer worker code moves to content. This code isn't used now since we
// don't support nested workers anyways.
//#include "content/renderer/webworker_proxy.h"
#include "content/worker/shared_worker_devtools_agent.h"
#include "content/worker/websharedworker_stub.h"
#include "content/worker/worker_thread.h"
#include "content/worker/worker_webapplicationcachehost_impl.h"
#include "ipc/ipc_logging.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFileSystemCallbacks.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebWorker.h"

using WebKit::WebApplicationCacheHost;
using WebKit::WebFrame;
using WebKit::WebMessagePortChannel;
using WebKit::WebMessagePortChannelArray;
using WebKit::WebSecurityOrigin;
using WebKit::WebString;
using WebKit::WebWorker;
using WebKit::WebWorkerClient;

// How long to wait for worker to finish after it's been told to terminate.
#define kMaxTimeForRunawayWorkerMs 3000

WebWorkerClientProxy::WebWorkerClientProxy(int route_id,
                                           WebSharedWorkerStub* stub)
    : route_id_(route_id),
      appcache_host_id_(0),
      stub_(stub),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      devtools_agent_(NULL) {
}

WebWorkerClientProxy::~WebWorkerClientProxy() {
}

void WebWorkerClientProxy::postMessageToWorkerObject(
    const WebString& message,
    const WebMessagePortChannelArray& channels) {
  std::vector<int> message_port_ids(channels.size());
  std::vector<int> routing_ids(channels.size());
  for (size_t i = 0; i < channels.size(); ++i) {
    WebMessagePortChannelImpl* webchannel =
        static_cast<WebMessagePortChannelImpl*>(channels[i]);
    message_port_ids[i] = webchannel->message_port_id();
    webchannel->QueueMessages();
    DCHECK(message_port_ids[i] != MSG_ROUTING_NONE);
    routing_ids[i] = MSG_ROUTING_NONE;
  }

  Send(new WorkerMsg_PostMessage(
      route_id_, message, message_port_ids, routing_ids));
}

void WebWorkerClientProxy::postExceptionToWorkerObject(
    const WebString& error_message,
    int line_number,
    const WebString& source_url) {
  Send(new WorkerHostMsg_PostExceptionToWorkerObject(
      route_id_, error_message, line_number, source_url));
}

void WebWorkerClientProxy::postConsoleMessageToWorkerObject(
    int source,
    int type,
    int level,
    const WebString& message,
    int line_number,
    const WebString& source_url) {
  WorkerHostMsg_PostConsoleMessageToWorkerObject_Params params;
  params.source_identifier = source;
  params.message_type = type;
  params.message_level = level;
  params.message = message;
  params.line_number = line_number;
  params.source_url = source_url;
  Send(new WorkerHostMsg_PostConsoleMessageToWorkerObject(route_id_, params));
}

void WebWorkerClientProxy::confirmMessageFromWorkerObject(
    bool has_pending_activity) {
  Send(new WorkerHostMsg_ConfirmMessageFromWorkerObject(
      route_id_, has_pending_activity));
}

void WebWorkerClientProxy::reportPendingActivity(bool has_pending_activity) {
  Send(new WorkerHostMsg_ReportPendingActivity(
      route_id_, has_pending_activity));
}

void WebWorkerClientProxy::workerContextClosed() {
  Send(new WorkerHostMsg_WorkerContextClosed(route_id_));
}

void WebWorkerClientProxy::workerContextDestroyed() {
  Send(new WorkerHostMsg_WorkerContextDestroyed(route_id_));
  // Tell the stub that the worker has shutdown - frees this object.
  if (stub_)
    stub_->Shutdown();
}

WebKit::WebWorker* WebWorkerClientProxy::createWorker(
    WebKit::WebWorkerClient* client) {
  // TODO(jam): see comment at top of file
  //return new WebWorkerProxy(client, WorkerThread::current(),
  //                          0, appcache_host_id_);
  return NULL;
}

WebKit::WebNotificationPresenter*
WebWorkerClientProxy::notificationPresenter() {
  // TODO(johnnyg): Notifications are not yet hooked up to workers.
  // Coming soon.
  NOTREACHED();
  return NULL;
}

WebApplicationCacheHost* WebWorkerClientProxy::createApplicationCacheHost(
    WebKit::WebApplicationCacheHostClient* client) {
  WorkerWebApplicationCacheHostImpl* host =
      new WorkerWebApplicationCacheHostImpl(stub_->appcache_init_info(),
                                            client);
  // Remember the id of the instance we create so we have access to that
  // value when creating nested dedicated workers in createWorker.
  appcache_host_id_ = host->host_id();
  return host;
}

// TODO(abarth): Security checks should use WebDocument or WebSecurityOrigin,
// not WebFrame as the context object because WebFrames can contain different
// WebDocuments at different times.
bool WebWorkerClientProxy::allowDatabase(WebFrame* frame,
                                         const WebString& name,
                                         const WebString& display_name,
                                         unsigned long estimated_size) {
  WebSecurityOrigin origin = frame->document().securityOrigin();
  if (origin.isUnique())
    return false;

  bool result = false;
  Send(new WorkerProcessHostMsg_AllowDatabase(
      route_id_, GURL(origin.toString().utf8()), name, display_name,
      estimated_size, &result));
  return result;
}

bool WebWorkerClientProxy::allowFileSystem() {
  bool result = false;
  Send(new WorkerProcessHostMsg_AllowFileSystem(
      route_id_, stub_->url().GetOrigin(), &result));
  return result;
}

void WebWorkerClientProxy::openFileSystem(
    WebKit::WebFileSystem::Type type,
    long long size,
    bool create,
    WebKit::WebFileSystemCallbacks* callbacks) {
  ChildThread::current()->file_system_dispatcher()->OpenFileSystem(
      stub_->url().GetOrigin(), static_cast<fileapi::FileSystemType>(type),
      size, create, new WebFileSystemCallbackDispatcher(callbacks));
}

void WebWorkerClientProxy::dispatchDevToolsMessage(const WebString& message) {
  if (devtools_agent_)
    devtools_agent_->SendDevToolsMessage(message);
}

void WebWorkerClientProxy::saveDevToolsAgentState(
    const WebKit::WebString& state) {
  if (devtools_agent_)
    devtools_agent_->SaveDevToolsAgentState(state);
}

bool WebWorkerClientProxy::Send(IPC::Message* message) {
  return WorkerThread::current()->Send(message);
}

void WebWorkerClientProxy::EnsureWorkerContextTerminates() {
  // Avoid a worker doing a while(1) from never exiting.
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWebWorkerShareProcesses)) {
    // Can't kill the process since there could be workers from other
    // renderer process.
    NOTIMPLEMENTED();
    return;
  }

  // This shuts down the process cleanly from the perspective of the browser
  // process, and avoids the crashed worker infobar from appearing to the new
  // page. It's ok to post several of theese, because the first executed task
  // will exit the message loop and subsequent ones won't be executed.
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      base::Bind(
          &WebWorkerClientProxy::workerContextDestroyed,
          weak_factory_.GetWeakPtr()),
      kMaxTimeForRunawayWorkerMs);
}
