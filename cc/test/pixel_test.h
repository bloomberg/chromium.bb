// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "cc/output/gl_renderer.h"
#include "cc/output/software_renderer.h"
#include "cc/quads/render_pass.h"
#include "cc/test/pixel_comparator.h"
#include "cc/trees/layer_tree_settings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
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
class TestGpuMemoryBufferManager;
class TestSharedBitmapManager;

class PixelTest : public testing::Test {
 protected:
  PixelTest();
  ~PixelTest() override;

  bool RunPixelTest(RenderPassList* pass_list,
                    const base::FilePath& ref_file,
                    const PixelComparator& comparator);

  bool RunPixelTest(RenderPassList* pass_list,
                    std::vector<SkColor>* ref_pixels,
                    const PixelComparator& comparator);

  bool RunPixelTestWithReadbackTarget(
      RenderPassList* pass_list,
      RenderPass* target,
      const base::FilePath& ref_file,
      const PixelComparator& comparator);

  bool RunPixelTestWithReadbackTargetAndArea(RenderPassList* pass_list,
                                             RenderPass* target,
                                             const base::FilePath& ref_file,
                                             const PixelComparator& comparator,
                                             const gfx::Rect* copy_rect);

  viz::ContextProvider* context_provider() const {
    return output_surface_->context_provider();
  }

  LayerTreeSettings settings_;
  viz::RendererSettings renderer_settings_;
  gfx::Size device_viewport_size_;
  bool disable_picture_quad_image_filtering_;
  std::unique_ptr<FakeOutputSurfaceClient> output_surface_client_;
  std::unique_ptr<OutputSurface> output_surface_;
  std::unique_ptr<TestSharedBitmapManager> shared_bitmap_manager_;
  std::unique_ptr<TestGpuMemoryBufferManager> gpu_memory_buffer_manager_;
  std::unique_ptr<BlockingTaskRunner> main_thread_task_runner_;
  std::unique_ptr<ResourceProvider> resource_provider_;
  std::unique_ptr<TextureMailboxDeleter> texture_mailbox_deleter_;
  std::unique_ptr<DirectRenderer> renderer_;
  SoftwareRenderer* software_renderer_ = nullptr;
  std::unique_ptr<SkBitmap> result_bitmap_;

  void SetUpGLRenderer(bool use_skia_gpu_backend, bool flipped_output_surface);
  void SetUpSoftwareRenderer();

  void EnableExternalStencilTest();

 private:
  void ReadbackResult(base::Closure quit_run_loop,
                      std::unique_ptr<CopyOutputResult> result);

  bool PixelsMatchReference(const base::FilePath& ref_file,
                            const PixelComparator& comparator);

  std::unique_ptr<gl::DisableNullDrawGLBindings> enable_pixel_output_;
};

template<typename RendererType>
class RendererPixelTest : public PixelTest {
 public:
  RendererType* renderer() {
    return static_cast<RendererType*>(renderer_.get());
  }

 protected:
  void SetUp() override;
};

// Wrappers to differentiate renderers where the the output surface and viewport
// have an externally determined size and offset.
class GLRendererWithExpandedViewport : public GLRenderer {
 public:
  GLRendererWithExpandedViewport(const viz::RendererSettings* settings,
                                 OutputSurface* output_surface,
                                 ResourceProvider* resource_provider,
                                 TextureMailboxDeleter* texture_mailbox_deleter)
      : GLRenderer(settings,
                   output_surface,
                   resource_provider,
                   texture_mailbox_deleter) {}
};

class SoftwareRendererWithExpandedViewport : public SoftwareRenderer {
 public:
  SoftwareRendererWithExpandedViewport(const viz::RendererSettings* settings,
                                       OutputSurface* output_surface,
                                       ResourceProvider* resource_provider)
      : SoftwareRenderer(settings, output_surface, resource_provider) {}
};

class GLRendererWithFlippedSurface : public GLRenderer {
 public:
  GLRendererWithFlippedSurface(const viz::RendererSettings* settings,
                               OutputSurface* output_surface,
                               ResourceProvider* resource_provider,
                               TextureMailboxDeleter* texture_mailbox_deleter)
      : GLRenderer(settings,
                   output_surface,
                   resource_provider,
                   texture_mailbox_deleter) {}
};

template<>
inline void RendererPixelTest<GLRenderer>::SetUp() {
  SetUpGLRenderer(false, false);
}

template<>
inline void RendererPixelTest<GLRendererWithExpandedViewport>::SetUp() {
  SetUpGLRenderer(false, false);
}

template <>
inline void RendererPixelTest<GLRendererWithFlippedSurface>::SetUp() {
  SetUpGLRenderer(false, true);
}

template <>
inline void RendererPixelTest<SoftwareRenderer>::SetUp() {
  SetUpSoftwareRenderer();
}

template<>
inline void RendererPixelTest<SoftwareRendererWithExpandedViewport>::SetUp() {
  SetUpSoftwareRenderer();
}

typedef RendererPixelTest<GLRenderer> GLRendererPixelTest;
typedef RendererPixelTest<SoftwareRenderer> SoftwareRendererPixelTest;

}  // namespace cc

#endif  // CC_TEST_PIXEL_TEST_H_
