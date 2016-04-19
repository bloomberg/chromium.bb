// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_MEDIA_GPU_VIDEO_DECODE_ACCELERATOR_FACTORY_IMPL_H_
#define CONTENT_COMMON_GPU_MEDIA_GPU_VIDEO_DECODE_ACCELERATOR_FACTORY_IMPL_H_

#include "base/callback.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "gpu/command_buffer/service/gpu_preferences.h"
#include "gpu/config/gpu_info.h"
#include "media/video/video_decode_accelerator.h"

namespace gfx {
class GLContext;
}

namespace gl {
class GLImage;
}

namespace gpu {
struct GpuPreferences;

namespace gles2 {
class GLES2Decoder;
}
}

namespace content {

// TODO(posciak): this class should be an implementation of
// content::GpuVideoDecodeAcceleratorFactory, however that can only be achieved
// once this is moved out of content/common, see crbug.com/597150 and related.
class CONTENT_EXPORT GpuVideoDecodeAcceleratorFactoryImpl {
public:
  ~GpuVideoDecodeAcceleratorFactoryImpl();

  // Return current GLContext.
  using GetGLContextCallback = base::Callback<gfx::GLContext*(void)>;

  // Make the applicable GL context current. To be called by VDAs before
  // executing any GL calls. Return true on success, false otherwise.
  using MakeGLContextCurrentCallback = base::Callback<bool(void)>;

  // Bind |image| to |client_texture_id| given |texture_target|. If
  // |can_bind_to_sampler| is true, then the image may be used as a sampler
  // directly, otherwise a copy to a staging buffer is required.
  // Return true on success, false otherwise.
  using BindGLImageCallback =
      base::Callback<bool(uint32_t client_texture_id,
                          uint32_t texture_target,
                          const scoped_refptr<gl::GLImage>& image,
                          bool can_bind_to_sampler)>;

  // Return a WeakPtr to a GLES2Decoder, if one is available.
  using GetGLES2DecoderCallback =
      base::Callback<base::WeakPtr<gpu::gles2::GLES2Decoder>(void)>;

  static std::unique_ptr<GpuVideoDecodeAcceleratorFactoryImpl> Create(
      const GetGLContextCallback& get_gl_context_cb,
      const MakeGLContextCurrentCallback& make_context_current_cb,
      const BindGLImageCallback& bind_image_cb);

  static std::unique_ptr<GpuVideoDecodeAcceleratorFactoryImpl>
  CreateWithGLES2Decoder(
      const GetGLContextCallback& get_gl_context_cb,
      const MakeGLContextCurrentCallback& make_context_current_cb,
      const BindGLImageCallback& bind_image_cb,
      const GetGLES2DecoderCallback& get_gles2_decoder_cb);

  static std::unique_ptr<GpuVideoDecodeAcceleratorFactoryImpl> CreateWithNoGL();

  static gpu::VideoDecodeAcceleratorCapabilities GetDecoderCapabilities(
      const gpu::GpuPreferences& gpu_preferences);

  std::unique_ptr<media::VideoDecodeAccelerator> CreateVDA(
      media::VideoDecodeAccelerator::Client* client,
      const media::VideoDecodeAccelerator::Config& config,
      const gpu::GpuPreferences& gpu_preferences);

 private:
  GpuVideoDecodeAcceleratorFactoryImpl(
      const GetGLContextCallback& get_gl_context_cb,
      const MakeGLContextCurrentCallback& make_context_current_cb,
      const BindGLImageCallback& bind_image_cb,
      const GetGLES2DecoderCallback& get_gles2_decoder_cb);

#if defined(OS_WIN)
  std::unique_ptr<media::VideoDecodeAccelerator> CreateDXVAVDA(
      const gpu::GpuPreferences& gpu_preferences) const;
#endif
#if defined(OS_CHROMEOS) && defined(USE_V4L2_CODEC)
  std::unique_ptr<media::VideoDecodeAccelerator> CreateV4L2VDA(
      const gpu::GpuPreferences& gpu_preferences) const;
  std::unique_ptr<media::VideoDecodeAccelerator> CreateV4L2SVDA(
      const gpu::GpuPreferences& gpu_preferences) const;
#endif
#if defined(OS_CHROMEOS) && defined(ARCH_CPU_X86_FAMILY)
  std::unique_ptr<media::VideoDecodeAccelerator> CreateVaapiVDA(
      const gpu::GpuPreferences& gpu_preferences) const;
#endif
#if defined(OS_MACOSX)
  std::unique_ptr<media::VideoDecodeAccelerator> CreateVTVDA(
      const gpu::GpuPreferences& gpu_preferences) const;
#endif
#if defined(OS_ANDROID)
  std::unique_ptr<media::VideoDecodeAccelerator> CreateAndroidVDA(
      const gpu::GpuPreferences& gpu_preferences) const;
#endif

  const GetGLContextCallback get_gl_context_cb_;
  const MakeGLContextCurrentCallback make_context_current_cb_;
  const BindGLImageCallback bind_image_cb_;
  const GetGLES2DecoderCallback get_gles2_decoder_cb_;

  base::ThreadChecker thread_checker_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(GpuVideoDecodeAcceleratorFactoryImpl);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_GPU_VIDEO_DECODE_ACCELERATOR_FACTORY_IMPL_H_
