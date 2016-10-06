// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_OUTPUT_SURFACE_H_
#define CC_OUTPUT_OUTPUT_SURFACE_H_

#include <deque>
#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/trace_event/memory_dump_provider.h"
#include "cc/base/cc_export.h"
#include "cc/output/context_provider.h"
#include "cc/output/overlay_candidate_validator.h"
#include "cc/output/software_output_device.h"
#include "cc/output/vulkan_context_provider.h"
#include "cc/resources/returned_resource.h"
#include "gpu/command_buffer/common/texture_in_use_response.h"
#include "ui/gfx/color_space.h"

namespace ui {
class LatencyInfo;
}

namespace gfx {
class ColorSpace;
class Rect;
class Size;
class Transform;
}

namespace cc {

struct ManagedMemoryPolicy;
class OutputSurfaceClient;
class OutputSurfaceFrame;

// Represents the output surface for a compositor. The compositor owns
// and manages its destruction. Its lifetime is:
//   1. Created on the main thread by the LayerTreeHost through its client.
//   2. Passed to the compositor thread and bound to a client via BindToClient.
//      From here on, it will only be used on the compositor thread.
//   3. If the 3D context is lost, then the compositor will delete the output
//      surface (on the compositor thread) and go back to step 1.
class CC_EXPORT OutputSurface : public base::trace_event::MemoryDumpProvider {
 public:
  struct Capabilities {
    Capabilities() = default;

    int max_frames_pending = 1;
    // Whether this output surface renders to the default OpenGL zero
    // framebuffer or to an offscreen framebuffer.
    bool uses_default_gl_framebuffer = true;
    // Whether this OutputSurface is flipped or not.
    bool flipped_output_surface = false;
  };

  // Constructor for GL-based compositing.
  explicit OutputSurface(scoped_refptr<ContextProvider> context_provider);
  // Constructor for software compositing.
  explicit OutputSurface(std::unique_ptr<SoftwareOutputDevice> software_device);
  // Constructor for Vulkan-based compositing.
  explicit OutputSurface(
      scoped_refptr<VulkanContextProvider> vulkan_context_provider);

  ~OutputSurface() override;

  // Called by the compositor on the compositor thread. This is a place where
  // thread-specific data for the output surface can be initialized, since from
  // this point to when DetachFromClient() is called the output surface will
  // only be used on the compositor thread.
  // The caller should call DetachFromClient() on the same thread before
  // destroying the OutputSurface, even if this fails. And BindToClient should
  // not be called twice for a given OutputSurface.
  virtual bool BindToClient(OutputSurfaceClient* client);

  const Capabilities& capabilities() const { return capabilities_; }

  // Obtain the 3d context or the software device associated with this output
  // surface. Either of these may return a null pointer, but not both.
  // In the event of a lost context, the entire output surface should be
  // recreated.
  ContextProvider* context_provider() const { return context_provider_.get(); }
  VulkanContextProvider* vulkan_context_provider() const {
    return vulkan_context_provider_.get();
  }
  SoftwareOutputDevice* software_device() const {
    return software_device_.get();
  }

  virtual void EnsureBackbuffer() = 0;
  virtual void DiscardBackbuffer() = 0;

  // Bind the default framebuffer for drawing to, only valid for GL backed
  // OutputSurfaces.
  virtual void BindFramebuffer() = 0;

  const gfx::ColorSpace& device_color_space() const {
    return device_color_space_;
  }

  // Get the class capable of informing cc of hardware overlay capability.
  virtual OverlayCandidateValidator* GetOverlayCandidateValidator() const = 0;

  // Returns true if a main image overlay plane should be scheduled.
  virtual bool IsDisplayedAsOverlayPlane() const = 0;

  // Get the texture for the main image's overlay.
  virtual unsigned GetOverlayTextureId() const = 0;

  // If this returns true, then the surface will not attempt to draw.
  virtual bool SurfaceIsSuspendForRecycle() const = 0;

  virtual void Reshape(const gfx::Size& size,
                       float scale_factor,
                       const gfx::ColorSpace& color_space,
                       bool alpha);

  virtual bool HasExternalStencilTest() const = 0;
  virtual void ApplyExternalStencil() = 0;

  // Gives the GL internal format that should be used for calling CopyTexImage2D
  // when the framebuffer is bound via BindFramebuffer().
  virtual uint32_t GetFramebufferCopyTextureFormat() = 0;

  // The implementation may destroy or steal the contents of the CompositorFrame
  // passed in (though it will not take ownership of the CompositorFrame
  // itself). For successful swaps, the implementation must call
  // OutputSurfaceClient::DidSwapBuffersComplete() eventually.
  virtual void SwapBuffers(OutputSurfaceFrame frame) = 0;

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

 protected:
  void PostSwapBuffersComplete();

  // Used internally for the context provider to inform the client about loss,
  // and can be overridden to change behaviour instead of informing the client.
  virtual void DidLoseOutputSurface();

  OutputSurfaceClient* client_ = nullptr;

  struct OutputSurface::Capabilities capabilities_;
  scoped_refptr<ContextProvider> context_provider_;
  scoped_refptr<VulkanContextProvider> vulkan_context_provider_;
  std::unique_ptr<SoftwareOutputDevice> software_device_;
  gfx::Size surface_size_;
  float device_scale_factor_ = -1;
  gfx::ColorSpace device_color_space_;
  bool has_alpha_ = true;
  gfx::ColorSpace color_space_;
  base::ThreadChecker thread_checker_;

 private:
  void OnSwapBuffersComplete();

  base::WeakPtrFactory<OutputSurface> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OutputSurface);
};

}  // namespace cc

#endif  // CC_OUTPUT_OUTPUT_SURFACE_H_
