// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/worker/webworkerclient_proxy.h"

#include "base/command_line.h"
#include "chrome/common/child_process.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webmessageportchannel_impl.h"
#include "chrome/common/worker_messages.h"
#include "chrome/renderer/webworker_proxy.h"
#include "chrome/worker/worker_thread.h"
#include "chrome/worker/nativewebworker_impl.h"
#include "ipc/ipc_logging.h"
#include "webkit/api/public/WebString.h"
#include "webkit/api/public/WebURL.h"
#include "webkit/api/public/WebWorker.h"

using WebKit::WebMessagePortChannel;
using WebKit::WebMessagePortChannelArray;
using WebKit::WebString;
using WebKit::WebWorker;
using WebKit::WebWorkerClient;

// How long to wait for worker to finish after it's been told to terminate.
#define kMaxTimeForRunawayWorkerMs 3000

static bool UrlIsNativeWorker(const GURL& url) {
  // If the renderer was not passed the switch to enable native workers,
  // then the URL should be treated as a JavaScript worker.
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
           switches::kEnableNativeWebWorkers)) {
    return false;
  }
  // Based on the suffix, decide whether the url should be considered
  // a NativeWebWorker (for .nexe) or a WebWorker (for anything else).
  const std::string kNativeSuffix(".nexe");
  std::string worker_url = url.path();
  // Compute the start index of the suffix.
  std::string::size_type suffix_index =
      worker_url.length() - kNativeSuffix.length();
  std::string::size_type pos = worker_url.find(kNativeSuffix, suffix_index);
  return (suffix_index == pos);
}

WebWorkerClientProxy::WebWorkerClientProxy(const GURL& url, int route_id)
    : url_(url),
      route_id_(route_id),
      ALLOW_THIS_IN_INITIALIZER_LIST(kill_process_factory_(this)) {
  if (UrlIsNativeWorker(url)) {
    // Launch a native worker.
    impl_ = NativeWebWorkerImpl::create(this);
  } else {
    // Launch a JavaScript worker.
    impl_ = WebWorker::create(this);
  }
  WorkerThread::current()->AddRoute(route_id_, this);
  ChildProcess::current()->AddRefProcess();
}

WebWorkerClientProxy::~WebWorkerClientProxy() {
  impl_->clientDestroyed();
  WorkerThread::current()->RemoveRoute(route_id_);
  ChildProcess::current()->ReleaseProcess();
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
    int destination,
    int source,
    int type,
    int level,
    const WebString& message,
    int line_number,
    const WebString& source_url) {
  WorkerHostMsg_PostConsoleMessageToWorkerObject_Params params;
  params.destination_identifier = destination;
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

void WebWorkerClientProxy::workerContextDestroyed() {
  Send(new WorkerHostMsg_WorkerContextDestroyed(route_id_));

  delete this;
}

WebKit::WebWorker* WebWorkerClientProxy::createWorker(
    WebKit::WebWorkerClient* client) {
  return new WebWorkerProxy(client, WorkerThread::current(), 0);
}

bool WebWorkerClientProxy::Send(IPC::Message* message) {
  return WorkerThread::current()->Send(message);
}

void WebWorkerClientProxy::OnMessageReceived(const IPC::Message& message) {
  if (!impl_)
    return;

  IPC_BEGIN_MESSAGE_MAP(WebWorkerClientProxy, message)
    IPC_MESSAGE_FORWARD(WorkerMsg_StartWorkerContext, impl_,
                        WebWorker::startWorkerContext)
    IPC_MESSAGE_HANDLER(WorkerMsg_TerminateWorkerContext,
                        OnTerminateWorkerContext)
    IPC_MESSAGE_HANDLER(WorkerMsg_PostMessage, OnPostMessage)
    IPC_MESSAGE_FORWARD(WorkerMsg_WorkerObjectDestroyed, impl_,
                        WebWorker::workerObjectDestroyed)
  IPC_END_MESSAGE_MAP()
}

void WebWorkerClientProxy::OnTerminateWorkerContext() {
  impl_->terminateWorkerContext();

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
  // page.
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      kill_process_factory_.NewRunnableMethod(
          &WebWorkerClientProxy::workerContextDestroyed),
          kMaxTimeForRunawayWorkerMs);
}

void WebWorkerClientProxy::OnPostMessage(
    const string16& message,
    const std::vector<int>& sent_message_port_ids,
    const std::vector<int>& new_routing_ids) {
  WebMessagePortChannelArray channels(sent_message_port_ids.size());
  for (size_t i = 0; i < sent_message_port_ids.size(); i++) {
    channels[i] = new WebMessagePortChannelImpl(
        new_routing_ids[i], sent_message_port_ids[i]);
  }

  impl_->postMessageToWorkerContext(message, channels);
}
