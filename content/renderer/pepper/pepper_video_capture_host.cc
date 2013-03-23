// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_video_capture_host.h"

#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/host_resource.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_buffer_api.h"
#include "webkit/plugins/ppapi/host_globals.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"

using ppapi::HostResource;
using ppapi::TrackedCallback;
using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_Buffer_API;
using webkit::ppapi::HostGlobals;
using webkit::ppapi::PPB_Buffer_Impl;

namespace {

// Maximum number of buffers to actually allocate.
const uint32_t kMaxBuffers = 20;

}  // namespace

namespace content {

PepperVideoCaptureHost::PepperVideoCaptureHost(RendererPpapiHost* host,
                                               PP_Instance instance,
                                               PP_Resource resource)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      renderer_ppapi_host_(host),
      buffer_count_hint_(0),
      status_(PP_VIDEO_CAPTURE_STATUS_STOPPED),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          enumeration_helper_(this, this, PP_DEVICETYPE_DEV_VIDEOCAPTURE)) {
}

PepperVideoCaptureHost::~PepperVideoCaptureHost() {
  Close();
}

bool PepperVideoCaptureHost::Init() {
  return !!GetPluginDelegate();
}

int32_t PepperVideoCaptureHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  int32_t result = PP_ERROR_FAILED;
  if (enumeration_helper_.HandleResourceMessage(msg, context, &result))
    return result;

  IPC_BEGIN_MESSAGE_MAP(PepperVideoCaptureHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_VideoCapture_Open,
        OnOpen)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(
        PpapiHostMsg_VideoCapture_StartCapture,
        OnStartCapture)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_VideoCapture_ReuseBuffer,
        OnReuseBuffer)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(
        PpapiHostMsg_VideoCapture_StopCapture,
        OnStopCapture)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(
        PpapiHostMsg_VideoCapture_Close,
        OnClose)
  IPC_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

void PepperVideoCaptureHost::OnInitialized(media::VideoCapture* capture,
                                           bool succeeded) {
  DCHECK(capture == platform_video_capture_.get());

  if (succeeded) {
    open_reply_context_.params.set_result(PP_OK);
  } else {
    DetachPlatformVideoCapture();
    open_reply_context_.params.set_result(PP_ERROR_FAILED);
  }

  host()->SendReply(open_reply_context_,
                    PpapiPluginMsg_VideoCapture_OpenReply());
}

void PepperVideoCaptureHost::OnStarted(media::VideoCapture* capture) {
  if (SetStatus(PP_VIDEO_CAPTURE_STATUS_STARTED, false))
    SendStatus();
}

void PepperVideoCaptureHost::OnStopped(media::VideoCapture* capture) {
  if (SetStatus(PP_VIDEO_CAPTURE_STATUS_STOPPED, false))
    SendStatus();
}

void PepperVideoCaptureHost::OnPaused(media::VideoCapture* capture) {
  if (SetStatus(PP_VIDEO_CAPTURE_STATUS_PAUSED, false))
    SendStatus();
}

void PepperVideoCaptureHost::OnError(media::VideoCapture* capture,
                                     int error_code) {
  // Today, the media layer only sends "1" as an error.
  DCHECK(error_code == 1);
  // It either comes because some error was detected while starting (e.g. 2
  // conflicting "master" resolution), or because the browser failed to start
  // the capture.
  SetStatus(PP_VIDEO_CAPTURE_STATUS_STOPPED, true);
  host()->SendUnsolicitedReply(pp_resource(),
      PpapiPluginMsg_VideoCapture_OnError(PP_ERROR_FAILED));
}

void PepperVideoCaptureHost::OnRemoved(media::VideoCapture* capture) {
}

void PepperVideoCaptureHost::OnBufferReady(
    media::VideoCapture* capture,
    scoped_refptr<media::VideoCapture::VideoFrameBuffer> buffer) {
  DCHECK(buffer.get());
  for (uint32_t i = 0; i < buffers_.size(); ++i) {
    if (!buffers_[i].in_use) {
      // TODO(ihf): Switch to a size calculation based on stride.
      // Stride is filled out now but not more meaningful than size
      // until wjia unifies VideoFrameBuffer and media::VideoFrame.
      size_t size = std::min(static_cast<size_t>(buffers_[i].buffer->size()),
          buffer->buffer_size);
      memcpy(buffers_[i].data, buffer->memory_pointer, size);
      buffers_[i].in_use = true;
      platform_video_capture_->FeedBuffer(buffer);
      host()->SendUnsolicitedReply(pp_resource(),
          PpapiPluginMsg_VideoCapture_OnBufferReady(i));
      return;
    }
  }

  // No free slot, just discard the frame and tell the media layer it can
  // re-use the buffer.
  platform_video_capture_->FeedBuffer(buffer);
}

void PepperVideoCaptureHost::OnDeviceInfoReceived(
    media::VideoCapture* capture,
    const media::VideoCaptureParams& device_info) {
  PP_VideoCaptureDeviceInfo_Dev info = {
    static_cast<uint32_t>(device_info.width),
    static_cast<uint32_t>(device_info.height),
    static_cast<uint32_t>(device_info.frame_per_second)
  };
  ReleaseBuffers();

  // YUV 4:2:0
  int uv_width = info.width / 2;
  int uv_height = info.height / 2;
  size_t size = info.width * info.height + 2 * uv_width * uv_height;

  ppapi::proxy::ResourceMessageReplyParams params(pp_resource(), 0);

  // Allocate buffers. We keep a reference to them, that is released in
  // ReleaseBuffers. In the mean time, we prepare the resource and handle here
  // for sending below.
  std::vector<HostResource> buffer_host_resources;
  buffers_.reserve(buffer_count_hint_);
  ::ppapi::ResourceTracker* tracker =
      HostGlobals::Get()->GetResourceTracker();
  ppapi::proxy::HostDispatcher* dispatcher =
      ppapi::proxy::HostDispatcher::GetForInstance(pp_instance());
  for (size_t i = 0; i < buffer_count_hint_; ++i) {
    PP_Resource res = PPB_Buffer_Impl::Create(pp_instance(), size);
    if (!res)
      break;

    EnterResourceNoLock<PPB_Buffer_API> enter(res, true);
    DCHECK(enter.succeeded());

    BufferInfo buf;
    buf.buffer = static_cast<PPB_Buffer_Impl*>(enter.object());
    buf.data = buf.buffer->Map();
    if (!buf.data) {
      tracker->ReleaseResource(res);
      break;
    }
    buffers_.push_back(buf);

    // Add to HostResource array to be sent.
    {
      HostResource host_resource;
      host_resource.SetHostResource(pp_instance(), res);
      buffer_host_resources.push_back(host_resource);

      // Add a reference for the plugin, which is resposible for releasing it.
      tracker->AddRefResource(res);
    }

    // Add the serialized shared memory handle to params. FileDescriptor is
    // treated in special case.
    {
      EnterResourceNoLock<PPB_Buffer_API> enter(res, true);
      DCHECK(enter.succeeded());
      int handle;
      int32_t result = enter.object()->GetSharedMemory(&handle);
      DCHECK(result == PP_OK);
      // TODO(piman/brettw): Change trusted interface to return a PP_FileHandle,
      // those casts are ugly.
      base::PlatformFile platform_file =
#if defined(OS_WIN)
          reinterpret_cast<HANDLE>(static_cast<intptr_t>(handle));
#elif defined(OS_POSIX)
          handle;
#else
#error Not implemented.
#endif
      params.AppendHandle(
          ppapi::proxy::SerializedHandle(
              dispatcher->ShareHandleWithRemote(platform_file, false),
              size));
    }
  }

  if (buffers_.empty()) {
    // We couldn't allocate/map buffers at all. Send an error and stop the
    // capture.
    SetStatus(PP_VIDEO_CAPTURE_STATUS_STOPPING, true);
    platform_video_capture_->StopCapture(this);
    OnError(capture, PP_ERROR_NOMEMORY);
    return;
  }

  host()->Send(new PpapiPluginMsg_ResourceReply(
      params, PpapiPluginMsg_VideoCapture_OnDeviceInfo(
          info, buffer_host_resources, size)));
}

webkit::ppapi::PluginDelegate* PepperVideoCaptureHost::GetPluginDelegate() {
  webkit::ppapi::PluginInstance* instance =
      renderer_ppapi_host_->GetPluginInstance(pp_instance());
  if (instance)
    return instance->delegate();
  return NULL;
}

int32_t PepperVideoCaptureHost::OnOpen(
    ppapi::host::HostMessageContext* context,
    const std::string& device_id,
    const PP_VideoCaptureDeviceInfo_Dev& requested_info,
    uint32_t buffer_count) {
  if (platform_video_capture_.get())
    return PP_ERROR_FAILED;

  webkit::ppapi::PluginDelegate* plugin_delegate = GetPluginDelegate();
  if (!plugin_delegate)
    return PP_ERROR_FAILED;

  SetRequestedInfo(requested_info, buffer_count);

  platform_video_capture_ =
      plugin_delegate->CreateVideoCapture(device_id, this);

  open_reply_context_ = context->MakeReplyMessageContext();

  // It is able to complete synchronously if the default device is used.
  bool sync_completion = device_id.empty();
  if (sync_completion) {
    // Send OpenACK directly, but still need to return PP_OK_COMPLETIONPENDING
    // to make PluginResource happy.
    OnInitialized(platform_video_capture_.get(), true);
  }

  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperVideoCaptureHost::OnStartCapture(
    ppapi::host::HostMessageContext* context) {
  if (!SetStatus(PP_VIDEO_CAPTURE_STATUS_STARTING, false) ||
      !platform_video_capture_.get())
    return PP_ERROR_FAILED;

  DCHECK(buffers_.empty());

  // It's safe to call this regardless it's capturing or not, because
  // PepperPlatformVideoCaptureImpl maintains the state.
  platform_video_capture_->StartCapture(this, capability_);
  return PP_OK;
}

int32_t PepperVideoCaptureHost::OnReuseBuffer(
    ppapi::host::HostMessageContext* context,
    uint32_t buffer) {
  if (buffer >= buffers_.size() || !buffers_[buffer].in_use)
    return PP_ERROR_BADARGUMENT;
  buffers_[buffer].in_use = false;
  return PP_OK;
}

int32_t PepperVideoCaptureHost::OnStopCapture(
    ppapi::host::HostMessageContext* context) {
  return StopCapture();
}

int32_t PepperVideoCaptureHost::OnClose(
    ppapi::host::HostMessageContext* context) {
  return Close();
}

int32_t PepperVideoCaptureHost::StopCapture() {
  if (!SetStatus(PP_VIDEO_CAPTURE_STATUS_STOPPING, false))
    return PP_ERROR_FAILED;

  DCHECK(platform_video_capture_.get());

  ReleaseBuffers();
  // It's safe to call this regardless it's capturing or not, because
  // PepperPlatformVideoCaptureImpl maintains the state.
  platform_video_capture_->StopCapture(this);
  return PP_OK;
}

int32_t PepperVideoCaptureHost::Close() {
  if (!platform_video_capture_.get())
    return PP_OK;

  StopCapture();
  DCHECK(buffers_.empty());
  DetachPlatformVideoCapture();
  return PP_OK;
}

void PepperVideoCaptureHost::ReleaseBuffers() {
  ::ppapi::ResourceTracker* tracker = HostGlobals::Get()->GetResourceTracker();
  for (size_t i = 0; i < buffers_.size(); ++i) {
    buffers_[i].buffer->Unmap();
    tracker->ReleaseResource(buffers_[i].buffer->pp_resource());
  }
  buffers_.clear();
}

void PepperVideoCaptureHost::SendStatus() {
  host()->SendUnsolicitedReply(pp_resource(),
      PpapiPluginMsg_VideoCapture_OnStatus(status_));
}

void PepperVideoCaptureHost::SetRequestedInfo(
    const PP_VideoCaptureDeviceInfo_Dev& device_info,
    uint32_t buffer_count) {
  // Clamp the buffer count to between 1 and |kMaxBuffers|.
  buffer_count_hint_ = std::min(std::max(buffer_count, 1U), kMaxBuffers);

  capability_.width = device_info.width;
  capability_.height = device_info.height;
  capability_.frame_rate = device_info.frames_per_second;
  capability_.expected_capture_delay = 0;  // Ignored.
  capability_.color = media::VideoCaptureCapability::kI420;
  capability_.interlaced = false;  // Ignored.
}

void PepperVideoCaptureHost::DetachPlatformVideoCapture() {
  if (platform_video_capture_.get()) {
    platform_video_capture_->DetachEventHandler();
    platform_video_capture_ = NULL;
  }
}

bool PepperVideoCaptureHost::SetStatus(PP_VideoCaptureStatus_Dev status,
                                       bool forced) {
  if (!forced) {
    switch (status) {
      case PP_VIDEO_CAPTURE_STATUS_STOPPED:
        if (status_ != PP_VIDEO_CAPTURE_STATUS_STOPPING)
          return false;
        break;
      case PP_VIDEO_CAPTURE_STATUS_STARTING:
        if (status_ != PP_VIDEO_CAPTURE_STATUS_STOPPED)
          return false;
        break;
      case PP_VIDEO_CAPTURE_STATUS_STARTED:
        switch (status_) {
          case PP_VIDEO_CAPTURE_STATUS_STARTING:
          case PP_VIDEO_CAPTURE_STATUS_PAUSED:
            break;
          default:
            return false;
        }
        break;
      case PP_VIDEO_CAPTURE_STATUS_PAUSED:
        switch (status_) {
          case PP_VIDEO_CAPTURE_STATUS_STARTING:
          case PP_VIDEO_CAPTURE_STATUS_STARTED:
            break;
          default:
            return false;
        }
        break;
      case PP_VIDEO_CAPTURE_STATUS_STOPPING:
        switch (status_) {
          case PP_VIDEO_CAPTURE_STATUS_STARTING:
          case PP_VIDEO_CAPTURE_STATUS_STARTED:
          case PP_VIDEO_CAPTURE_STATUS_PAUSED:
            break;
          default:
            return false;
        }
        break;
    }
  }

  status_ = status;
  return true;
}

PepperVideoCaptureHost::BufferInfo::BufferInfo()
    : in_use(false),
      data(NULL),
      buffer() {
}

PepperVideoCaptureHost::BufferInfo::~BufferInfo() {
}

}  // namespace content
