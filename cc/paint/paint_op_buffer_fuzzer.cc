// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "cc/paint/paint_op_buffer.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/gl/GrGLInterface.h"

// Deserialize an arbitrary number of cc::PaintOps and raster them
// using gpu raster into an SkCanvas.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  const size_t kMaxSerializedSize = 1000000;
  const size_t kRasterDimension = 32;

  SkImageInfo image_info = SkImageInfo::MakeN32(
      kRasterDimension, kRasterDimension, kOpaque_SkAlphaType);
  sk_sp<const GrGLInterface> gl_interface(GrGLCreateNullInterface());
  sk_sp<GrContext> gr_context(GrContext::Create(
      kOpenGL_GrBackend,
      reinterpret_cast<GrBackendContext>(gl_interface.get())));
  sk_sp<SkSurface> surface = SkSurface::MakeRenderTarget(
      gr_context.get(), SkBudgeted::kYes, image_info);
  SkCanvas* canvas = surface->getCanvas();
  SkMatrix original_ctm = canvas->getTotalMatrix();

  // Need 4 bytes to be able to read the type/skip.
  while (size >= 4) {
    const cc::PaintOp* serialized = reinterpret_cast<const cc::PaintOp*>(data);

    if (serialized->skip > kMaxSerializedSize)
      break;

    size_t deserialized_size = sizeof(cc::LargestPaintOp) + serialized->skip;
    std::unique_ptr<char, base::AlignedFreeDeleter> deserialized(
        static_cast<char*>(base::AlignedAlloc(
            deserialized_size, cc::PaintOpBuffer::PaintOpAlign)));
    cc::PaintOp* deserialized_op = cc::PaintOp::Deserialize(
        data, size, deserialized.get(), deserialized_size);

    if (!deserialized_op)
      break;

    deserialized_op->Raster(canvas, original_ctm);

    deserialized_op->DestroyThis();

    if (serialized->skip >= size)
      break;

    size -= serialized->skip;
    data += serialized->skip;
  }
  return 0;
}
