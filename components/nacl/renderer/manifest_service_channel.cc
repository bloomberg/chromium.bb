// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/renderer/manifest_service_channel.h"

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "content/public/renderer/render_thread.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_sync_channel.h"
#include "ppapi/c/pp_errors.h"

namespace nacl {

ManifestServiceChannel::ManifestServiceChannel(
    const IPC::ChannelHandle& handle,
    const base::Callback<void(int32_t)>& connected_callback,
    base::WaitableEvent* waitable_event)
    : connected_callback_(connected_callback),
      channel_(new IPC::SyncChannel(
          handle, IPC::Channel::MODE_CLIENT, this,
          content::RenderThread::Get()->GetIOMessageLoopProxy(),
          true, waitable_event)) {
}

ManifestServiceChannel::~ManifestServiceChannel() {
  if (!connected_callback_.is_null())
    base::ResetAndReturn(&connected_callback_).Run(PP_ERROR_FAILED);
}

bool ManifestServiceChannel::OnMessageReceived(const IPC::Message& message) {
  // TODO(hidehiko): Implement StartCompleted and OpenResource.
  return false;
}

void ManifestServiceChannel::OnChannelConnected(int32 peer_pid) {
  if (!connected_callback_.is_null())
    base::ResetAndReturn(&connected_callback_).Run(PP_OK);
}

void ManifestServiceChannel::OnChannelError() {
  if (!connected_callback_.is_null())
    base::ResetAndReturn(&connected_callback_).Run(PP_ERROR_FAILED);
}

}  // namespace nacl
