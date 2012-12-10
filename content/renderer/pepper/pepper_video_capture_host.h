// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_VIDEO_CAPTURE_HOST_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_VIDEO_CAPTURE_HOST_H_

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "content/renderer/pepper/pepper_device_enumeration_host_helper.h"
#include "media/video/capture/video_capture.h"
#include "media/video/capture/video_capture_types.h"
#include "ppapi/c/dev/ppp_video_capture_dev.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/resource_host.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/ppapi/ppb_buffer_impl.h"

namespace content {

class PepperVideoCaptureHost
  : public ppapi::host::ResourceHost,
    public webkit::ppapi::PluginDelegate::PlatformVideoCaptureEventHandler,
    public PepperDeviceEnumerationHostHelper::Delegate {
 public:
  PepperVideoCaptureHost(RendererPpapiHost* host,
                         PP_Instance instance,
                         PP_Resource resource);

  virtual ~PepperVideoCaptureHost();

  bool Init();

  virtual int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) OVERRIDE;

  // PluginDelegate::PlatformVideoCaptureEventHandler
  virtual void OnInitialized(media::VideoCapture* capture,
                             bool succeeded) OVERRIDE;
  virtual void OnStarted(media::VideoCapture* capture) OVERRIDE;
  virtual void OnStopped(media::VideoCapture* capture) OVERRIDE;
  virtual void OnPaused(media::VideoCapture* capture) OVERRIDE;
  virtual void OnError(media::VideoCapture* capture, int error_code) OVERRIDE;
  virtual void OnRemoved(media::VideoCapture* capture) OVERRIDE;
  virtual void OnBufferReady(
      media::VideoCapture* capture,
      scoped_refptr<media::VideoCapture::VideoFrameBuffer> buffer) OVERRIDE;
  virtual void OnDeviceInfoReceived(
      media::VideoCapture* capture,
      const media::VideoCaptureParams& device_info) OVERRIDE;

  // PepperDeviceEnumerationHostHelper::Delegate implementation.
  virtual webkit::ppapi::PluginDelegate* GetPluginDelegate() OVERRIDE;

 private:
  int32_t OnOpen(ppapi::host::HostMessageContext* context,
                 const std::string& device_id,
                 const PP_VideoCaptureDeviceInfo_Dev& requested_info,
                 uint32_t buffer_count);
  int32_t OnStartCapture(ppapi::host::HostMessageContext* context);
  int32_t OnReuseBuffer(ppapi::host::HostMessageContext* context,
                        uint32_t buffer);
  int32_t OnStopCapture(ppapi::host::HostMessageContext* context);
  int32_t OnClose(ppapi::host::HostMessageContext* context);

  int32_t StopCapture();
  int32_t Close();
  void ReleaseBuffers();
  void SendStatus();

  void SetRequestedInfo(const PP_VideoCaptureDeviceInfo_Dev& device_info,
                        uint32_t buffer_count);

  void DetachPlatformVideoCapture();

  bool SetStatus(PP_VideoCaptureStatus_Dev status, bool forced);

  scoped_refptr<webkit::ppapi::PluginDelegate::PlatformVideoCapture>
      platform_video_capture_;

  // Buffers of video frame.
  struct BufferInfo {
    BufferInfo();
    ~BufferInfo();

    bool in_use;
    void* data;
    scoped_refptr<webkit::ppapi::PPB_Buffer_Impl> buffer;
  };

  RendererPpapiHost* renderer_ppapi_host_;

  std::vector<BufferInfo> buffers_;
  size_t buffer_count_hint_;

  media::VideoCaptureCapability capability_;

  PP_VideoCaptureStatus_Dev status_;

  ppapi::host::ReplyMessageContext open_reply_context_;

  PepperDeviceEnumerationHostHelper enumeration_helper_;

  DISALLOW_COPY_AND_ASSIGN(PepperVideoCaptureHost);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_VIDEO_CAPTURE_HOST_H_
