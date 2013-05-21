// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "cc/output/gl_renderer.h"
#include "cc/quads/render_pass.h"
#include "cc/test/pixel_comparator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/size.h"

#ifndef CC_TEST_PIXEL_TEST_H_
#define CC_TEST_PIXEL_TEST_H_

namespace cc {
class DirectRenderer;
class SoftwareRenderer;
class OutputSurface;
class ResourceProvider;

class PixelTest : public testing::Test {
 protected:
  PixelTest();
  virtual ~PixelTest();

  bool RunPixelTest(RenderPassList* pass_list,
                    const base::FilePath& ref_file,
                    const PixelComparator& comparator);

  bool RunPixelTestWithReadbackTarget(RenderPassList* pass_list,
                                      RenderPass* target,
                                      const base::FilePath& ref_file,
                                      const PixelComparator& comparator);

  gfx::Size device_viewport_size_;
  scoped_ptr<OutputSurface> output_surface_;
  scoped_ptr<ResourceProvider> resource_provider_;
  class PixelTestRendererClient;
  scoped_ptr<PixelTestRendererClient> fake_client_;
  scoped_ptr<DirectRenderer> renderer_;
  scoped_ptr<SkBitmap> result_bitmap_;

  void SetUpGLRenderer(bool use_skia_gpu_backend);
  void SetUpSoftwareRenderer();

 private:
  void ReadbackResult(base::Closure quit_run_loop, scoped_ptr<SkBitmap> bitmap);

  bool PixelsMatchReference(const base::FilePath& ref_file,
                            const PixelComparator& comparator);
};

template<typename RendererType>
class RendererPixelTest : public PixelTest {
 public:
  RendererType* renderer() {
    return static_cast<RendererType*>(renderer_.get());
  }

  bool UseSkiaGPUBackend() const;

 protected:
  virtual void SetUp() OVERRIDE;
};

// A simple wrapper to differentiate a renderer that should use ganesh
// and one that shouldn't in templates.
class GLRendererWithSkiaGPUBackend : public GLRenderer {
 public:
  GLRendererWithSkiaGPUBackend(RendererClient* client,
                       OutputSurface* output_surface,
                       ResourceProvider* resource_provider,
                       int highp_threshold_min)
      : GLRenderer(client,
                   output_surface,
                   resource_provider,
                   highp_threshold_min) {}
};

template<>
inline void RendererPixelTest<GLRenderer>::SetUp() {
  SetUpGLRenderer(false);
  DCHECK(!renderer()->CanUseSkiaGPUBackend());
}

template<>
inline bool RendererPixelTest<GLRenderer>::UseSkiaGPUBackend() const {
  return false;
}

template<>
inline void RendererPixelTest<GLRendererWithSkiaGPUBackend>::SetUp() {
  SetUpGLRenderer(true);
  DCHECK(renderer()->CanUseSkiaGPUBackend());
}

template <>
inline bool
RendererPixelTest<GLRendererWithSkiaGPUBackend>::UseSkiaGPUBackend() const {
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

typedef RendererPixelTest<GLRenderer> GLRendererPixelTest;
typedef RendererPixelTest<GLRendererWithSkiaGPUBackend>
    GLRendererSkiaGPUBackendPixelTest;
typedef RendererPixelTest<SoftwareRenderer> SoftwareRendererPixelTest;

}  // namespace cc

#endif  // CC_TEST_PIXEL_TEST_H_
