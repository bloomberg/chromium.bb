// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/renderer/manifest_service_channel.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "content/public/renderer/render_thread.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_sync_channel.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace nacl {

ManifestServiceChannel::ManifestServiceChannel(
    const IPC::ChannelHandle& handle,
    const base::Callback<void(int32_t)>& connected_callback,
    scoped_ptr<Delegate> delegate,
    base::WaitableEvent* waitable_event)
    : connected_callback_(connected_callback),
      delegate_(delegate.Pass()),
      channel_(IPC::SyncChannel::Create(
          handle,
          IPC::Channel::MODE_CLIENT,
          this,
          content::RenderThread::Get()->GetIOMessageLoopProxy(),
          true,
          waitable_event)),
      weak_ptr_factory_(this) {
}

ManifestServiceChannel::~ManifestServiceChannel() {
  if (!connected_callback_.is_null())
    base::ResetAndReturn(&connected_callback_).Run(PP_ERROR_FAILED);
}

void ManifestServiceChannel::Send(IPC::Message* message) {
  channel_->Send(message);
}

bool ManifestServiceChannel::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ManifestServiceChannel, message)
      IPC_MESSAGE_HANDLER(PpapiHostMsg_StartupInitializationComplete,
                          OnStartupInitializationComplete)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(PpapiHostMsg_OpenResource,
                                      OnOpenResource)
      IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ManifestServiceChannel::OnChannelConnected(int32 peer_pid) {
  if (!connected_callback_.is_null())
    base::ResetAndReturn(&connected_callback_).Run(PP_OK);
}

void ManifestServiceChannel::OnChannelError() {
  if (!connected_callback_.is_null())
    base::ResetAndReturn(&connected_callback_).Run(PP_ERROR_FAILED);
}

void ManifestServiceChannel::OnStartupInitializationComplete() {
  delegate_->StartupInitializationComplete();
}

void ManifestServiceChannel::OnOpenResource(
    const std::string& key, IPC::Message* reply) {
  // Currently this is used only for non-SFI mode, which is not supported on
  // windows.
#if !defined(OS_WIN)
  delegate_->OpenResource(
      key,
      base::Bind(&ManifestServiceChannel::DidOpenResource,
                 weak_ptr_factory_.GetWeakPtr(), reply));
#else
  PpapiHostMsg_OpenResource::WriteReplyParams(
      reply, ppapi::proxy::SerializedHandle());
  Send(reply);
#endif
}

#if !defined(OS_WIN)
void ManifestServiceChannel::DidOpenResource(
    IPC::Message* reply, base::File file) {
  // Here, PlatformFileForTransit is alias of base::FileDescriptor.
  PpapiHostMsg_OpenResource::WriteReplyParams(
      reply,
      ppapi::proxy::SerializedHandle(
          ppapi::proxy::SerializedHandle::FILE,
          base::FileDescriptor(file.Pass())));
  Send(reply);
}
#endif

}  // namespace nacl
