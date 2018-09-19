// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_FACTORY_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_FACTORY_H_

#include <memory>

#include "base/containers/flat_set.h"
#include "base/memory/scoped_refptr.h"
#include "components/viz/common/resources/resource_format.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/gpu_gles2_export.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gl/gl_bindings.h"

namespace gfx {
class Size;
class ColorSpace;
}  // namespace gfx

namespace gpu {

class GpuDriverBugWorkarounds;
struct GpuFeatureInfo;
struct GpuPreferences;
class ImageFactory;

class MailboxManager;

namespace gles2 {
class MemoryTracker;
class MemoryTypeTracker;
class TexturePassthrough;
};  // namespace gles2

class GPU_GLES2_EXPORT SharedImageFactory {
 public:
  SharedImageFactory(const GpuPreferences& gpu_preferences,
                     const GpuDriverBugWorkarounds& workarounds,
                     const GpuFeatureInfo& gpu_feature_info,
                     MailboxManager* mailbox_manager,
                     ImageFactory* image_factory,
                     gles2::MemoryTracker* tracker);
  ~SharedImageFactory();

  bool CreateSharedImage(const Mailbox& mailbox,
                         viz::ResourceFormat format,
                         const gfx::Size& size,
                         const gfx::ColorSpace& color_space,
                         uint32_t usage);
  bool DestroySharedImage(const Mailbox& mailbox);
  bool HasImages() const { return !mailboxes_.empty(); }
  void DestroyAllSharedImages(bool have_context);
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd,
                    int client_id,
                    uint64_t client_tracing_id);

 private:
  struct FormatInfo {
    FormatInfo();
    ~FormatInfo();

    // Whether this format is supported.
    bool enabled = false;

    // Whether to use glTexStorage2D or glTexImage2D.
    bool use_storage = false;

    // Whether to allow SHARED_IMAGE_USAGE_SCANOUT.
    bool allow_scanout = false;

    // GL internal_format/format/type triplet.
    GLuint internal_format = 0;
    GLenum gl_format = 0;
    GLenum gl_type = 0;

    const gles2::Texture::CompatibilitySwizzle* swizzle = nullptr;
    GLuint adjusted_internal_format = 0;
    GLenum adjusted_format = 0;

    // GL target to use for scanout images.
    GLenum target_for_scanout = GL_TEXTURE_2D;

    // BufferFormat for scanout images.
    gfx::BufferFormat buffer_format = gfx::BufferFormat::RGBA_8888;
  };

  bool use_passthrough_;
  MailboxManager* mailbox_manager_;
  ImageFactory* image_factory_;
  std::unique_ptr<gles2::MemoryTypeTracker> memory_tracker_;
  FormatInfo format_info_[viz::RESOURCE_FORMAT_MAX + 1];

  int32_t max_texture_size_ = 0;
  bool texture_usage_angle_ = false;
  bool es3_capable_ = false;

  // Mailboxes that have been created by the SharedImageFactory. If not using
  // passthrough, the textures pointed to by these mailboxes are kept alive with
  // a lightweight ref.
  base::flat_set<Mailbox> mailboxes_;

  // Textures created by the SharedImageFactory, if using passthrough, to keep
  // them alive.
  base::flat_set<scoped_refptr<gles2::TexturePassthrough>>
      passthrough_textures_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_FACTORY_H_
