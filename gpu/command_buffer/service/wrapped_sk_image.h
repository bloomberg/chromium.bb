// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_WRAPPED_SK_IMAGE_H_
#define GPU_COMMAND_BUFFER_SERVICE_WRAPPED_SK_IMAGE_H_

#include <memory>

#include "base/macros.h"
#include "components/viz/common/resources/resource_format.h"
#include "gpu/command_buffer/service/shared_image_backing_factory.h"
#include "gpu/command_buffer/service/texture_base.h"
#include "gpu/gpu_gles2_export.h"
#include "third_party/skia/include/core/SkImage.h"
#include "ui/gfx/geometry/size.h"

class GrBackendTexture;
class SkSurfaceProps;

namespace gpu {
namespace raster {

struct RasterDecoderContextState;

class GPU_GLES2_EXPORT WrappedSkImageFactory
    : public gpu::SharedImageBackingFactory {
 public:
  explicit WrappedSkImageFactory(RasterDecoderContextState* context_state);
  ~WrappedSkImageFactory() override;

  // SharedImageBackingFactory implementation:
  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      uint32_t usage) override;

 private:
  RasterDecoderContextState* const context_state_;

  DISALLOW_COPY_AND_ASSIGN(WrappedSkImageFactory);
};

class GPU_GLES2_EXPORT WrappedSkImage : public TextureBase {
 public:
  ~WrappedSkImage() override;
  bool Initialize(const gfx::Size& size, viz::ResourceFormat format);

  // TextureBase implementation:
  TextureBase::Type GetType() const override;
  uint64_t GetTracingId() const override;

  static WrappedSkImage* CheckedCast(TextureBase* texture);

  sk_sp<SkSurface> GetSkSurface(int final_msaa_count,
                                SkColorType color_type,
                                const SkSurfaceProps& surface_props);
  bool GetGrBackendTexture(GrBackendTexture* gr_texture) const;

  uint32_t estimated_size() const { return estimated_size_; }
  bool cleared() const { return cleared_; }
  void SetCleared() { cleared_ = true; }

 private:
  friend class WrappedSkImageFactory;

  explicit WrappedSkImage(raster::RasterDecoderContextState* context_state);

  RasterDecoderContextState* const context_state_;

  sk_sp<SkImage> image_;
  uint32_t estimated_size_ = 0;
  bool cleared_ = false;

  uint64_t tracing_id_ = 0;

  DISALLOW_COPY_AND_ASSIGN(WrappedSkImage);
};

}  // namespace raster
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_WRAPPED_SK_IMAGE_H_
