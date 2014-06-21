// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "cc/output/gl_renderer.h"
#include "cc/output/software_renderer.h"
#include "cc/quads/render_pass.h"
#include "cc/test/pixel_comparator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/size.h"
#include "ui/gl/gl_implementation.h"

#ifndef CC_TEST_PIXEL_TEST_H_
#define CC_TEST_PIXEL_TEST_H_

namespace cc {
class CopyOutputResult;
class DirectRenderer;
class FakeOutputSurfaceClient;
class OutputSurface;
class ResourceProvider;
class SoftwareRenderer;
class SharedBitmapManager;

class PixelTest : public testing::Test, RendererClient {
 protected:
  PixelTest();
  virtual ~PixelTest();

  bool RunPixelTest(RenderPassList* pass_list,
                    const base::FilePath& ref_file,
                    const PixelComparator& comparator);

  bool RunPixelTestWithReadbackTarget(
      RenderPassList* pass_list,
      RenderPass* target,
      const base::FilePath& ref_file,
      const PixelComparator& comparator);

  LayerTreeSettings settings_;
  gfx::Size device_viewport_size_;
  bool disable_picture_quad_image_filtering_;
  class PixelTestRendererClient;
  scoped_ptr<FakeOutputSurfaceClient> output_surface_client_;
  scoped_ptr<OutputSurface> output_surface_;
  scoped_ptr<SharedBitmapManager> shared_bitmap_manager_;
  scoped_ptr<ResourceProvider> resource_provider_;
  scoped_ptr<TextureMailboxDeleter> texture_mailbox_deleter_;
  scoped_ptr<DirectRenderer> renderer_;
  scoped_ptr<SkBitmap> result_bitmap_;
  gfx::Vector2d external_device_viewport_offset_;
  gfx::Rect external_device_clip_rect_;

  void SetUpGLRenderer(bool use_skia_gpu_backend);
  void SetUpSoftwareRenderer();

  void ForceExpandedViewport(const gfx::Size& surface_expansion);
  void ForceViewportOffset(const gfx::Vector2d& viewport_offset);
  void ForceDeviceClip(const gfx::Rect& clip);
  void EnableExternalStencilTest();

  // RendererClient implementation.
  virtual void SetFullRootLayerDamage() OVERRIDE {}
  virtual void RunOnDemandRasterTask(Task* on_demand_raster_task) OVERRIDE;

 private:
  void ReadbackResult(base::Closure quit_run_loop,
                      scoped_ptr<CopyOutputResult> result);

  bool PixelsMatchReference(const base::FilePath& ref_file,
                            const PixelComparator& comparator);

  scoped_ptr<gfx::DisableNullDrawGLBindings> enable_pixel_output_;
};

template<typename RendererType>
class RendererPixelTest : public PixelTest {
 public:
  RendererType* renderer() {
    return static_cast<RendererType*>(renderer_.get());
  }

  bool UseSkiaGPUBackend() const;
  bool ExpandedViewport() const;

 protected:
  virtual void SetUp() OVERRIDE;
};

// Wrappers to differentiate renderers where the the output surface and viewport
// have an externally determined size and offset.
class GLRendererWithExpandedViewport : public GLRenderer {
 public:
  GLRendererWithExpandedViewport(RendererClient* client,
                                 const LayerTreeSettings* settings,
                                 OutputSurface* output_surface,
                                 ResourceProvider* resource_provider,
                                 TextureMailboxDeleter* texture_mailbox_deleter,
                                 int highp_threshold_min)
      : GLRenderer(client,
                   settings,
                   output_surface,
                   resource_provider,
                   texture_mailbox_deleter,
                   highp_threshold_min) {}
};

class SoftwareRendererWithExpandedViewport : public SoftwareRenderer {
 public:
  SoftwareRendererWithExpandedViewport(RendererClient* client,
                                       const LayerTreeSettings* settings,
                                       OutputSurface* output_surface,
                                       ResourceProvider* resource_provider)
      : SoftwareRenderer(client, settings, output_surface, resource_provider) {}
};

template<>
inline void RendererPixelTest<GLRenderer>::SetUp() {
  SetUpGLRenderer(false);
}

template<>
inline bool RendererPixelTest<GLRenderer>::UseSkiaGPUBackend() const {
  return false;
}

template<>
inline bool RendererPixelTest<GLRenderer>::ExpandedViewport() const {
  return false;
}

template<>
inline void RendererPixelTest<GLRendererWithExpandedViewport>::SetUp() {
  SetUpGLRenderer(false);
  ForceExpandedViewport(gfx::Size(50, 50));
  ForceViewportOffset(gfx::Vector2d(10, 20));
}

template <>
inline bool
RendererPixelTest<GLRendererWithExpandedViewport>::UseSkiaGPUBackend() const {
  return false;
}

template <>
inline bool
RendererPixelTest<GLRendererWithExpandedViewport>::ExpandedViewport() const {
  return true;
}

template<>
inline void RendererPixelTest<SoftwareRenderer>::SetUp() {
  SetUpSoftwareRenderer();
}

template<>
inline bool RendererPixelTest<SoftwareRenderer>::UseSkiaGPUBackend() const {
  return false;
}

template <>
inline bool RendererPixelTest<SoftwareRenderer>::ExpandedViewport() const {
  return false;
}

template<>
inline void RendererPixelTest<SoftwareRendererWithExpandedViewport>::SetUp() {
  SetUpSoftwareRenderer();
  ForceExpandedViewport(gfx::Size(50, 50));
  ForceViewportOffset(gfx::Vector2d(10, 20));
}

template <>
inline bool RendererPixelTest<
    SoftwareRendererWithExpandedViewport>::UseSkiaGPUBackend() const {
  return false;
}

template <>
inline bool RendererPixelTest<
    SoftwareRendererWithExpandedViewport>::ExpandedViewport() const {
  return true;
}

typedef RendererPixelTest<GLRenderer> GLRendererPixelTest;
typedef RendererPixelTest<SoftwareRenderer> SoftwareRendererPixelTest;

}  // namespace cc

#endif  // CC_TEST_PIXEL_TEST_H_
