// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_in_process_router.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "content/renderer/pepper/renderer_ppapi_host_impl.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_sender.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/resource_tracker.h"

namespace content {

class PepperInProcessRouter::Channel : public IPC::Sender {
 public:
  Channel(const base::Callback<bool(IPC::Message*)>& callback)
      : callback_(callback) {}

  virtual ~Channel() {}

  virtual bool Send(IPC::Message* message) OVERRIDE {
    return callback_.Run(message);
  }

 private:
  base::Callback<bool(IPC::Message*)> callback_;
};

PepperInProcessRouter::PepperInProcessRouter(
    RendererPpapiHostImpl* host_impl)
    : host_impl_(host_impl),
      pending_message_id_(0),
      reply_result_(false),
      weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  dummy_browser_channel_.reset(
      new Channel(base::Bind(&PepperInProcessRouter::DummySendTo,
                             base::Unretained(this))));
  host_to_plugin_router_.reset(
      new Channel(base::Bind(&PepperInProcessRouter::SendToPlugin,
                             base::Unretained(this))));
  plugin_to_host_router_.reset(
      new Channel(base::Bind(&PepperInProcessRouter::SendToHost,
                             base::Unretained(this))));
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

bool PepperInProcessRouter::SendToHost(IPC::Message* msg) {
  scoped_ptr<IPC::Message> message(msg);

  if (!message->is_sync()) {
    bool result = host_impl_->GetPpapiHost()->OnMessageReceived(*message);
    DCHECK(result) << "The message was not handled by the host.";
    return true;
  }

  pending_message_id_ = IPC::SyncMessage::GetMessageId(*message);
  reply_deserializer_.reset(
      static_cast<IPC::SyncMessage*>(message.get())->GetReplyDeserializer());
  reply_result_ = false;

  bool result = host_impl_->GetPpapiHost()->OnMessageReceived(*message);
  DCHECK(result) << "The message was not handled by the host.";

  pending_message_id_ = 0;
  reply_deserializer_.reset(NULL);
  return reply_result_;
}

bool PepperInProcessRouter::SendToPlugin(IPC::Message* msg) {
  scoped_ptr<IPC::Message> message(msg);
  CHECK(!msg->is_sync());
  if (IPC::SyncMessage::IsMessageReplyTo(*message, pending_message_id_)) {
    if (!msg->is_reply_error())
      reply_result_ = reply_deserializer_->SerializeOutputParameters(*message);
  } else {
    CHECK(!pending_message_id_);
    // Dispatch plugin messages from the message loop.
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&PepperInProcessRouter::DispatchPluginMsg,
          weak_factory_.GetWeakPtr(),
          base::Owned(message.release())));
  }
  return true;
}

void PepperInProcessRouter::DispatchPluginMsg(IPC::Message* msg) {
  // Emulate the proxy by dispatching the relevant message here.
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PepperInProcessRouter, *msg)
    IPC_MESSAGE_HANDLER(PpapiPluginMsg_ResourceReply, OnResourceReply)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled) << "The message wasn't handled by the plugin.";
}

bool PepperInProcessRouter::DummySendTo(IPC::Message *msg) {
  NOTREACHED();
  delete msg;
  return false;
}

void PepperInProcessRouter::OnResourceReply(
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

}  // namespace content
