// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_media_stream_track_host_base.h"

#include "base/logging.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/media_stream_frame.h"

using ppapi::host::HostMessageContext;
using ppapi::proxy::SerializedHandle;

namespace content {

PepperMediaStreamTrackHostBase::PepperMediaStreamTrackHostBase(
    RendererPpapiHost* host,
    PP_Instance instance,
    PP_Resource resource)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      host_(host),
      frame_buffer_(this) {
}

PepperMediaStreamTrackHostBase::~PepperMediaStreamTrackHostBase() {
}

bool PepperMediaStreamTrackHostBase::InitFrames(int32_t number_of_frames,
                                                int32_t frame_size) {
  DCHECK_GT(number_of_frames, 0);
  DCHECK_GT(frame_size,
            static_cast<int32_t>(sizeof(ppapi::MediaStreamFrame::Header)));
  // Make each frame 4 byte aligned.
  frame_size = (frame_size + 3) & ~0x3;

  int32_t size = number_of_frames * frame_size;
  content::RenderThread* render_thread = content::RenderThread::Get();
  scoped_ptr<base::SharedMemory> shm(
      render_thread->HostAllocateSharedMemoryBuffer(size).Pass());
  if (!shm)
    return false;

  base::SharedMemoryHandle shm_handle = shm->handle();
  if (!frame_buffer_.SetFrames(number_of_frames, frame_size, shm.Pass(), true))
    return false;

  base::PlatformFile platform_file =
#if defined(OS_WIN)
    shm_handle;
#elif defined(OS_POSIX)
    shm_handle.fd;
#else
#error Not implemented.
#endif
  SerializedHandle handle(
      host_->ShareHandleWithRemote(platform_file, false), size);
  host()->SendUnsolicitedReplyWithHandles(pp_resource(),
      PpapiPluginMsg_MediaStreamTrack_InitFrames(number_of_frames, frame_size),
      std::vector<SerializedHandle>(1, handle));
  return true;
}

void PepperMediaStreamTrackHostBase::SendEnqueueFrameMessageToPlugin(
    int32_t index) {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, frame_buffer_.number_of_frames());
  host()->SendUnsolicitedReply(pp_resource(),
      PpapiPluginMsg_MediaStreamTrack_EnqueueFrame(index));
}

int32_t PepperMediaStreamTrackHostBase::OnResourceMessageReceived(
    const IPC::Message& msg,
    HostMessageContext* context) {
  IPC_BEGIN_MESSAGE_MAP(PepperMediaStreamTrackHostBase, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_MediaStreamTrack_EnqueueFrame, OnHostMsgEnqueueFrame)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(
        PpapiHostMsg_MediaStreamTrack_Close, OnHostMsgClose)
  IPC_END_MESSAGE_MAP()
  return ppapi::host::ResourceHost::OnResourceMessageReceived(msg, context);
}

int32_t PepperMediaStreamTrackHostBase::OnHostMsgEnqueueFrame(
    HostMessageContext* context,
    int32_t index) {
  frame_buffer_.EnqueueFrame(index);
  return PP_OK;
}

int32_t PepperMediaStreamTrackHostBase::OnHostMsgClose(
    HostMessageContext* context) {
  OnClose();
  return PP_OK;
}

}  // namespace content
