// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_GRAPHICS_2D_HOST_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_GRAPHICS_2D_HOST_H_

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "ppapi/c/ppb_graphics_2d.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/resource_host.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/ppapi/ppb_graphics_2d_impl.h"

namespace webkit {
namespace ppapi {
class PPB_ImageData_Impl;
class PluginInstance;
}
}

namespace content {

class RendererPpapiHost;

class CONTENT_EXPORT PepperGraphics2DHost
    : public ppapi::host::ResourceHost,
      public webkit::ppapi::PluginDelegate::PlatformGraphics2D,
      public base::SupportsWeakPtr<PepperGraphics2DHost> {
 public:
  static PepperGraphics2DHost* Create(RendererPpapiHost* host,
                                      PP_Instance instance,
                                      PP_Resource resource,
                                      const PP_Size& size,
                                      PP_Bool is_always_opaque);

  virtual ~PepperGraphics2DHost();

  virtual int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) OVERRIDE;

  // PlatformGraphics2D overrides.
  virtual bool ReadImageData(PP_Resource image,
                             const PP_Point* top_left) OVERRIDE;
  virtual bool BindToInstance(
      webkit::ppapi::PluginInstance* new_instance) OVERRIDE;
  virtual void Paint(WebKit::WebCanvas* canvas,
                     const gfx::Rect& plugin_rect,
                     const gfx::Rect& paint_rect) OVERRIDE;
  virtual void ViewWillInitiatePaint() OVERRIDE;
  virtual void ViewInitiatedPaint() OVERRIDE;
  virtual void ViewFlushedPaint() OVERRIDE;
  virtual void SetScale(float scale) OVERRIDE;
  virtual float GetScale() const OVERRIDE;
  virtual bool IsAlwaysOpaque() const OVERRIDE;
  virtual webkit::ppapi::PPB_ImageData_Impl* ImageData() OVERRIDE;
  virtual bool IsGraphics2DHost() const OVERRIDE;

 private:
  PepperGraphics2DHost(RendererPpapiHost* host,
                       PP_Instance instance,
                       PP_Resource resource);

  int32_t OnHostMsgPaintImageData(ppapi::host::HostMessageContext* context,
                                  const ppapi::HostResource& image_data,
                                  const PP_Point& top_left,
                                  bool src_rect_specified,
                                  const PP_Rect& src_rect);
  int32_t OnHostMsgScroll(ppapi::host::HostMessageContext* context,
                          bool clip_specified,
                          const PP_Rect& clip,
                          const PP_Point& amount);
  int32_t OnHostMsgReplaceContents(ppapi::host::HostMessageContext* context,
                                   const ppapi::HostResource& image_data);
  int32_t OnHostMsgFlush(ppapi::host::HostMessageContext* context);
  int32_t OnHostMsgSetScale(ppapi::host::HostMessageContext* context,
                            float scale);
  int32_t OnHostMsgReadImageData(ppapi::host::HostMessageContext* context,
                                 PP_Resource image,
                                 const PP_Point& top_left);

  static void SendFlushACKToPlugin(void* data, int32_t pp_error);

  // TODO: merge this delegation into this host class.
  scoped_refptr<webkit::ppapi::PPB_Graphics2D_Impl> graphics_2d_;

  ppapi::host::ReplyMessageContext flush_reply_context_;

  bool is_running_in_process_;

  DISALLOW_COPY_AND_ASSIGN(PepperGraphics2DHost);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_GRAPHICS_2D_HOST_H_
