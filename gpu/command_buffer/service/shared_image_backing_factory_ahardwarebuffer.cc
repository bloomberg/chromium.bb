// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_backing_factory_ahardwarebuffer.h"

#include "base/android/android_hardware_buffer_compat.h"
#include "base/android/scoped_hardware_buffer_handle.h"
#include "base/logging.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/ahardwarebuffer_utils.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_image_ahardwarebuffer.h"
#include "ui/gl/gl_version_info.h"

namespace gpu {

// Representation of a SharedImageBackingAHB as a GL Texture.
class SharedImageRepresentationGLTextureAHB
    : public SharedImageRepresentationGLTexture {
 public:
  SharedImageRepresentationGLTextureAHB(SharedImageManager* manager,
                                        SharedImageBacking* backing,
                                        MemoryTypeTracker* tracker,
                                        gles2::Texture* texture)
      : SharedImageRepresentationGLTexture(manager, backing, tracker),
        texture_(texture) {}

  gles2::Texture* GetTexture() override { return texture_; }

 private:
  gles2::Texture* texture_;

  DISALLOW_COPY_AND_ASSIGN(SharedImageRepresentationGLTextureAHB);
};

// GL backed Skia representation of SharedImageBackingAHB.
// TODO(vikassoni): Add follow up patch to add a vulkan backed skia
// representation.
class SharedImageRepresentationSkiaGLAHB
    : public SharedImageRepresentationSkia {
 public:
  SharedImageRepresentationSkiaGLAHB(SharedImageManager* manager,
                                     SharedImageBacking* backing,
                                     MemoryTypeTracker* tracker,
                                     GLenum target,
                                     GLenum internal_format,
                                     GLenum driver_internal_format,
                                     GLuint service_id)
      : SharedImageRepresentationSkia(manager, backing, tracker),
        target_(target),
        internal_format_(internal_format),
        driver_internal_format_(driver_internal_format),
        service_id_(service_id) {}

  ~SharedImageRepresentationSkiaGLAHB() override { DCHECK(!write_surface_); }

  sk_sp<SkSurface> BeginWriteAccess(
      GrContext* gr_context,
      int final_msaa_count,
      SkColorType color_type,
      const SkSurfaceProps& surface_props) override {
    if (write_surface_)
      return nullptr;

    GrBackendTexture backend_texture;
    if (!GetGrBackendTexture(target_, size(), internal_format_,
                             driver_internal_format_, service_id_, color_type,
                             &backend_texture)) {
      return nullptr;
    }
    auto surface = SkSurface::MakeFromBackendTextureAsRenderTarget(
        gr_context, backend_texture, kTopLeft_GrSurfaceOrigin, final_msaa_count,
        color_type, nullptr, &surface_props);
    write_surface_ = surface.get();
    return surface;
  }

  void EndWriteAccess(sk_sp<SkSurface> surface) override {
    DCHECK_EQ(surface.get(), write_surface_);
    DCHECK(surface->unique());
    // TODO(ericrk): Keep the surface around for re-use.
    write_surface_ = nullptr;
  }

  bool BeginReadAccess(SkColorType color_type,
                       GrBackendTexture* backend_texture) override {
    if (!GetGrBackendTexture(target_, size(), internal_format_,
                             driver_internal_format_, service_id_, color_type,
                             backend_texture)) {
      return false;
    }
    return true;
  }

  void EndReadAccess() override {
    // TODO(ericrk): Handle begin/end correctness checks.
  }

 private:
  GLenum target_;
  GLenum internal_format_ = 0;
  GLenum driver_internal_format_ = 0;
  GLuint service_id_;

  SkSurface* write_surface_ = nullptr;
};

// Implementation of SharedImageBacking that holds an AHardwareBuffer. This
// can be used to create a GL texture or a VK Image from the AHardwareBuffer
// backing.
class SharedImageBackingAHB : public SharedImageBacking {
 public:
  SharedImageBackingAHB(const Mailbox& mailbox,
                        viz::ResourceFormat format,
                        const gfx::Size& size,
                        const gfx::ColorSpace& color_space,
                        uint32_t usage,
                        base::android::ScopedHardwareBufferHandle handle,
                        size_t estimated_size)
      : SharedImageBacking(mailbox,
                           format,
                           size,
                           color_space,
                           usage,
                           estimated_size),
        hardware_buffer_handle_(std::move(handle)) {
    DCHECK(hardware_buffer_handle_.is_valid());
  }

  ~SharedImageBackingAHB() override {
    // Check to make sure buffer is explicitly destroyed using Destroy() api
    // before this destructor is called.
    DCHECK(!hardware_buffer_handle_.is_valid());
    DCHECK(!texture_);
  }

  bool IsCleared() const override {
    if (texture_)
      return texture_->IsLevelCleared(texture_->target(), 0);
    return is_cleared_;
  }

  void SetCleared() override {
    if (texture_)
      texture_->SetLevelCleared(texture_->target(), 0, true);
    is_cleared_ = true;
  }

  void Update() override {}

  bool ProduceLegacyMailbox(MailboxManager* mailbox_manager) override {
    DCHECK(hardware_buffer_handle_.is_valid());
    if (!GenGLTexture())
      return false;
    DCHECK(texture_);
    mailbox_manager->ProduceTexture(mailbox(), texture_);
    return true;
  }

  void Destroy() override {
    DCHECK(hardware_buffer_handle_.is_valid());
    if (texture_) {
      texture_->RemoveLightweightRef(have_context());
      texture_ = nullptr;
    }
    hardware_buffer_handle_.reset();
  }

 protected:
  std::unique_ptr<SharedImageRepresentationGLTexture> ProduceGLTexture(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override {
    // Use same texture for all the texture representations generated from same
    // backing.
    if (!GenGLTexture())
      return nullptr;

    DCHECK(texture_);
    return std::make_unique<SharedImageRepresentationGLTextureAHB>(
        manager, this, tracker, texture_);
  }

  std::unique_ptr<SharedImageRepresentationSkia> ProduceSkia(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override {
    // TODO(vikassoni): Currently we only have a GL backed skia representation.
    // Follow up patch will add support to check whether we are in Vulkan mode
    // OR  GL mode and accordingly create Skia representation.
    if (!GenGLTexture())
      return nullptr;

    DCHECK(texture_);
    return std::make_unique<SharedImageRepresentationSkiaGLAHB>(
        manager, this, tracker, texture_->target(), internal_format_,
        driver_internal_format_, texture_->service_id());
  }

 private:
  bool GenGLTexture() {
    if (texture_)
      return true;

    DCHECK(hardware_buffer_handle_.is_valid());

    // Target for AHB backed egl images.
    // Note that we are not using GL_TEXTURE_EXTERNAL_OES target since sksurface
    // doesnt supports it. As per the egl documentation -
    // https://www.khronos.org/registry/OpenGL/extensions/OES/OES_EGL_image_external.txt
    // if GL_OES_EGL_image is supported then <target> may also be TEXTURE_2D.
    GLenum target = GL_TEXTURE_2D;
    GLenum get_target = GL_TEXTURE_BINDING_2D;

    // Create a gles2 texture using the AhardwareBuffer.
    gl::GLApi* api = gl::g_current_gl_context;
    GLuint service_id = 0;
    api->glGenTexturesFn(1, &service_id);
    GLint old_texture_binding = 0;
    api->glGetIntegervFn(get_target, &old_texture_binding);
    api->glBindTextureFn(target, service_id);
    api->glTexParameteriFn(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    api->glTexParameteriFn(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    api->glTexParameteriFn(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    api->glTexParameteriFn(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Create an egl image using AHardwareBuffer.
    auto egl_image = base::MakeRefCounted<gl::GLImageAHardwareBuffer>(size());
    if (!egl_image->Initialize(hardware_buffer_handle_.get(), false)) {
      LOG(ERROR) << "Failed to create EGL image ";
      api->glBindTextureFn(target, old_texture_binding);
      api->glDeleteTexturesFn(1, &service_id);
      return false;
    }
    if (!egl_image->BindTexImage(target)) {
      LOG(ERROR) << "Failed to bind egl image";
      api->glBindTextureFn(target, old_texture_binding);
      api->glDeleteTexturesFn(1, &service_id);
      return false;
    }

    // Create a gles2 Texture.
    texture_ = new gles2::Texture(service_id);
    texture_->SetLightweightRef();
    texture_->SetTarget(target, 1);
    texture_->sampler_state_.min_filter = GL_LINEAR;
    texture_->sampler_state_.mag_filter = GL_LINEAR;
    texture_->sampler_state_.wrap_t = GL_CLAMP_TO_EDGE;
    texture_->sampler_state_.wrap_s = GL_CLAMP_TO_EDGE;

    // If the backing is already cleared, no need to clear it again.
    gfx::Rect cleared_rect;
    if (is_cleared_)
      cleared_rect = gfx::Rect(size());

    GLenum gl_format = viz::GLDataFormat(format());
    GLenum gl_type = viz::GLDataType(format());
    texture_->SetLevelInfo(target, 0, egl_image->GetInternalFormat(),
                           size().width(), size().height(), 1, 0, gl_format,
                           gl_type, cleared_rect);
    texture_->SetLevelImage(target, 0, egl_image.get(), gles2::Texture::BOUND);
    texture_->SetImmutable(true);
    api->glBindTextureFn(target, old_texture_binding);
    internal_format_ = egl_image->GetInternalFormat();
    driver_internal_format_ = gl::GetInternalFormat(
        gl::GLContext::GetCurrent()->GetVersionInfo(), internal_format_);
    return true;
  }

  base::android::ScopedHardwareBufferHandle hardware_buffer_handle_;

  // This texture will be lazily initialised/created when ProduceGLTexture is
  // called.
  gles2::Texture* texture_ = nullptr;
  GLenum internal_format_ = 0;
  GLenum driver_internal_format_ = 0;

  // TODO(vikassoni): In future when we add begin/end write support, we will
  // need to properly use this flag to pass the is_cleared_ information to
  // the GL texture representation while begin write and back to this class from
  // the GL texture represntation after end write. This is because this class
  // will not know if SetCleared() arrives during begin write happening on GL
  // texture representation.
  bool is_cleared_ = false;

  DISALLOW_COPY_AND_ASSIGN(SharedImageBackingAHB);
};

SharedImageBackingFactoryAHB::SharedImageBackingFactoryAHB(
    const GpuDriverBugWorkarounds& workarounds,
    const GpuFeatureInfo& gpu_feature_info) {
  scoped_refptr<gles2::FeatureInfo> feature_info =
      new gles2::FeatureInfo(workarounds, gpu_feature_info);
  feature_info->Initialize(ContextType::CONTEXT_TYPE_OPENGLES2, false,
                           gles2::DisallowedFeatures());
  const gles2::Validators* validators = feature_info->validators();
  const bool is_egl_image_supported =
      gl::g_current_gl_driver->ext.b_GL_OES_EGL_image;

  // Build the feature info for all the resource formats.
  for (int i = 0; i <= viz::RESOURCE_FORMAT_MAX; ++i) {
    auto format = static_cast<viz::ResourceFormat>(i);
    FormatInfo& info = format_info_[i];

    // If AHB does not support this format, we will not be able to create this
    // backing.
    if (!AHardwareBufferSupportedFormat(format))
      continue;

    info.ahb_supported = true;
    info.ahb_format = AHardwareBufferFormat(format);

    // TODO(vikassoni): In future when we use GL_TEXTURE_EXTERNAL_OES target
    // with AHB, we need to check if oes_egl_image_external is supported or not.
    if (!is_egl_image_supported)
      continue;

    // Check if AHB backed GL texture can be created using this format and
    // gather GL related format info.
    // TODO(vikassoni): Add vulkan related information in future.
    GLuint internal_format = viz::GLInternalFormat(format);
    GLenum gl_format = viz::GLDataFormat(format);
    GLenum gl_type = viz::GLDataType(format);

    //  GLImageAHardwareBuffer currently supports internal format GL_RGBA only.
    //  TODO(vikassoni): Pass the AHBuffer format while GLImageAHardwareBuffer
    //  creation and based on that return the equivalent internal format as
    //  GL_RGBA or GL_RGB.
    if (internal_format != GL_RGBA)
      continue;

    // Validate if GL format, type and internal format is supported.
    if (validators->texture_internal_format.IsValid(internal_format) &&
        validators->texture_format.IsValid(gl_format) &&
        validators->pixel_type.IsValid(gl_type)) {
      info.gl_supported = true;
      info.gl_format = gl_format;
      info.gl_type = gl_type;
      info.internal_format = internal_format;
    }
  }
  // TODO(vikassoni): We are using below GL api calls for now as Vulkan mode
  // doesn't exist. Once we have vulkan support, we shouldn't query GL in this
  // code until we are asked to make a GL representation (or allocate a backing
  // for import into GL)? We may use an AHardwareBuffer exclusively with Vulkan,
  // where there is no need to require that a GL context is current. Maybe we
  // can lazy init this if someone tries to create an AHardwareBuffer with
  // SHARED_IMAGE_USAGE_GLES2 || !gpu_preferences.enable_vulkan. When in Vulkan
  // mode, we should only need this with GLES2.
  gl::GLApi* api = gl::g_current_gl_context;
  api->glGetIntegervFn(GL_MAX_TEXTURE_SIZE, &max_gl_texture_size_);

  // TODO(vikassoni): Check vulkan image size restrictions also.
  if (workarounds.max_texture_size) {
    max_gl_texture_size_ =
        std::min(max_gl_texture_size_, workarounds.max_texture_size);
  }
}

SharedImageBackingFactoryAHB::~SharedImageBackingFactoryAHB() = default;

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactoryAHB::CreateSharedImage(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage) {
  DCHECK(base::AndroidHardwareBufferCompat::IsSupportAvailable());

  const FormatInfo& format_info = format_info_[format];

  // Check if the format is supported by AHardwareBuffer.
  if (!format_info.ahb_supported) {
    LOG(ERROR) << "viz::ResourceFormat " << format
               << " not supported by AHardwareBuffer";
    return nullptr;
  }

  // SHARED_IMAGE_USAGE_RASTER is set when we want to write on Skia
  // representation and SHARED_IMAGE_USAGE_DISPLAY is used for cases we want to
  // read from skia representation.
  // TODO(vikassoni): Also check gpu_preferences.enable_vulkan to figure out if
  // skia is using vulkan backing or GL backing.
  const bool use_gles2 =
      (usage & (SHARED_IMAGE_USAGE_GLES2 | SHARED_IMAGE_USAGE_RASTER |
                SHARED_IMAGE_USAGE_DISPLAY));

  // If usage flags indicated this backing can be used as a GL texture, then do
  // below gl related checks.
  if (use_gles2) {
    // Check if the GL texture can be created from AHB with this format.
    if (!format_info.gl_supported) {
      LOG(ERROR)
          << "viz::ResourceFormat " << format
          << " can not be used to create a GL texture from AHardwareBuffer.";
      return nullptr;
    }
  }

  // Check if AHB can be created with the current size restrictions.
  // TODO(vikassoni): Check for VK size restrictions for VK import, GL size
  // restrictions for GL import OR both if this backing is needed to be used
  // with both GL and VK.
  if (size.width() < 1 || size.height() < 1 ||
      size.width() > max_gl_texture_size_ ||
      size.height() > max_gl_texture_size_) {
    LOG(ERROR) << "CreateSharedImage: invalid size";
    return nullptr;
  }

  // Calculate SharedImage size in bytes.
  size_t estimated_size;
  if (!viz::ResourceSizes::MaybeSizeInBytes(size, format, &estimated_size)) {
    LOG(ERROR) << "Failed to calculate SharedImage size";
    return nullptr;
  }

  // Setup AHardwareBuffer.
  AHardwareBuffer* buffer = nullptr;
  AHardwareBuffer_Desc hwb_desc;
  hwb_desc.width = size.width();
  hwb_desc.height = size.height();
  hwb_desc.format = format_info.ahb_format;

  // Set usage so that gpu can both read as a texture/write as a framebuffer
  // attachment. TODO(vikassoni): Find out if we need to set some more usage
  // flags based on the usage params in the current function call.
  hwb_desc.usage = AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE |
                   AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT;

  // Number of images in an image array.
  hwb_desc.layers = 1;

  // The following three are not used here.
  hwb_desc.stride = 0;
  hwb_desc.rfu0 = 0;
  hwb_desc.rfu1 = 0;

  // Allocate an AHardwareBuffer.
  base::AndroidHardwareBufferCompat::GetInstance().Allocate(&hwb_desc, &buffer);
  if (!buffer) {
    LOG(ERROR) << "Failed to allocate AHardwareBuffer";
    return nullptr;
  }

  auto backing = std::make_unique<SharedImageBackingAHB>(
      mailbox, format, size, color_space, usage,
      base::android::ScopedHardwareBufferHandle::Adopt(buffer), estimated_size);
  return backing;
}

SharedImageBackingFactoryAHB::FormatInfo::FormatInfo() = default;
SharedImageBackingFactoryAHB::FormatInfo::~FormatInfo() = default;

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactoryAHB::CreateSharedImage(
    const Mailbox& mailbox,
    int client_id,
    gfx::GpuMemoryBufferHandle handle,
    gfx::BufferFormat buffer_format,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage) {
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace gpu
