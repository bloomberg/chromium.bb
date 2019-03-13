// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_backing_factory_ahardwarebuffer.h"

#include <sync/sync.h>
#include <unistd.h>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/android/android_hardware_buffer_compat.h"
#include "base/android/scoped_hardware_buffer_handle.h"
#include "base/logging.h"
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/ahardwarebuffer_utils.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/ipc/common/android/android_image_reader_utils.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_fence_android_native_fence_sync.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_image_ahardwarebuffer.h"
#include "ui/gl/gl_version_info.h"

namespace gpu {
namespace {

bool InsertEglWriteFence(base::ScopedFD write_sync_fd) {
  if (write_sync_fd.is_valid()) {
    if (!InsertEglFenceAndWait(std::move(write_sync_fd)))
      return false;
  }

  return true;
}

bool InsertAllEglFencesForWriting(base::ScopedFD write_sync_fd,
                                  std::vector<base::ScopedFD> read_sync_fds) {
  if (!InsertEglWriteFence(std::move(write_sync_fd)))
    return false;

  // Insert read fences
  for (auto& read_sync_fd : read_sync_fds) {
    if (!InsertEglFenceAndWait(std::move(read_sync_fd)))
      return false;
  }

  return true;
}

bool InsertAllVulkanFencesForWriting(
    std::vector<base::ScopedFD> sync_fds,
    gpu::VulkanImplementation* vk_implementation,
    VkDevice vk_device,
    VkQueue vk_queue,
    std::vector<VkSemaphore>* semaphores) {
  DCHECK_EQ(size_t{0}, semaphores->size());

  semaphores->resize(sync_fds.size(), VK_NULL_HANDLE);

  for (size_t i = 0; i < sync_fds.size(); ++i) {
    VkSemaphore& semaphore = semaphores->operator[](i);

    if (sync_fds[i].is_valid()) {
      if (!vk_implementation->ImportSemaphoreFdKHR(
              vk_device, std::move(sync_fds[i]), &semaphore))
        return false;

      // Submit wait semaphore to the queue. Note that Skia uses the same queue
      // exposed by vk_queue(), so this will work due to Vulkan queue ordering.
      if (!vk_implementation->SubmitWaitSemaphore(vk_queue, semaphore)) {
        vkDestroySemaphore(vk_device, semaphore, nullptr);
        return false;
      }
    }
  }

  return true;
}

sk_sp<SkPromiseImageTexture> BeginVulkanAccess(
    gpu::VulkanImplementation* vk_implementation,
    VkDevice vk_device,
    VkPhysicalDevice vk_physical_device,
    VkQueue vk_queue,
    base::android::ScopedHardwareBufferHandle ahb_handle,
    gfx::Size size,
    viz::ResourceFormat format,
    std::vector<VkSemaphore> semaphores_to_clean) {
  // Create a VkImage and import AHB.
  VkImage vk_image;
  VkImageCreateInfo vk_image_info;
  VkDeviceMemory vk_device_memory;
  VkDeviceSize mem_allocation_size;
  if (!vk_implementation->CreateVkImageAndImportAHB(
          vk_device, vk_physical_device, size, std::move(ahb_handle), &vk_image,
          &vk_image_info, &vk_device_memory, &mem_allocation_size)) {
    return nullptr;
  }

  // Create backend texture from the VkImage.
  GrVkAlloc alloc = {vk_device_memory, 0, mem_allocation_size, 0};
  GrVkImageInfo vk_info = {vk_image,
                           alloc,
                           vk_image_info.tiling,
                           vk_image_info.initialLayout,
                           vk_image_info.format,
                           vk_image_info.mipLevels};
  // TODO(bsalomon): Determine whether it makes sense to attempt to reuse this
  // if the vk_info stays the same on subsequent calls.
  auto promise_texture = SkPromiseImageTexture::Make(
      GrBackendTexture(size.width(), size.height(), vk_info));
  if (!promise_texture) {
    vkDestroyImage(vk_device, vk_image, nullptr);
    vkFreeMemory(vk_device, vk_device_memory, nullptr);
    return nullptr;
  }

  // TODO(vikassoni): Need to do better semaphore cleanup management. Waiting
  // on device to be idle to delete the semaphore is costly. Instead use a
  // fence to get signal when semaphore submission is done.
  if (!semaphores_to_clean.empty()) {
    VkResult result = vkQueueWaitIdle(vk_queue);
    if (result != VK_SUCCESS) {
      LOG(ERROR) << "vkQueueWaitIdle failed: " << result;
      return nullptr;
    }

    for (VkSemaphore& semaphore : semaphores_to_clean) {
      vkDestroySemaphore(vk_device, semaphore, nullptr);
    }
  }

  return promise_texture;
}

void EndVulkanAccess(gpu::VulkanImplementation* vk_implementation,
                     VkDevice vk_device,
                     VkQueue vk_queue,
                     SkSurface** surface,
                     base::ScopedFD* sync_fd) {
  // Create a vk semaphore which can be exported.
  VkExportSemaphoreCreateInfo export_info;
  export_info.sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO;
  export_info.pNext = nullptr;
  export_info.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT;

  VkSemaphore vk_semaphore;
  VkSemaphoreCreateInfo sem_info;
  sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  sem_info.pNext = &export_info;
  sem_info.flags = 0;
  VkResult result =
      vkCreateSemaphore(vk_device, &sem_info, nullptr, &vk_semaphore);
  if (result != VK_SUCCESS) {
    LOG(ERROR) << "vkCreateSemaphore failed";
    (*surface) = nullptr;
    return;
  }
  GrBackendSemaphore gr_semaphore;
  gr_semaphore.initVulkan(vk_semaphore);

  // If GrSemaphoresSubmitted::kNo is returned, the GPU back-end did not
  // create or add any semaphores to signal on the GPU; the caller should not
  // instruct the GPU to wait on any of the semaphores.
  if ((*surface)->flushAndSignalSemaphores(1, &gr_semaphore) ==
      GrSemaphoresSubmitted::kNo) {
    vkDestroySemaphore(vk_device, vk_semaphore, nullptr);
    (*surface) = nullptr;
    return;
  }
  // TODO(ericrk): Keep the surface around for re-use.
  (*surface) = nullptr;

  // Export a sync fd from the semaphore.
  vk_implementation->GetSemaphoreFdKHR(vk_device, vk_semaphore, sync_fd);

  // TODO(vikassoni): We need to wait for the queue submission to complete
  // before we can destroy the semaphore. This will decrease the performance.
  // Add a future patch to handle this in more efficient way. Keep semaphores
  // in a STL queue instead of destroying it. Later use a fence to check if
  // the batch that refers the semaphore has completed execution. Delete the
  // semaphore once the fence is signalled.
  result = vkQueueWaitIdle(vk_queue);
  if (result != VK_SUCCESS) {
    LOG(ERROR) << "vkQueueWaitIdle failed: " << result;
    return;
  }
  vkDestroySemaphore(vk_device, vk_semaphore, nullptr);
}

}  // namespace

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
                        size_t estimated_size,
                        SharedContextState* context_state);

  ~SharedImageBackingAHB() override;

  bool IsCleared() const override;
  void SetCleared() override;
  void Update() override;
  bool ProduceLegacyMailbox(MailboxManager* mailbox_manager) override;
  void Destroy() override;
  SharedContextState* GetContextState() const;
  base::ScopedFD GetWriteSyncFd() const;
  std::vector<base::ScopedFD> GetReadSyncFds() const;
  void ClearReadSyncFds();
  base::android::ScopedHardwareBufferHandle GetAhbHandle();
  void SetWriteSyncFd(base::ScopedFD fd);
  void AddReadSyncFd(base::ScopedFD fd);

 protected:
  std::unique_ptr<SharedImageRepresentationGLTexture> ProduceGLTexture(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override;

  std::unique_ptr<SharedImageRepresentationSkia> ProduceSkia(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override;

 private:
  gles2::Texture* GenGLTexture();
  base::android::ScopedHardwareBufferHandle hardware_buffer_handle_;

  gles2::Texture* legacy_texture_ = nullptr;

  bool is_cleared_ = false;
  SharedContextState* context_state_ = nullptr;

  // All reads and writes must wait for exiting writes to complete.
  base::ScopedFD write_sync_fd_;

  // All writes must wait for existing reads to complete.
  std::vector<base::ScopedFD> read_sync_fds_;

  DISALLOW_COPY_AND_ASSIGN(SharedImageBackingAHB);
};

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

  ~SharedImageRepresentationGLTextureAHB() override {
    if (texture_)
      texture_->RemoveLightweightRef(has_context());
  }

  gles2::Texture* GetTexture() override { return texture_; }

  bool BeginAccess(GLenum mode) override {
    if (mode == GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM) {
      if (!InsertEglWriteFence(ahb_backing()->GetWriteSyncFd()))
        return false;
    } else if (mode == GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM) {
      if (!InsertAllEglFencesForWriting(ahb_backing()->GetWriteSyncFd(),
                                        ahb_backing()->GetReadSyncFds()))
        return false;
    }
    mode_ = mode;
    return true;
  }

  void EndAccess() override {
    base::ScopedFD sync_fd = CreateEglFenceAndExportFd();
    if (!sync_fd.is_valid())
      return;

    // Pass this fd to its backing.
    if (mode_ == GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM) {
      ahb_backing()->AddReadSyncFd(std::move(sync_fd));
    } else if (mode_ == GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM) {
      ahb_backing()->SetWriteSyncFd(std::move(sync_fd));
      ahb_backing()->ClearReadSyncFds();

      if (texture_) {
        if (texture_->IsLevelCleared(texture_->target(), 0))
          backing()->SetCleared();
      }
    }
  }

 private:
  SharedImageBackingAHB* ahb_backing() {
    return static_cast<SharedImageBackingAHB*>(backing());
  }

  gles2::Texture* texture_;
  GLenum mode_ = GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM;

  DISALLOW_COPY_AND_ASSIGN(SharedImageRepresentationGLTextureAHB);
};

// GL backed Skia representation of SharedImageBackingAHB.
class SharedImageRepresentationSkiaGLAHB
    : public SharedImageRepresentationSkia {
 public:
  SharedImageRepresentationSkiaGLAHB(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      sk_sp<SkPromiseImageTexture> cached_promise_image_texture,
      MemoryTypeTracker* tracker,
      gles2::Texture* texture)
      : SharedImageRepresentationSkia(manager, backing, tracker),
        promise_texture_(cached_promise_image_texture),
        texture_(std::move(texture)) {
#if DCHECK_IS_ON()
    context_ = gl::GLContext::GetCurrent();
#endif
  }

  ~SharedImageRepresentationSkiaGLAHB() override {
    DCHECK(!surface_);
    if (texture_)
      texture_->RemoveLightweightRef(has_context());
  }

  sk_sp<SkSurface> BeginWriteAccess(
      GrContext* gr_context,
      int final_msaa_count,
      const SkSurfaceProps& surface_props) override {
    CheckContext();
    // if there is already a surface_, it means previous BeginWriteAccess
    // doesn't have a corresponding EndWriteAccess.
    if (surface_)
      return nullptr;

    InsertAllEglFencesForWriting(ahb_backing()->GetWriteSyncFd(),
                                 ahb_backing()->GetReadSyncFds());

    if (!promise_texture_) {
      return nullptr;
    }

    SkColorType sk_color_type = viz::ResourceFormatToClosestSkColorType(
        /*gpu_compositing=*/true, format());
    auto surface = SkSurface::MakeFromBackendTextureAsRenderTarget(
        gr_context, promise_texture_->backendTexture(),
        kTopLeft_GrSurfaceOrigin, final_msaa_count, sk_color_type,
        backing()->color_space().ToSkColorSpace(), &surface_props);
    surface_ = surface.get();
    return surface;
  }

  void EndWriteAccess(sk_sp<SkSurface> surface) override {
    CheckContext();
    DCHECK_EQ(surface.get(), surface_);
    DCHECK(surface->unique());
    // TODO(ericrk): Keep the surface around for re-use.
    surface_ = nullptr;

    // Insert a gl fence to signal the write completion. Vulkan representation
    // needs to wait on this signal before it can read from this.
    base::ScopedFD sync_fd = CreateEglFenceAndExportFd();
    if (!sync_fd.is_valid())
      return;

    // Pass this fd to its backing.
    ahb_backing()->SetWriteSyncFd(std::move(sync_fd));

    ahb_backing()->ClearReadSyncFds();

    if (texture_) {
      if (texture_->IsLevelCleared(texture_->target(), 0))
        backing()->SetCleared();
    }
  }

  sk_sp<SkPromiseImageTexture> BeginReadAccess(SkSurface* sk_surface) override {
    CheckContext();
    if (!InsertEglWriteFence(ahb_backing()->GetWriteSyncFd()))
      return nullptr;
    return promise_texture_;
  }

  void EndReadAccess() override {
    CheckContext();

    base::ScopedFD sync_fd = CreateEglFenceAndExportFd();
    if (!sync_fd.is_valid())
      return;
    ahb_backing()->AddReadSyncFd(std::move(sync_fd));
  }

 private:
  SharedImageBackingAHB* ahb_backing() {
    return static_cast<SharedImageBackingAHB*>(backing());
  }

  void CheckContext() {
#if DCHECK_IS_ON()
    DCHECK(gl::GLContext::GetCurrent() == context_);
#endif
  }

  sk_sp<SkPromiseImageTexture> promise_texture_;
  gles2::Texture* texture_;
  SkSurface* surface_ = nullptr;
#if DCHECK_IS_ON()
  gl::GLContext* context_;
#endif
};

// Vk backed Skia representation of SharedImageBackingAHB.
class SharedImageRepresentationSkiaVkAHB
    : public SharedImageRepresentationSkia {
 public:
  SharedImageRepresentationSkiaVkAHB(SharedImageManager* manager,
                                     SharedImageBacking* backing)
      : SharedImageRepresentationSkia(manager, backing, nullptr) {
    SharedImageBackingAHB* ahb_backing =
        static_cast<SharedImageBackingAHB*>(backing);
    DCHECK(ahb_backing);
    context_state_ = ahb_backing->GetContextState();
    DCHECK(context_state_);
    DCHECK(context_state_->vk_context_provider());
  }

  ~SharedImageRepresentationSkiaVkAHB() override { DCHECK(!surface_); }

  sk_sp<SkSurface> BeginWriteAccess(
      GrContext* gr_context,
      int final_msaa_count,
      const SkSurfaceProps& surface_props) override {
    // If previous access has not ended.
    if (surface_)
      return nullptr;

    std::vector<base::ScopedFD> sync_fds = ahb_backing()->GetReadSyncFds();
    sync_fds.emplace_back(ahb_backing()->GetWriteSyncFd());
    std::vector<VkSemaphore> semaphores;
    if (!InsertAllVulkanFencesForWriting(std::move(sync_fds),
                                         vk_implementation(), vk_device(),
                                         vk_queue(), &semaphores))
      return nullptr;

    promise_texture_ = BeginVulkanAccess(
        vk_implementation(), vk_device(), vk_phy_device(), vk_queue(),
        ahb_backing()->GetAhbHandle(), size(), format(), std::move(semaphores));
    if (!promise_texture_)
      return nullptr;

    SkColorType sk_color_type = viz::ResourceFormatToClosestSkColorType(
        /*gpu_compositing=*/true, format());
    auto surface = SkSurface::MakeFromBackendTextureAsRenderTarget(
        gr_context, promise_texture_->backendTexture(),
        kTopLeft_GrSurfaceOrigin, final_msaa_count, sk_color_type, nullptr,
        &surface_props);

    // Cache the sk surface in the representation so that it can be used in the
    // EndWriteAccess. Also make sure previous surface_ have been consumed by
    // EndWriteAccess() call.
    surface_ = surface.get();

    return surface;
  }

  void EndWriteAccess(sk_sp<SkSurface> surface) override {
    // There should be a surface_ from the BeginWriteAccess().
    DCHECK(surface_);
    DCHECK_EQ(surface.get(), surface_);
    DCHECK(surface->unique());

    base::ScopedFD sync_fd;
    EndVulkanAccess(vk_implementation(), vk_device(), vk_queue(), &surface_,
                    &sync_fd);
    // pass this sync fd to the backing.
    ahb_backing()->SetWriteSyncFd(std::move(sync_fd));
    ahb_backing()->ClearReadSyncFds();
  }

  sk_sp<SkPromiseImageTexture> BeginReadAccess(SkSurface* sk_surface) override {
    // If previous access has not ended.
    if (surface_)
      return nullptr;
    DCHECK(sk_surface);

    // Synchronise the read access with the writes.
    base::ScopedFD sync_fd = ahb_backing()->GetWriteSyncFd();

    VkSemaphore semaphore = VK_NULL_HANDLE;
    // We need to wait only if there is a valid fd.
    if (sync_fd.is_valid()) {
      // Import the above sync fd into a semaphore.
      if (!vk_implementation()->ImportSemaphoreFdKHR(
              vk_device(), std::move(sync_fd), &semaphore)) {
        return nullptr;
      }

      // Submit wait semaphore to the queue. Note that Skia uses the same queue
      // exposed by vk_queue(), so this will work due to Vulkan queue ordering.
      if (!vk_implementation()->SubmitWaitSemaphore(vk_queue(), semaphore)) {
        vkDestroySemaphore(vk_device(), semaphore, nullptr);
        return nullptr;
      }
    }

    std::vector<VkSemaphore> semaphores_to_clean(1);
    semaphores_to_clean.emplace_back(std::move(semaphore));

    auto promise_texture =
        BeginVulkanAccess(vk_implementation(), vk_device(), vk_phy_device(),
                          vk_queue(), ahb_backing()->GetAhbHandle(), size(),
                          format(), std::move(semaphores_to_clean));

    // Cache the sk surface in the representation so that it can be used in the
    // EndReadAccess. Also make sure previous surface_ have been consumed by
    // EndReadAccess() call.
    surface_ = sk_surface;

    return promise_texture;
  }

  void EndReadAccess() override {
    // There should be a surface_ from the BeginReadAccess().
    DCHECK(surface_);

    base::ScopedFD sync_fd;
    EndVulkanAccess(vk_implementation(), vk_device(), vk_queue(), &surface_,
                    &sync_fd);
    // pass this sync fd to the backing.
    ahb_backing()->AddReadSyncFd(std::move(sync_fd));
  }

 private:
  SharedImageBackingAHB* ahb_backing() {
    return static_cast<SharedImageBackingAHB*>(backing());
  }

  gpu::VulkanImplementation* vk_implementation() {
    return context_state_->vk_context_provider()->GetVulkanImplementation();
  }

  VkDevice vk_device() {
    return context_state_->vk_context_provider()
        ->GetDeviceQueue()
        ->GetVulkanDevice();
  }

  VkPhysicalDevice vk_phy_device() {
    return context_state_->vk_context_provider()
        ->GetDeviceQueue()
        ->GetVulkanPhysicalDevice();
  }

  VkQueue vk_queue() {
    return context_state_->vk_context_provider()
        ->GetDeviceQueue()
        ->GetVulkanQueue();
  }

  sk_sp<SkPromiseImageTexture> promise_texture_;
  SkSurface* surface_ = nullptr;
  SharedContextState* context_state_ = nullptr;
};

SharedImageBackingAHB::SharedImageBackingAHB(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage,
    base::android::ScopedHardwareBufferHandle handle,
    size_t estimated_size,
    SharedContextState* context_state)
    : SharedImageBacking(mailbox,
                         format,
                         size,
                         color_space,
                         usage,
                         estimated_size),
      hardware_buffer_handle_(std::move(handle)),
      context_state_(context_state) {
  DCHECK(hardware_buffer_handle_.is_valid());
}

SharedImageBackingAHB::~SharedImageBackingAHB() {
  // Check to make sure buffer is explicitly destroyed using Destroy() api
  // before this destructor is called.
  DCHECK(!hardware_buffer_handle_.is_valid());
}

bool SharedImageBackingAHB::IsCleared() const {
  return is_cleared_;
}

void SharedImageBackingAHB::SetCleared() {
  if (legacy_texture_)
    legacy_texture_->SetLevelCleared(legacy_texture_->target(), 0, true);
  is_cleared_ = true;
}

void SharedImageBackingAHB::Update() {}

bool SharedImageBackingAHB::ProduceLegacyMailbox(
    MailboxManager* mailbox_manager) {
  DCHECK(hardware_buffer_handle_.is_valid());
  legacy_texture_ = GenGLTexture();
  if (!legacy_texture_)
    return false;
  mailbox_manager->ProduceTexture(mailbox(), legacy_texture_);
  return true;
}

void SharedImageBackingAHB::Destroy() {
  DCHECK(hardware_buffer_handle_.is_valid());
  if (legacy_texture_) {
    legacy_texture_->RemoveLightweightRef(have_context());
    legacy_texture_ = nullptr;
  }
  hardware_buffer_handle_.reset();
}

SharedContextState* SharedImageBackingAHB::GetContextState() const {
  return context_state_;
}

base::ScopedFD SharedImageBackingAHB::GetWriteSyncFd() const {
  base::ScopedFD dup_fd =
      base::ScopedFD(HANDLE_EINTR(dup(write_sync_fd_.get())));
  return dup_fd;
}

void SharedImageBackingAHB::SetWriteSyncFd(base::ScopedFD fd) {
  write_sync_fd_ = std::move(fd);
}

std::vector<base::ScopedFD> SharedImageBackingAHB::GetReadSyncFds() const {
  std::vector<base::ScopedFD> dup_fds{read_sync_fds_.size()};
  for (size_t i = 0; i < read_sync_fds_.size(); ++i) {
    base::ScopedFD dup_fd =
        base::ScopedFD(HANDLE_EINTR(dup(read_sync_fds_[i].get())));
    dup_fds.emplace_back(std::move(dup_fd));
  }
  return dup_fds;
}

void SharedImageBackingAHB::AddReadSyncFd(base::ScopedFD fd) {
  read_sync_fds_.emplace_back(std::move(fd));
}

void SharedImageBackingAHB::ClearReadSyncFds() {
  read_sync_fds_.clear();
  read_sync_fds_.shrink_to_fit();
}

base::android::ScopedHardwareBufferHandle
SharedImageBackingAHB::GetAhbHandle() {
  return hardware_buffer_handle_.Clone();
}

std::unique_ptr<SharedImageRepresentationGLTexture>
SharedImageBackingAHB::ProduceGLTexture(SharedImageManager* manager,
                                        MemoryTypeTracker* tracker) {
  // Use same texture for all the texture representations generated from same
  // backing.
  auto* texture = GenGLTexture();
  if (!texture)
    return nullptr;

  return std::make_unique<SharedImageRepresentationGLTextureAHB>(
      manager, this, tracker, std::move(texture));
}

std::unique_ptr<SharedImageRepresentationSkia>
SharedImageBackingAHB::ProduceSkia(SharedImageManager* manager,
                                   MemoryTypeTracker* tracker) {
  DCHECK(context_state_);

  // Check whether we are in Vulkan mode OR GL mode and accordingly create
  // Skia representation.
  if (context_state_->use_vulkan_gr_context()) {
    return std::make_unique<SharedImageRepresentationSkiaVkAHB>(manager, this);
  }

  auto* texture = GenGLTexture();
  if (!texture)
    return nullptr;

  GrBackendTexture backend_texture;
  GetGrBackendTexture(gl::GLContext::GetCurrent()->GetVersionInfo(),
                      texture->target(), size(), texture->service_id(),
                      format(), &backend_texture);
  sk_sp<SkPromiseImageTexture> promise_texture =
      SkPromiseImageTexture::Make(backend_texture);
  return std::make_unique<SharedImageRepresentationSkiaGLAHB>(
      manager, this, promise_texture, tracker, std::move(texture));
}

gles2::Texture* SharedImageBackingAHB::GenGLTexture() {
  DCHECK(hardware_buffer_handle_.is_valid());

  // Target for AHB backed egl images.
  // Note that we are not using GL_TEXTURE_EXTERNAL_OES target since sksurface
  // doesn't supports it. As per the egl documentation -
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
    return nullptr;
  }
  if (!egl_image->BindTexImage(target)) {
    LOG(ERROR) << "Failed to bind egl image";
    api->glBindTextureFn(target, old_texture_binding);
    api->glDeleteTexturesFn(1, &service_id);
    return nullptr;
  }

  // Create a gles2 Texture.
  auto* texture = new gles2::Texture(service_id);
  texture->SetLightweightRef();
  texture->SetTarget(target, 1);
  texture->sampler_state_.min_filter = GL_LINEAR;
  texture->sampler_state_.mag_filter = GL_LINEAR;
  texture->sampler_state_.wrap_t = GL_CLAMP_TO_EDGE;
  texture->sampler_state_.wrap_s = GL_CLAMP_TO_EDGE;

  // If the backing is already cleared, no need to clear it again.
  gfx::Rect cleared_rect;
  if (is_cleared_)
    cleared_rect = gfx::Rect(size());

  GLenum gl_format = viz::GLDataFormat(format());
  GLenum gl_type = viz::GLDataType(format());
  texture->SetLevelInfo(target, 0, egl_image->GetInternalFormat(),
                        size().width(), size().height(), 1, 0, gl_format,
                        gl_type, cleared_rect);
  texture->SetLevelImage(target, 0, egl_image.get(), gles2::Texture::BOUND);
  texture->SetImmutable(true);
  api->glBindTextureFn(target, old_texture_binding);
  DCHECK_EQ(egl_image->GetInternalFormat(), gl_format);
  return texture;
}

SharedImageBackingFactoryAHB::SharedImageBackingFactoryAHB(
    const GpuDriverBugWorkarounds& workarounds,
    const GpuFeatureInfo& gpu_feature_info,
    SharedContextState* context_state)
    : context_state_(context_state) {
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
    // with AHB, we need to check if oes_egl_image_external is supported or
    // not.
    if (!is_egl_image_supported)
      continue;

    // Check if AHB backed GL texture can be created using this format and
    // gather GL related format info.
    // TODO(vikassoni): Add vulkan related information in future.
    GLuint internal_format = viz::GLInternalFormat(format);
    GLenum gl_format = viz::GLDataFormat(format);
    GLenum gl_type = viz::GLDataType(format);

    //  GLImageAHardwareBuffer supports internal format GL_RGBA and GL_RGB.
    if (internal_format != GL_RGBA && internal_format != GL_RGB)
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
  // code until we are asked to make a GL representation (or allocate a
  // backing for import into GL)? We may use an AHardwareBuffer exclusively
  // with Vulkan, where there is no need to require that a GL context is
  // current. Maybe we can lazy init this if someone tries to create an
  // AHardwareBuffer with SHARED_IMAGE_USAGE_GLES2 ||
  // !gpu_preferences.enable_vulkan. When in Vulkan mode, we should only need
  // this with GLES2.
  gl::GLApi* api = gl::g_current_gl_context;
  api->glGetIntegervFn(GL_MAX_TEXTURE_SIZE, &max_gl_texture_size_);

  // TODO(vikassoni): Check vulkan image size restrictions also.
  if (workarounds.max_texture_size) {
    max_gl_texture_size_ =
        std::min(max_gl_texture_size_, workarounds.max_texture_size);
  }
  // Ensure max_texture_size_ is less than INT_MAX so that gfx::Rect and friends
  // can be used to accurately represent all valid sub-rects, with overflow
  // cases, clamped to INT_MAX, always invalid.
  max_gl_texture_size_ = std::min(max_gl_texture_size_, INT_MAX - 1);
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
  // representation and SHARED_IMAGE_USAGE_DISPLAY is used for cases we want
  // to read from skia representation.
  // TODO(vikassoni): Also check gpu_preferences.enable_vulkan to figure out
  // if skia is using vulkan backing or GL backing.
  const bool use_gles2 =
      (usage & (SHARED_IMAGE_USAGE_GLES2 | SHARED_IMAGE_USAGE_RASTER |
                SHARED_IMAGE_USAGE_DISPLAY));

  // If usage flags indicated this backing can be used as a GL texture, then
  // do below gl related checks.
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
      base::android::ScopedHardwareBufferHandle::Adopt(buffer), estimated_size,
      context_state_);
  return backing;
}

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactoryAHB::CreateSharedImage(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage,
    base::span<const uint8_t> pixel_data) {
  NOTIMPLEMENTED();
  return nullptr;
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
