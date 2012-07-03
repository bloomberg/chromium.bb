// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_in_process_resource_creation.h"

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "content/renderer/pepper/content_renderer_pepper_host_factory.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/resource_tracker.h"

// Note that the code in the creation functions in this file should generally
// be the same as that in ppapi/proxy/resource_creation_proxy.cc. See
// pepper_in_process_resource_creation.h for what this file is for.

namespace content {

class PepperInProcessResourceCreation::PluginToHostRouter
    : public IPC::Sender {
 public:
  PluginToHostRouter(RenderViewImpl* render_view,
                     IPC::Sender* host_to_plugin_sender);
  virtual ~PluginToHostRouter() {}

  // Sender implementation.
  virtual bool Send(IPC::Message* msg) OVERRIDE;

 private:
  void DoSend(IPC::Message* msg);

  base::WeakPtrFactory<PluginToHostRouter> weak_factory_;

  ContentRendererPepperHostFactory factory_;
  ppapi::host::PpapiHost host_;

  DISALLOW_COPY_AND_ASSIGN(PluginToHostRouter);
};

PepperInProcessResourceCreation::PluginToHostRouter::PluginToHostRouter(
    RenderViewImpl* render_view,
    IPC::Sender* host_to_plugin_sender)
    : weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      factory_(render_view),
      host_(host_to_plugin_sender, &factory_) {
}

bool PepperInProcessResourceCreation::PluginToHostRouter::Send(
    IPC::Message* msg) {
  // Don't directly call into the message handler to avoid reentrancy. The IPC
  // systen assumes everything is executed from the message loop, so emulate
  // the same thing for in-process.
  MessageLoop::current()->PostTask(FROM_HERE,
      base::Bind(&PluginToHostRouter::DoSend, weak_factory_.GetWeakPtr(),
                 base::Owned(msg)));
  return true;
}

void PepperInProcessResourceCreation::PluginToHostRouter::DoSend(
    IPC::Message* msg) {
  host_.OnMessageReceived(*msg);
}

// HostToPluginRouter ---------------------------------------------------------

class PepperInProcessResourceCreation::HostToPluginRouter
    : public IPC::Sender {
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

PepperInProcessResourceCreation::HostToPluginRouter::HostToPluginRouter()
    : weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

bool PepperInProcessResourceCreation::HostToPluginRouter::Send(
    IPC::Message* msg) {
  // As in the PluginToHostRouter, dispatch from the message loop.
  MessageLoop::current()->PostTask(FROM_HERE,
      base::Bind(&HostToPluginRouter::DispatchMsg,
                 weak_factory_.GetWeakPtr(),
                 base::Owned(msg)));
  return true;
}

void PepperInProcessResourceCreation::HostToPluginRouter::DispatchMsg(
    IPC::Message* msg) {
  // Emulate the proxy by dispatching the relevant message here.
  IPC_BEGIN_MESSAGE_MAP(HostToPluginRouter, *msg)
    IPC_MESSAGE_HANDLER(PpapiPluginMsg_ResourceReply, OnMsgResourceReply)
  IPC_END_MESSAGE_MAP()
}

void PepperInProcessResourceCreation::HostToPluginRouter::OnMsgResourceReply(
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
  resource->OnReplyReceived(reply_params.sequence(), reply_params.result(),
                            nested_msg);
}

// PepperInProcessResourceCreation --------------------------------------------

PepperInProcessResourceCreation::PepperInProcessResourceCreation(
    RenderViewImpl* render_view,
    webkit::ppapi::PluginInstance* instance)
    : ResourceCreationImpl(instance),
      host_to_plugin_router_(new HostToPluginRouter),
      plugin_to_host_router_(
          new PluginToHostRouter(render_view, host_to_plugin_router_.get())) {
}

PepperInProcessResourceCreation::~PepperInProcessResourceCreation() {
}

}  // namespace content
