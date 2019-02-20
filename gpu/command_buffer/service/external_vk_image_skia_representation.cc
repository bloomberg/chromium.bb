// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/external_vk_image_skia_representation.h"

#include "gpu/vulkan/vulkan_function_pointers.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"

namespace gpu {

ExternalVkImageSkiaRepresentation::ExternalVkImageSkiaRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker)
    : SharedImageRepresentationSkia(manager, backing, tracker) {}

ExternalVkImageSkiaRepresentation::~ExternalVkImageSkiaRepresentation() =
    default;

sk_sp<SkSurface> ExternalVkImageSkiaRepresentation::BeginWriteAccess(
    GrContext* gr_context,
    int final_msaa_count,
    const SkSurfaceProps& surface_props) {
  // TODO(crbug.com/932214): Implement this method.
  NOTIMPLEMENTED();
  return nullptr;
}

void ExternalVkImageSkiaRepresentation::EndWriteAccess(
    sk_sp<SkSurface> surface) {
  // TODO(crbug.com/932214): Implement this method.
  NOTIMPLEMENTED();
}

sk_sp<SkPromiseImageTexture> ExternalVkImageSkiaRepresentation::BeginReadAccess(
    SkSurface* sk_surface) {
  DCHECK(!read_surface_) << "Previous read hasn't ended yet";

  VkSemaphore gl_write_finished_semaphore;
  // This can return false if another write access is currently in progress.
  if (!backing_impl()->BeginVulkanReadAccess(&gl_write_finished_semaphore))
    return nullptr;

  if (gl_write_finished_semaphore != VK_NULL_HANDLE) {
    // Submit wait semaphore to the queue. Note that Skia uses the same queue
    // exposed by vk_queue(), so this will work due to Vulkan queue ordering.
    if (!vk_implementation()->SubmitWaitSemaphore(
            vk_queue(), gl_write_finished_semaphore)) {
      LOG(ERROR) << "Failed to wait on semaphore";
      // Since the semaphore was not actually sent to the queue, it is safe to
      // destroy it here.
      vkDestroySemaphore(vk_device(), gl_write_finished_semaphore, nullptr);
      return nullptr;
    }
  }

  // Create backend texture from the VkImage.
  GrVkAlloc alloc = {backing_impl()->memory(), 0, backing_impl()->memory_size(),
                     0};
  GrVkImageInfo vk_info = {
      backing_impl()->image(),     alloc,
      VK_IMAGE_TILING_OPTIMAL,     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      backing_impl()->vk_format(), 1 /* levelCount */};
  // TODO(bsalomon): Determine whether it makes sense to attempt to reuse this
  // if the vk_info stays the same on subsequent calls.
  auto promise_texture = SkPromiseImageTexture::Make(
      GrBackendTexture(size().width(), size().height(), vk_info));

  // Cache the sk surface in the representation so that it can be used in the
  // EndReadAccess.
  read_surface_ = sk_sp<SkSurface>(sk_surface);

  // TODO(samans): This function should take a sk_sp<SkSurface> instead of a
  // SkSurface* so we don't have to manually add a reference here.
  read_surface_->ref();

  // TODO(crbug.com/932260): Need to do better semaphore cleanup management.
  // Waiting on device to be idle to delete the semaphore is costly. Instead use
  // a fence to get signal when semaphore submission is done.
  if (gl_write_finished_semaphore) {
    VkResult result = vkQueueWaitIdle(vk_queue());
    if (result != VK_SUCCESS) {
      LOG(ERROR) << "vkQueueWaitIdle failed: " << result;
      return nullptr;
    }
    vkDestroySemaphore(vk_device(), gl_write_finished_semaphore, nullptr);
  }

  return promise_texture;
}

void ExternalVkImageSkiaRepresentation::EndReadAccess() {
  DCHECK(read_surface_) << "EndReadAccess is called before BeginReadAccess";

  VkSemaphore vulkan_write_finished_semaphore =
      backing_impl()->CreateExternalVkSemaphore();

  if (vulkan_write_finished_semaphore == VK_NULL_HANDLE) {
    // TODO(crbug.com/933452): We should be able to handle this failure more
    // gracefully rather than shutting down the whole process.
    LOG(FATAL) << "Unable to create a VkSemaphore in "
               << "ExternalVkImageSkiaRepresentation";
    read_surface_ = nullptr;
    return;
  }

  GrBackendSemaphore gr_semaphore;
  gr_semaphore.initVulkan(vulkan_write_finished_semaphore);

  // If GrSemaphoresSubmitted::kNo is returned, the GPU back-end did not
  // create or add any semaphores to signal on the GPU; the caller should not
  // instruct the GPU to wait on any of the semaphores.
  if (read_surface_->flushAndSignalSemaphores(1, &gr_semaphore) ==
      GrSemaphoresSubmitted::kNo) {
    // TODO(crbug.com/933452): We should be able to handle this failure more
    // gracefully rather than shutting down the whole process.
    LOG(FATAL) << "Unable to signal VkSemaphore in "
                  "ExternalVkImageSkiaRepresentation";
    vkDestroySemaphore(vk_device(), vulkan_write_finished_semaphore, nullptr);
    read_surface_ = nullptr;
    return;
  }

  read_surface_ = nullptr;

  // Wait for the queue to get idle, so that when
  // |vulkan_write_finished_semaphore| gets destroyed, we can guarantee it's not
  // associated with any unexecuted command.
  VkResult result = vkQueueWaitIdle(vk_queue());
  if (result != VK_SUCCESS) {
    // TODO(crbug.com/933452): We should be able to handle this failure more
    // gracefully rather than shutting down the whole process.
    LOG(FATAL) << "vkQueueWaitIdle failed: " << result;
    return;
  }

  backing_impl()->EndVulkanReadAccess(vulkan_write_finished_semaphore);
}

}  // namespace gpu
