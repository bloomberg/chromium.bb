// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_decode_accelerator_unittest_helpers.h"

#include "base/callback_helpers.h"
#include "base/files/file_util.h"
#include "base/strings/string_split.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/gpu/format_utils.h"
#include "media/gpu/test/rendering_helper.h"

#if defined(OS_CHROMEOS)
#include "ui/gfx/native_pixmap.h"
#include "ui/ozone/public/ozone_gpu_test_helper.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#endif

namespace media {
namespace test {

namespace {
const int kMD5StringLength = 32;
}  // namespace

VideoDecodeAcceleratorTestEnvironment::VideoDecodeAcceleratorTestEnvironment(
    bool use_gl_renderer)
    : use_gl_renderer_(use_gl_renderer),
      rendering_thread_("GLRenderingVDAClientThread") {}

void VideoDecodeAcceleratorTestEnvironment::SetUp() {
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_UI;
  rendering_thread_.StartWithOptions(options);

  base::WaitableEvent done(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  rendering_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&RenderingHelper::InitializeOneOff, use_gl_renderer_, &done));
  done.Wait();

#if defined(OS_CHROMEOS)
  gpu_helper_.reset(new ui::OzoneGpuTestHelper());
  // Need to initialize after the rendering side since the rendering side
  // initializes the "GPU" parts of Ozone.
  //
  // This also needs to be done in the test environment since this shouldn't
  // be initialized multiple times for the same Ozone platform.
  gpu_helper_->Initialize(base::ThreadTaskRunnerHandle::Get());
#endif
}

void VideoDecodeAcceleratorTestEnvironment::TearDown() {
#if defined(OS_CHROMEOS)
  gpu_helper_.reset();
#endif
  rendering_thread_.Stop();
}

scoped_refptr<base::SingleThreadTaskRunner>
VideoDecodeAcceleratorTestEnvironment::GetRenderingTaskRunner() const {
  return rendering_thread_.task_runner();
}

TextureRef::TextureRef(uint32_t texture_id,
                       const base::Closure& no_longer_needed_cb)
    : texture_id_(texture_id), no_longer_needed_cb_(no_longer_needed_cb) {}

TextureRef::~TextureRef() {
  base::ResetAndReturn(&no_longer_needed_cb_).Run();
}

// static
scoped_refptr<TextureRef> TextureRef::Create(
    uint32_t texture_id,
    const base::Closure& no_longer_needed_cb) {
  return base::WrapRefCounted(new TextureRef(texture_id, no_longer_needed_cb));
}

// static
scoped_refptr<TextureRef> TextureRef::CreatePreallocated(
    uint32_t texture_id,
    const base::Closure& no_longer_needed_cb,
    VideoPixelFormat pixel_format,
    const gfx::Size& size) {
  scoped_refptr<TextureRef> texture_ref;
#if defined(OS_CHROMEOS)
  texture_ref = TextureRef::Create(texture_id, no_longer_needed_cb);
  LOG_ASSERT(texture_ref);

  ui::OzonePlatform* platform = ui::OzonePlatform::GetInstance();
  ui::SurfaceFactoryOzone* factory = platform->GetSurfaceFactoryOzone();
  gfx::BufferFormat buffer_format =
      VideoPixelFormatToGfxBufferFormat(pixel_format);
  texture_ref->pixmap_ =
      factory->CreateNativePixmap(gfx::kNullAcceleratedWidget, size,
                                  buffer_format, gfx::BufferUsage::SCANOUT);
  LOG_ASSERT(texture_ref->pixmap_);
#endif

  return texture_ref;
}

gfx::GpuMemoryBufferHandle TextureRef::ExportGpuMemoryBufferHandle() const {
  gfx::GpuMemoryBufferHandle handle;
#if defined(OS_CHROMEOS)
  CHECK(pixmap_);
  handle.type = gfx::NATIVE_PIXMAP;
  for (size_t i = 0; i < pixmap_->GetDmaBufFdCount(); i++) {
    int duped_fd = HANDLE_EINTR(dup(pixmap_->GetDmaBufFd(i)));
    LOG_ASSERT(duped_fd != -1) << "Failed duplicating dmabuf fd";
    handle.native_pixmap_handle.fds.emplace_back(
        base::FileDescriptor(duped_fd, true));
    handle.native_pixmap_handle.planes.emplace_back(
        pixmap_->GetDmaBufPitch(i), pixmap_->GetDmaBufOffset(i), i,
        pixmap_->GetDmaBufModifier(i));
  }
#endif
  return handle;
}

// Read in golden MD5s for the thumbnailed rendering of this video
void ReadGoldenThumbnailMD5s(const base::FilePath& md5_file_path,
                             std::vector<std::string>* md5_strings) {
  std::string all_md5s;
  base::ReadFileToString(md5_file_path, &all_md5s);
  *md5_strings = base::SplitString(all_md5s, "\n", base::TRIM_WHITESPACE,
                                   base::SPLIT_WANT_ALL);
  // Check these are legitimate MD5s.
  for (const std::string& md5_string : *md5_strings) {
    // Ignore the empty string added by SplitString
    if (!md5_string.length())
      continue;
    // Ignore comments
    if (md5_string.at(0) == '#')
      continue;

    LOG_IF(ERROR, static_cast<int>(md5_string.length()) != kMD5StringLength)
        << "MD5 length error: " << md5_string;
    bool hex_only = std::count_if(md5_string.begin(), md5_string.end(),
                                  isxdigit) == kMD5StringLength;
    LOG_IF(ERROR, !hex_only) << "MD5 includes non-hex char: " << md5_string;
  }
  LOG_IF(ERROR, md5_strings->empty())
      << "  MD5 checksum file (" << md5_file_path.MaybeAsASCII()
      << ") missing or empty.";
}

}  // namespace test
}  // namespace media
