// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_in_process_router.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "content/renderer/pepper/renderer_ppapi_host_impl.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_sender.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/resource_tracker.h"

namespace content {

class PepperInProcessRouter::DummyBrowserChannel : public IPC::Sender {
 public:
  DummyBrowserChannel() {}
  ~DummyBrowserChannel() {}

  // Sender implementation.
  virtual bool Send(IPC::Message* msg) OVERRIDE {
    // See the class comment in the header file for what this is about.
    NOTREACHED();
    delete msg;
    return false;
  }
};

class PepperInProcessRouter::PluginToHostRouter : public IPC::Sender {
 public:
  PluginToHostRouter(RendererPpapiHostImpl* host_impl);
  virtual ~PluginToHostRouter() {}

  // Sender implementation.
  virtual bool Send(IPC::Message* msg) OVERRIDE;

 private:
  void DoSend(IPC::Message* msg);

  RendererPpapiHostImpl* host_impl_;

  base::WeakPtrFactory<PluginToHostRouter> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PluginToHostRouter);
};

PepperInProcessRouter::PluginToHostRouter::PluginToHostRouter(
    RendererPpapiHostImpl* host_impl)
    : host_impl_(host_impl),
      weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

bool PepperInProcessRouter::PluginToHostRouter::Send(
    IPC::Message* msg) {
  // Don't directly call into the message handler to avoid reentrancy. The IPC
  // systen assumes everything is executed from the message loop, so emulate
  // the same thing for in-process.
  MessageLoop::current()->PostTask(FROM_HERE,
      base::Bind(&PluginToHostRouter::DoSend, weak_factory_.GetWeakPtr(),
                 base::Owned(msg)));
  return true;
}

void PepperInProcessRouter::PluginToHostRouter::DoSend(
    IPC::Message* msg) {
  host_impl_->GetPpapiHost()->OnMessageReceived(*msg);
}

// HostToPluginRouter ---------------------------------------------------------

class PepperInProcessRouter::HostToPluginRouter : public IPC::Sender {
 public:
  HostToPluginRouter();
  virtual ~HostToPluginRouter() {}

  // Sender implementation.
  virtual bool Send(IPC::Message* msg) OVERRIDE;

 private:
  void DispatchMsg(IPC::Message* msg);

  void OnMsgResourceReply(
      const ppapi::proxy::ResourceMessageReplyParams& reply_params,
      const IPC::Message& nested_msg);

  base::WeakPtrFactory<HostToPluginRouter> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(HostToPluginRouter);
};

PepperInProcessRouter::HostToPluginRouter::HostToPluginRouter()
    : weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

bool PepperInProcessRouter::HostToPluginRouter::Send(
    IPC::Message* msg) {
  // As in the PluginToHostRouter, dispatch from the message loop.
  MessageLoop::current()->PostTask(FROM_HERE,
      base::Bind(&HostToPluginRouter::DispatchMsg,
                 weak_factory_.GetWeakPtr(),
                 base::Owned(msg)));
  return true;
}

void PepperInProcessRouter::HostToPluginRouter::DispatchMsg(
    IPC::Message* msg) {
  // Emulate the proxy by dispatching the relevant message here.
  IPC_BEGIN_MESSAGE_MAP(HostToPluginRouter, *msg)
    IPC_MESSAGE_HANDLER(PpapiPluginMsg_ResourceReply, OnMsgResourceReply)
  IPC_END_MESSAGE_MAP()
}

void PepperInProcessRouter::HostToPluginRouter::OnMsgResourceReply(
    const ppapi::proxy::ResourceMessageReplyParams& reply_params,
    const IPC::Message& nested_msg) {
  ppapi::Resource* resource =
      ppapi::PpapiGlobals::Get()->GetResourceTracker()->GetResource(
          reply_params.pp_resource());
  if (!resource) {
    // The resource could have been destroyed while the async processing was
    // pending. Just drop the message.
    return;
  }
  resource->OnReplyReceived(reply_params, nested_msg);
}

PepperInProcessRouter::PepperInProcessRouter(
    RendererPpapiHostImpl* host_impl)
    : host_to_plugin_router_(new HostToPluginRouter),
      plugin_to_host_router_(new PluginToHostRouter(host_impl)) {
}

PepperInProcessRouter::~PepperInProcessRouter() {
}

IPC::Sender* PepperInProcessRouter::GetPluginToRendererSender() {
  return plugin_to_host_router_.get();
}

IPC::Sender* PepperInProcessRouter::GetRendererToPluginSender() {
  return host_to_plugin_router_.get();
}

ppapi::proxy::Connection PepperInProcessRouter::GetPluginConnection() {
  return ppapi::proxy::Connection(dummy_browser_channel_.get(),
                                  plugin_to_host_router_.get());
}

}  // namespace content
