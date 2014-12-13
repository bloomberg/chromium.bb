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
#include "content/common/navigator_connect_types.h"
#include "third_party/WebKit/public/platform/WebCallbacks.h"
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

// Extracts the message_port_id from a message port, and queues messages for the
// port. This is a separate method since while all of this can be done on any
// thread, to get ordering right it is easiest to do both these things on the
// main thread.
int QueueMessagesAndGetMessagePortId(WebMessagePortChannelImpl* webchannel) {
  int message_port_id = webchannel->message_port_id();
  DCHECK(message_port_id != MSG_ROUTING_NONE);
  webchannel->QueueMessages();
  return message_port_id;
}

void SendConnectMessage(const scoped_refptr<ThreadSafeSender>& sender,
                        int thread_id,
                        int request_id,
                        const GURL& target_url,
                        const GURL& origin,
                        int message_port_id) {
  sender->Send(new NavigatorConnectHostMsg_Connect(
      thread_id, request_id,
      CrossOriginServiceWorkerClient(target_url, origin, message_port_id)));
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
    blink::WebMessagePortChannel* port,
    blink::WebCallbacks<void, void>* callbacks) {
  int request_id = requests_.Add(callbacks);

  WebMessagePortChannelImpl* webchannel =
      static_cast<WebMessagePortChannelImpl*>(port);

  if (main_loop_->BelongsToCurrentThread()) {
    SendConnectMessage(thread_safe_sender_, CurrentWorkerId(), request_id,
                       target_url, GURL(origin),
                       QueueMessagesAndGetMessagePortId(webchannel));
  } else {
    // WebMessagePortChannelImpl is mostly thread safe, but is only fully
    // initialized after a round-trip to the main thread. This means that at
    // this point the class might not be fully initialized yet. To work around
    // this, call QueueMessages and extract the message_port_id on the main
    // thread.
    // Furthermore the last reference to WebMessagePortChannelImpl has to be
    // released on the main thread as well. To ensure this happens correctly,
    // it is passed using base::Unretained. Without that the closure could
    // hold the last reference, which would be deleted on the wrong thread.
    PostTaskAndReplyWithResult(
        main_loop_.get(), FROM_HERE,
        base::Bind(&QueueMessagesAndGetMessagePortId,
                   base::Unretained(webchannel)),
        base::Bind(&SendConnectMessage, thread_safe_sender_, CurrentWorkerId(),
                   request_id, target_url, GURL(origin)));
  }
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
                                               bool allow_connect) {
  ConnectCallback* callbacks = requests_.Lookup(request_id);
  DCHECK(callbacks);

  if (allow_connect) {
    callbacks->onSuccess();
  } else {
    callbacks->onError();
  }
  requests_.Remove(request_id);
}

void NavigatorConnectProvider::OnWorkerRunLoopStopped() {
  delete this;
}

}  // namespace content
