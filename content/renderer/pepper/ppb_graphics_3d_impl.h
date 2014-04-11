// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PPB_GRAPHICS_3D_IMPL_H_
#define CONTENT_RENDERER_PEPPER_PPB_GRAPHICS_3D_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "ppapi/shared_impl/ppb_graphics_3d_shared.h"
#include "ppapi/shared_impl/resource.h"

namespace content {
class PlatformContext3D;

class PPB_Graphics3D_Impl : public ppapi::PPB_Graphics3D_Shared {
 public:
  static PP_Resource Create(PP_Instance instance,
                            PP_Resource share_context,
                            const int32_t* attrib_list);
  static PP_Resource CreateRaw(PP_Instance instance,
                               PP_Resource share_context,
                               const int32_t* attrib_list);

  // PPB_Graphics3D_API trusted implementation.
  virtual PP_Bool SetGetBuffer(int32_t transfer_buffer_id) OVERRIDE;
  virtual gpu::CommandBuffer::State GetState() OVERRIDE;
  virtual scoped_refptr<gpu::Buffer> CreateTransferBuffer(uint32_t size,
                                                          int32* id) OVERRIDE;
  virtual PP_Bool DestroyTransferBuffer(int32_t id) OVERRIDE;
  virtual PP_Bool Flush(int32_t put_offset) OVERRIDE;
  virtual gpu::CommandBuffer::State WaitForTokenInRange(int32_t start,
                                                        int32_t end) OVERRIDE;
  virtual gpu::CommandBuffer::State WaitForGetOffsetInRange(int32_t start,
                                                            int32_t end)
      OVERRIDE;
  virtual uint32_t InsertSyncPoint() OVERRIDE;

  // Binds/unbinds the graphics of this context with the associated instance.
  // Returns true if binding/unbinding is successful.
  bool BindToInstance(bool bind);

  // Returns true if the backing texture is always opaque.
  bool IsOpaque();

  // Notifications about the view's progress painting.  See PluginInstance.
  // These messages are used to send Flush callbacks to the plugin.
  void ViewInitiatedPaint();
  void ViewFlushedPaint();

  PlatformContext3D* platform_context() { return platform_context_.get(); }

 protected:
  virtual ~PPB_Graphics3D_Impl();
  // ppapi::PPB_Graphics3D_Shared overrides.
  virtual gpu::CommandBuffer* GetCommandBuffer() OVERRIDE;
  virtual gpu::GpuControl* GetGpuControl() OVERRIDE;
  virtual int32 DoSwapBuffers() OVERRIDE;

 private:
  explicit PPB_Graphics3D_Impl(PP_Instance instance);

  bool Init(PPB_Graphics3D_API* share_context, const int32_t* attrib_list);
  bool InitRaw(PPB_Graphics3D_API* share_context, const int32_t* attrib_list);

  // Notifications received from the GPU process.
  void OnSwapBuffers();
  void OnContextLost();
  void OnConsoleMessage(const std::string& msg, int id);
  // Notifications sent to plugin.
  void SendContextLost();

  // True if context is bound to instance.
  bool bound_to_instance_;
  // True when waiting for compositor to commit our backing texture.
  bool commit_pending_;
  // The 3D Context. Responsible for providing the command buffer.
  scoped_ptr<PlatformContext3D> platform_context_;
  base::WeakPtrFactory<PPB_Graphics3D_Impl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PPB_Graphics3D_Impl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PPB_GRAPHICS_3D_IMPL_H_
