// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_factory.h"

#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/image_factory.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/config/gpu_preferences.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_bindings.h"

namespace gpu {

SharedImageFactory::SharedImageFactory(
    const GpuPreferences& gpu_preferences,
    const GpuDriverBugWorkarounds& workarounds,
    const GpuFeatureInfo& gpu_feature_info,
    MailboxManager* mailbox_manager,
    ImageFactory* image_factory,
    gles2::MemoryTracker* tracker)
    : use_passthrough_(gpu_preferences.use_passthrough_cmd_decoder &&
                       gles2::PassthroughCommandDecoderSupported()),
      mailbox_manager_(mailbox_manager),
      image_factory_(image_factory),
      memory_tracker_(std::make_unique<gles2::MemoryTypeTracker>(tracker)) {
  gl::GLApi* api = gl::g_current_gl_context;
  api->glGetIntegervFn(GL_MAX_TEXTURE_SIZE, &max_texture_size_);
  if (workarounds.max_texture_size) {
    max_texture_size_ =
        std::min(max_texture_size_, workarounds.max_texture_size);
  }

  // TODO(piman): Can we extract the logic out of FeatureInfo?
  scoped_refptr<gles2::FeatureInfo> feature_info =
      new gles2::FeatureInfo(workarounds, gpu_feature_info);
  feature_info->Initialize(ContextType::CONTEXT_TYPE_OPENGLES2,
                           use_passthrough_, gles2::DisallowedFeatures());
  texture_usage_angle_ = feature_info->feature_flags().angle_texture_usage;
  es3_capable_ = feature_info->IsES3Capable();
  bool enable_texture_storage =
      feature_info->feature_flags().ext_texture_storage;
  bool enable_scanout_images =
      (image_factory_ && image_factory_->SupportsCreateAnonymousImage());
  const gles2::Validators* validators = feature_info->validators();
  for (int i = 0; i <= viz::RESOURCE_FORMAT_MAX; ++i) {
    auto format = static_cast<viz::ResourceFormat>(i);
    FormatInfo& info = format_info_[i];
    // TODO(piman): do we need to support ETC1?
    if (!viz::GLSupportsFormat(format) ||
        viz::IsResourceFormatCompressed(format))
      continue;
    if (enable_texture_storage) {
      GLuint internal_format = viz::TextureStorageFormat(format);
      if (validators->texture_internal_format_storage.IsValid(
              internal_format)) {
        info.enabled = true;
        info.use_storage = true;
        info.internal_format = internal_format;
        info.gl_format = gles2::TextureManager::ExtractFormatFromStorageFormat(
            info.internal_format);
        info.gl_type = gles2::TextureManager::ExtractTypeFromStorageFormat(
            internal_format);
        info.swizzle = gles2::TextureManager::GetCompatibilitySwizzle(
            feature_info.get(), info.gl_format);
        info.adjusted_internal_format =
            gles2::TextureManager::AdjustTexStorageFormat(feature_info.get(),
                                                          internal_format);
      }
    }
    if (!info.enabled) {
      GLuint internal_format = viz::GLInternalFormat(format);
      GLenum gl_format = viz::GLDataFormat(format);
      GLenum gl_type = viz::GLDataType(format);
      if (validators->texture_internal_format.IsValid(internal_format) &&
          validators->texture_format.IsValid(gl_format) &&
          validators->pixel_type.IsValid(gl_type)) {
        info.enabled = true;
        info.use_storage = false;
        info.internal_format = internal_format;
        info.gl_format = gl_format;
        info.gl_type = gl_type;
        info.swizzle = gles2::TextureManager::GetCompatibilitySwizzle(
            feature_info.get(), gl_format);
        info.adjusted_internal_format =
                gles2::TextureManager::AdjustTexInternalFormat(
                    feature_info.get(), internal_format);
        info.adjusted_format = gles2::TextureManager::AdjustTexFormat(
            feature_info.get(), gl_format);
      }
    }
    if (!info.enabled || !enable_scanout_images)
      continue;
    gfx::BufferFormat buffer_format = viz::BufferFormat(format);
    switch (buffer_format) {
      case gfx::BufferFormat::RGBA_8888:
      case gfx::BufferFormat::BGRA_8888:
      case gfx::BufferFormat::RGBA_F16:
      case gfx::BufferFormat::R_8:
        break;
      default:
        continue;
    }
    info.allow_scanout = true;
    info.buffer_format = buffer_format;
    if (base::ContainsValue(
        gpu_preferences.texture_target_exception_list,
        gfx::BufferUsageAndFormat(gfx::BufferUsage::SCANOUT, buffer_format)))
      info.target_for_scanout = gpu::GetPlatformSpecificTextureTarget();
  }
}

SharedImageFactory::~SharedImageFactory() {
  DCHECK(mailboxes_.empty());
  DCHECK(passthrough_textures_.empty());
}

bool SharedImageFactory::CreateSharedImage(const Mailbox& mailbox,
                                           viz::ResourceFormat format,
                                           const gfx::Size& size,
                                           const gfx::ColorSpace& color_space,
                                           uint32_t usage) {
  if (mailboxes_.find(mailbox) != mailboxes_.end()) {
    LOG(ERROR) << "CreateSharedImage: mailbox already exists";
    return false;
  }
  const FormatInfo& format_info = format_info_[format];
  if (!format_info.enabled) {
    LOG(ERROR) << "CreateSharedImage: invalid format";
    return false;
  }

  const bool use_buffer = usage & SHARED_IMAGE_USAGE_SCANOUT;
  if (use_buffer && !format_info.allow_scanout) {
    LOG(ERROR) << "CreateSharedImage: SCANOUT shared images unavailable";
    return false;
  }

  if (size.width() < 1 || size.height() < 1 ||
      size.width() > max_texture_size_ || size.height() > max_texture_size_) {
    LOG(ERROR) << "CreateSharedImage: invalid size";
    return false;
  }

  GLenum target = use_buffer ? GL_TEXTURE_2D : format_info.target_for_scanout;

  GLenum get_target = GL_TEXTURE_BINDING_2D;
  switch (target) {
    case GL_TEXTURE_2D:
      get_target = GL_TEXTURE_BINDING_2D;
      break;
    case GL_TEXTURE_RECTANGLE_ARB:
      get_target = GL_TEXTURE_BINDING_RECTANGLE_ARB;
      break;
    case GL_TEXTURE_EXTERNAL_OES:
      get_target = GL_TEXTURE_BINDING_EXTERNAL_OES;
      break;
    default:
      NOTREACHED();
      break;
  }

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

  const bool for_framebuffer_attachment =
      (usage & (SHARED_IMAGE_USAGE_RASTER |
                SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT)) != 0;
  if (for_framebuffer_attachment && texture_usage_angle_) {
    api->glTexParameteriFn(target, GL_TEXTURE_USAGE_ANGLE,
                           GL_FRAMEBUFFER_ATTACHMENT_ANGLE);
  }

  gfx::Rect cleared_rect;
  scoped_refptr<gl::GLImage> image;
  GLuint internal_format = format_info.internal_format;
  if (use_buffer) {
    bool is_cleared = false;
    image = image_factory_->CreateAnonymousImage(
        size, format_info.buffer_format, gfx::BufferUsage::SCANOUT,
        format_info.gl_format, &is_cleared);
    if (!image || !image->BindTexImage(target)) {
      LOG(ERROR) << "CreateSharedImage: Failed to create image";
      api->glBindTextureFn(target, old_texture_binding);
      api->glDeleteTexturesFn(1, &service_id);
      return false;
    }
    if (is_cleared)
      cleared_rect = gfx::Rect(size);
    internal_format = image->GetInternalFormat();
    if (color_space.IsValid())
      image->SetColorSpace(color_space);
  } else if (format_info.use_storage) {
    api->glTexStorage2DEXTFn(target, 1, format_info.adjusted_internal_format,
                             size.width(), size.height());
  } else {
    // Need to unbind any GL_PIXEL_UNPACK_BUFFER for the nullptr in glTexImage2D
    // to mean "no pixels" (as opposed to offset 0 in the buffer).
    GLint bound_pixel_unpack_buffer = 0;
    if (es3_capable_) {
      api->glGetIntegervFn(GL_PIXEL_UNPACK_BUFFER_BINDING,
                           &bound_pixel_unpack_buffer);
      if (bound_pixel_unpack_buffer)
        api->glBindBufferFn(GL_PIXEL_UNPACK_BUFFER, 0);
    }
    api->glTexImage2DFn(target, 0, format_info.adjusted_internal_format,
                        size.width(), size.height(), 0,
                        format_info.adjusted_format, format_info.gl_type,
                        nullptr);
    if (bound_pixel_unpack_buffer)
      api->glBindBufferFn(GL_PIXEL_UNPACK_BUFFER, bound_pixel_unpack_buffer);
  }

  if (use_passthrough_) {
    auto texture =
        base::MakeRefCounted<gles2::TexturePassthrough>(service_id, target);
    if (image)
      texture->SetLevelImage(target, 0, image.get());
    mailbox_manager_->ProduceTexture(mailbox, texture.get());
    passthrough_textures_.insert(std::move(texture));
  } else {
    gles2::Texture* texture = new gles2::Texture(service_id);
    texture->SetLightweightRef(memory_tracker_.get());
    texture->SetTarget(target, 1);
    texture->SetLevelInfo(target, 0, internal_format, size.width(),
                          size.height(), 1, 0, format_info.gl_format,
                          format_info.gl_type, cleared_rect);
    if (format_info.swizzle)
      texture->SetCompatibilitySwizzle(format_info.swizzle);
    if (image)
      texture->SetLevelImage(target, 0, image.get(), gles2::Texture::BOUND);
    texture->SetImmutable(true);
    mailbox_manager_->ProduceTexture(mailbox, texture);
  }

  api->glBindTextureFn(target, old_texture_binding);

  mailboxes_.insert(mailbox);
  return true;
}

bool SharedImageFactory::DestroySharedImage(const Mailbox& mailbox) {
  auto it = mailboxes_.find(mailbox);
  if (it == mailboxes_.end()) {
    LOG(ERROR) << "Could not find shared image mailbox";
    return false;
  }
  TextureBase* texture_base = mailbox_manager_->ConsumeTexture(mailbox);
  DCHECK(texture_base);
  if (use_passthrough_) {
    scoped_refptr<gles2::TexturePassthrough> texture(
        static_cast<gles2::TexturePassthrough*>(texture_base));
    size_t count = passthrough_textures_.erase(texture);
    DCHECK_EQ(count, 1u);
  } else {
    auto* texture = static_cast<gles2::Texture*>(texture_base);
    texture->RemoveLightweightRef(true);
    // Texture may have been deleted at this point.
  }
  mailboxes_.erase(it);
  return true;
}

void SharedImageFactory::DestroyAllSharedImages(bool have_context) {
  if (use_passthrough_) {
    if (!have_context) {
      for (auto& texture : passthrough_textures_)
        texture->MarkContextLost();
    }
    passthrough_textures_.clear();
  } else {
    for (const auto& mailbox : mailboxes_) {
      auto* texture = static_cast<gles2::Texture*>(
          mailbox_manager_->ConsumeTexture(mailbox));
      DCHECK(texture);
      texture->RemoveLightweightRef(have_context);
    }
  }
  mailboxes_.clear();
}

SharedImageFactory::FormatInfo::FormatInfo() = default;
SharedImageFactory::FormatInfo::~FormatInfo() = default;

}  // namespace gpu
