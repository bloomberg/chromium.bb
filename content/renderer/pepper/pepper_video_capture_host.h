// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_VIDEO_CAPTURE_HOST_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_VIDEO_CAPTURE_HOST_H_

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "content/renderer/pepper/pepper_device_enumeration_host_helper.h"
#include "content/renderer/pepper/ppb_buffer_impl.h"
#include "media/video/capture/video_capture.h"
#include "media/video/capture/video_capture_types.h"
#include "ppapi/c/dev/ppp_video_capture_dev.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/resource_host.h"

namespace content {
class PepperPlatformVideoCapture;
class RendererPpapiHostImpl;

class PepperVideoCaptureHost : public ppapi::host::ResourceHost,
                               public media::VideoCapture::EventHandler {
 public:
  PepperVideoCaptureHost(RendererPpapiHostImpl* host,
                         PP_Instance instance,
                         PP_Resource resource);

  virtual ~PepperVideoCaptureHost();

  bool Init();

  virtual int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) OVERRIDE;

  void OnInitialized(media::VideoCapture* capture, bool succeeded);

  // media::VideoCapture::EventHandler
  virtual void OnStarted(media::VideoCapture* capture) OVERRIDE;
  virtual void OnStopped(media::VideoCapture* capture) OVERRIDE;
  virtual void OnPaused(media::VideoCapture* capture) OVERRIDE;
  virtual void OnError(media::VideoCapture* capture, int error_code) OVERRIDE;
  virtual void OnRemoved(media::VideoCapture* capture) OVERRIDE;
  virtual void OnFrameReady(media::VideoCapture* capture,
                            const scoped_refptr<media::VideoFrame>& frame)
      OVERRIDE;

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
  void PostErrorReply();
  void AllocBuffers(const gfx::Size& resolution, int frame_rate);
  void ReleaseBuffers();
  void SendStatus();

  void SetRequestedInfo(const PP_VideoCaptureDeviceInfo_Dev& device_info,
                        uint32_t buffer_count);

  void DetachPlatformVideoCapture();

  bool SetStatus(PP_VideoCaptureStatus_Dev status, bool forced);

  scoped_refptr<PepperPlatformVideoCapture> platform_video_capture_;

  // Buffers of video frame.
  struct BufferInfo {
    BufferInfo();
    ~BufferInfo();

    bool in_use;
    void* data;
    scoped_refptr<PPB_Buffer_Impl> buffer;
  };

  RendererPpapiHostImpl* renderer_ppapi_host_;

  gfx::Size alloc_size_;
  std::vector<BufferInfo> buffers_;
  size_t buffer_count_hint_;

  media::VideoCaptureParams video_capture_params_;

  PP_VideoCaptureStatus_Dev status_;

  ppapi::host::ReplyMessageContext open_reply_context_;

  PepperDeviceEnumerationHostHelper enumeration_helper_;

  DISALLOW_COPY_AND_ASSIGN(PepperVideoCaptureHost);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_VIDEO_CAPTURE_HOST_H_
