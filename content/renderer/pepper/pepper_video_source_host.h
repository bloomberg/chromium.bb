// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_VIDEO_SOURCE_HOST_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_VIDEO_SOURCE_HOST_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/renderer/media/video_source_handler.h"
#include "ppapi/c/pp_time.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/resource_host.h"

namespace content {

class RendererPpapiHost;

class CONTENT_EXPORT PepperVideoSourceHost
    : public ppapi::host::ResourceHost,
      public content::FrameReaderInterface {
 public:
  PepperVideoSourceHost(RendererPpapiHost* host,
                        PP_Instance instance,
                        PP_Resource resource);

  virtual ~PepperVideoSourceHost();

  // content::FrameReaderInterface implementation.
  virtual bool GotFrame(cricket::VideoFrame* frame) OVERRIDE;

  virtual int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) OVERRIDE;

 private:
  int32_t OnHostMsgOpen(ppapi::host::HostMessageContext* context,
                        const std::string& stream_url);
  int32_t OnHostMsgGetFrame(ppapi::host::HostMessageContext* context);
  int32_t OnHostMsgClose(ppapi::host::HostMessageContext* context);

  void OnGotFrame(scoped_ptr<cricket::VideoFrame> frame);

  int32_t ConvertFrame(ppapi::HostResource* image_data_resource,
                       PP_TimeTicks* timestamp);
  void SendFrame(const ppapi::HostResource& image_data_resource,
                 PP_TimeTicks timestamp,
                 int32_t result);
  void Close();

  RendererPpapiHost* renderer_ppapi_host_;

  base::WeakPtrFactory<PepperVideoSourceHost> weak_factory_;

  ppapi::host::ReplyMessageContext reply_context_;
  scoped_refptr<base::MessageLoopProxy> main_message_loop_proxy_;

  scoped_ptr<content::VideoSourceHandler> source_handler_;
  std::string stream_url_;
  scoped_ptr<cricket::VideoFrame> last_frame_;
  bool get_frame_pending_;

  DISALLOW_COPY_AND_ASSIGN(PepperVideoSourceHost);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_VIDEO_SOURCE_HOST_H_
