// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_TEXTURE_REF_H_
#define MEDIA_GPU_TEST_TEXTURE_REF_H_

#include "base/memory/scoped_refptr.h"
#include "base/threading/thread_checker.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "ui/gfx/gpu_memory_buffer.h"
#if defined(OS_CHROMEOS)
#include "ui/gfx/geometry/size.h"
#endif

#if defined(OS_CHROMEOS)
namespace gfx {
class NativePixmap;
}
#endif

namespace media {
namespace test {

// A helper class used to manage the lifetime of a Texture. Can be backed by
// either a buffer allocated by the VDA, or by a preallocated pixmap.
class TextureRef : public base::RefCounted<TextureRef> {
 public:
  static scoped_refptr<TextureRef> Create(
      uint32_t texture_id,
      base::OnceClosure no_longer_needed_cb);

  static scoped_refptr<TextureRef> CreatePreallocated(
      uint32_t texture_id,
      base::OnceClosure no_longer_needed_cb,
      VideoPixelFormat pixel_format,
      const gfx::Size& size);

  gfx::GpuMemoryBufferHandle ExportGpuMemoryBufferHandle() const;
  scoped_refptr<VideoFrame> CreateVideoFrame(
      const gfx::Rect& visible_rect) const;

  int32_t texture_id() const { return texture_id_; }

 private:
  friend class base::RefCounted<TextureRef>;

  TextureRef(uint32_t texture_id, base::OnceClosure no_longer_needed_cb);
  ~TextureRef();

  uint32_t texture_id_;
  base::OnceClosure no_longer_needed_cb_;
#if defined(OS_CHROMEOS)
  scoped_refptr<gfx::NativePixmap> pixmap_;
  gfx::Size coded_size_;
#endif
  THREAD_CHECKER(thread_checker_);
};

}  // namespace test
}  // namespace media

#endif  // MEDIA_GPU_TEST_TEXTURE_REF_H_
