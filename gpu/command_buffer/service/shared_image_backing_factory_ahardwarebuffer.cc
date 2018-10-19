// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_backing_factory_ahardwarebuffer.h"

#include "base/android/android_hardware_buffer_compat.h"
#include "base/android/scoped_hardware_buffer_handle.h"
#include "base/logging.h"
#include "gpu/command_buffer/service/ahardwarebuffer_utils.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_bindings.h"

namespace gpu {

// Implementation of SharedImageBacking that holds an AHardwareBuffer. This
// can be used to create a GL texture or a VK Image from the AHardwareBuffer
// backing.
class SharedImageBackingAHardwareBuffer : public SharedImageBacking {
 public:
  SharedImageBackingAHardwareBuffer(
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      uint32_t usage,
      base::android::ScopedHardwareBufferHandle handle)
      : SharedImageBacking(mailbox, format, size, color_space, usage),
        hardware_buffer_handle_(std::move(handle)) {
    DCHECK(hardware_buffer_handle_.is_valid());
  }

  ~SharedImageBackingAHardwareBuffer() override {
    // Check to make sure buffer is explicitly destroyed using Destroy() api
    // before this destructor is called.
    DCHECK(!hardware_buffer_handle_.is_valid());
  }

  bool IsCleared() const override { return is_cleared_; }

  void SetCleared() override { is_cleared_ = true; }

  bool ProduceLegacyMailbox(MailboxManager* mailbox_manager) override {
    DCHECK(hardware_buffer_handle_.is_valid());

    // Legacy mailbox is not supported.
    return false;
  }

  void Destroy() override {
    DCHECK(hardware_buffer_handle_.is_valid());
    hardware_buffer_handle_.reset();
  }

 private:
  base::android::ScopedHardwareBufferHandle hardware_buffer_handle_;

  // TODO(vikassoni): In future when we add begin/end write support, we will
  // need to properly use this flag to pass the is_cleared_ information to
  // the GL texture representation while begin write and back to this class from
  // the GL texture represntation after end write. This is because this class
  // will not know if SetCleared() arrives during begin write happening on GL
  // texture representation.
  bool is_cleared_ = false;

  DISALLOW_COPY_AND_ASSIGN(SharedImageBackingAHardwareBuffer);
};

SharedImageBackingFactoryAHardwareBuffer::
    SharedImageBackingFactoryAHardwareBuffer(
        const GpuDriverBugWorkarounds& workarounds) {
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

SharedImageBackingFactoryAHardwareBuffer::
    ~SharedImageBackingFactoryAHardwareBuffer() = default;

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactoryAHardwareBuffer::CreateSharedImage(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage) {
  DCHECK(base::AndroidHardwareBufferCompat::IsSupportAvailable());

  // Check if the format is supported by AHardwareBuffer.
  // TODO(vikassoni): We need to check if the equivalent GL texture format
  // and VK image format support is available in the underlying hardware. Else
  // we might not be able to use this AHB as a gl texture or VK Image.
  // Additionally we also need to check usage flags to determine the format.
  if (!AHardwareBufferSupportedFormat(format)) {
    LOG(ERROR) << "viz::ResourceFormat " << format
               << "not supported by AHardwareBuffer";
    return nullptr;
  }

  // Check for size restrictions.
  // TODO(vikassoni): Check for VK size restrictions for VK import, GL size
  // restrictions for GL import OR both if this backing is needed to be used
  // with both GL and VK.
  if (size.width() < 1 || size.height() < 1 ||
      size.width() > max_gl_texture_size_ ||
      size.height() > max_gl_texture_size_) {
    LOG(ERROR) << "CreateSharedImage: invalid size";
    return nullptr;
  }

  // Setup AHardwareBuffer.
  AHardwareBuffer* buffer = nullptr;
  AHardwareBuffer_Desc hwb_desc;
  hwb_desc.width = size.width();
  hwb_desc.height = size.height();
  hwb_desc.format = AHardwareBufferFormat(format);

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

  auto backing = std::make_unique<SharedImageBackingAHardwareBuffer>(
      mailbox, format, size, color_space, usage,
      base::android::ScopedHardwareBufferHandle::Adopt(buffer));
  return backing;
}

}  // namespace gpu
