// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_VIDEO_DECODE_ACCELERATOR_UNITTEST_HELPERS_H_
#define MEDIA_GPU_TEST_VIDEO_DECODE_ACCELERATOR_UNITTEST_HELPERS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "media/base/video_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/native_pixmap.h"

#if defined(OS_CHROMEOS)
namespace ui {
class OzoneGpuTestHelper;
}  // namespace ui
#endif

namespace media {
namespace test {

// Initialize the GPU thread for rendering. We only need to setup once
// for all test cases.
class VideoDecodeAcceleratorTestEnvironment : public ::testing::Environment {
 public:
  explicit VideoDecodeAcceleratorTestEnvironment(bool use_gl_renderer);

  void SetUp() override;
  void TearDown() override;
  scoped_refptr<base::SingleThreadTaskRunner> GetRenderingTaskRunner() const;

 private:
  bool use_gl_renderer_;
  base::Thread rendering_thread_;
#if defined(OS_CHROMEOS)
  std::unique_ptr<ui::OzoneGpuTestHelper> gpu_helper_;
#endif

  DISALLOW_COPY_AND_ASSIGN(VideoDecodeAcceleratorTestEnvironment);
};

// A helper class used to manage the lifetime of a Texture. Can be backed by
// either a buffer allocated by the VDA, or by a preallocated pixmap.
class TextureRef : public base::RefCounted<TextureRef> {
 public:
  static scoped_refptr<TextureRef> Create(
      uint32_t texture_id,
      const base::Closure& no_longer_needed_cb);

  static scoped_refptr<TextureRef> CreatePreallocated(
      uint32_t texture_id,
      const base::Closure& no_longer_needed_cb,
      VideoPixelFormat pixel_format,
      const gfx::Size& size);

  gfx::GpuMemoryBufferHandle ExportGpuMemoryBufferHandle() const;

  int32_t texture_id() const { return texture_id_; }

 private:
  friend class base::RefCounted<TextureRef>;

  TextureRef(uint32_t texture_id, const base::Closure& no_longer_needed_cb);
  ~TextureRef();

  uint32_t texture_id_;
  base::Closure no_longer_needed_cb_;
#if defined(OS_CHROMEOS)
  scoped_refptr<gfx::NativePixmap> pixmap_;
#endif
};

// Read in golden MD5s for the thumbnailed rendering of this video
void ReadGoldenThumbnailMD5s(const base::FilePath& md5_file_path,
                             std::vector<std::string>* md5_strings);
}  // namespace test
}  // namespace media
#endif  // MEDIA_GPU_TEST_VIDEO_DECODE_ACCELERATOR_UNITTEST_HELPERS_H_
