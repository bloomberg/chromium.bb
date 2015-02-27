// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/navigator_connect/navigator_connect_provider.h"

#include "base/lazy_instance.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner_util.h"
#include "content/child/thread_safe_sender.h"
#include "content/child/webmessageportchannel_impl.h"
#include "content/common/navigator_connect_messages.h"
#include "content/public/common/navigator_connect_client.h"
#include "third_party/WebKit/public/platform/WebURL.h"

namespace content {

namespace {

base::LazyInstance<base::ThreadLocalPointer<NavigatorConnectProvider>>::Leaky
    g_provider_tls = LAZY_INSTANCE_INITIALIZER;

NavigatorConnectProvider* const kHasBeenDeleted =
    reinterpret_cast<NavigatorConnectProvider*>(0x1);

int CurrentWorkerId() {
  return WorkerTaskRunner::Instance()->CurrentWorkerId();
}

}  // namespace

NavigatorConnectProvider::NavigatorConnectProvider(
    ThreadSafeSender* thread_safe_sender,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_loop)
    : thread_safe_sender_(thread_safe_sender), main_loop_(main_loop) {
  g_provider_tls.Pointer()->Set(this);
}

NavigatorConnectProvider::~NavigatorConnectProvider() {
  g_provider_tls.Pointer()->Set(kHasBeenDeleted);
}

void NavigatorConnectProvider::connect(
    const blink::WebURL& target_url,
    const blink::WebString& origin,
    blink::WebNavigatorConnectPortCallbacks* callbacks) {
  int request_id = requests_.Add(callbacks);

  thread_safe_sender_->Send(new NavigatorConnectHostMsg_Connect(
      CurrentWorkerId(), request_id,
      NavigatorConnectClient(target_url, GURL(origin), MSG_ROUTING_NONE)));
}

void NavigatorConnectProvider::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(NavigatorConnectProvider, msg)
  IPC_MESSAGE_HANDLER(NavigatorConnectMsg_ConnectResult, OnConnectResult)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled) << "Unhandled message:" << msg.type();
}

NavigatorConnectProvider* NavigatorConnectProvider::ThreadSpecificInstance(
    ThreadSafeSender* thread_safe_sender,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_loop) {
  if (g_provider_tls.Pointer()->Get() == kHasBeenDeleted) {
    NOTREACHED() << "Re-instantiating TLS NavigatorConnectProvider.";
    g_provider_tls.Pointer()->Set(NULL);
  }
  if (g_provider_tls.Pointer()->Get())
    return g_provider_tls.Pointer()->Get();

  NavigatorConnectProvider* provider =
      new NavigatorConnectProvider(thread_safe_sender, main_loop);
  if (WorkerTaskRunner::Instance()->CurrentWorkerId())
    WorkerTaskRunner::Instance()->AddStopObserver(provider);
  return provider;
}

void NavigatorConnectProvider::OnConnectResult(int thread_id,
                                               int request_id,
                                               int message_port_id,
                                               int message_port_route_id,
                                               bool allow_connect) {
  blink::WebNavigatorConnectPortCallbacks* callbacks =
      requests_.Lookup(request_id);
  DCHECK(callbacks);

  if (allow_connect) {
    WebMessagePortChannelImpl* channel = new WebMessagePortChannelImpl(
        message_port_route_id, message_port_id, main_loop_);
    callbacks->onSuccess(channel);
  } else {
    callbacks->onError();
  }
  requests_.Remove(request_id);
}

void NavigatorConnectProvider::OnWorkerRunLoopStopped() {
  delete this;
}

}  // namespace content
