// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_MEDIA_STREAM_TRACK_HOST_BASE_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_MEDIA_STREAM_TRACK_HOST_BASE_H_

#include "base/compiler_specific.h"
#include "content/common/content_export.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/shared_impl/media_stream_frame_buffer.h"

namespace content {

class RendererPpapiHost;

class PepperMediaStreamTrackHostBase
    : public ppapi::host::ResourceHost,
      public ppapi::MediaStreamFrameBuffer::Delegate {
 protected:
  PepperMediaStreamTrackHostBase(RendererPpapiHost* host,
                                 PP_Instance instance,
                                 PP_Resource resource);
  virtual ~PepperMediaStreamTrackHostBase();

  bool InitFrames(int32_t number_of_frames, int32_t frame_size);

  ppapi::MediaStreamFrameBuffer* frame_buffer() { return &frame_buffer_; }

  // Sends a frame index to the corresponding MediaStreamTrackResourceBase
  // via an IPC message. The resource adds the frame index into its
  // |frame_buffer_| for reading or writing. Also see |MediaStreamFrameBuffer|.
  void SendEnqueueFrameMessageToPlugin(int32_t index);

 private:
  // Subclasses must implement this method to clean up when the track is closed.
  virtual void OnClose() = 0;

  // ResourceMessageHandler overrides:
  virtual int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) OVERRIDE;

  // Message handlers:
  int32_t OnHostMsgEnqueueFrame(ppapi::host::HostMessageContext* context,
                                int32_t index);
  int32_t OnHostMsgClose(ppapi::host::HostMessageContext* context);

  RendererPpapiHost* host_;

  ppapi::MediaStreamFrameBuffer frame_buffer_;

  DISALLOW_COPY_AND_ASSIGN(PepperMediaStreamTrackHostBase);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_MEDIA_STREAM_TRACK_HOST_BASE_H_
