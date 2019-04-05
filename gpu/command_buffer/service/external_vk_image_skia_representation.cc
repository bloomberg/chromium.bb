// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/external_vk_image_skia_representation.h"

#include <limits>
#include <utility>

#include "base/trace_event/trace_event.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_util.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"

namespace gpu {

ExternalVkImageSkiaRepresentation::ExternalVkImageSkiaRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker)
    : SharedImageRepresentationSkia(manager, backing, tracker) {
  VkFenceCreateInfo create_info = {
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
  };
  VkResult result =
      vkCreateFence(vk_device(), &create_info, nullptr /* pAllocator */,
                    &begin_access_fence_);
  DCHECK_EQ(result, VK_SUCCESS);
  result = vkCreateFence(vk_device(), &create_info, nullptr /* pAllocator */,
                         &end_access_fence_);
  DCHECK_EQ(result, VK_SUCCESS);
}

ExternalVkImageSkiaRepresentation::~ExternalVkImageSkiaRepresentation() {
  DestroySemaphores(std::move(begin_access_semaphores_), begin_access_fence_);
  begin_access_semaphores_.clear();
  DestroySemaphore(end_access_semaphore_, end_access_fence_);
  end_access_semaphore_ = VK_NULL_HANDLE;
  vkDestroyFence(vk_device(), begin_access_fence_, nullptr /* pAllocator */);
  vkDestroyFence(vk_device(), end_access_fence_, nullptr /* pAllocator */);
}

sk_sp<SkSurface> ExternalVkImageSkiaRepresentation::BeginWriteAccess(
    GrContext* gr_context,
    int final_msaa_count,
    const SkSurfaceProps& surface_props) {
  DCHECK_EQ(access_mode_, kNone) << "Previous access hasn't ended yet";
  DCHECK(!surface_);

  auto promise_texture = BeginAccess(false /* readonly */);
  if (!promise_texture)
    return nullptr;
  SkColorType sk_color_type = viz::ResourceFormatToClosestSkColorType(
      true /* gpu_compositing */, format());
  surface_ = SkSurface::MakeFromBackendTextureAsRenderTarget(
      gr_context, promise_texture->backendTexture(), kTopLeft_GrSurfaceOrigin,
      final_msaa_count, sk_color_type,
      backing_impl()->color_space().ToSkColorSpace(), &surface_props);
  access_mode_ = kWrite;
  return surface_;
}

void ExternalVkImageSkiaRepresentation::EndWriteAccess(
    sk_sp<SkSurface> surface) {
  DCHECK_EQ(access_mode_, kWrite)
      << "EndWriteAccess is called before BeginWriteAccess";
  DCHECK(surface_);
  surface_ = nullptr;
  EndAccess(false /* readonly */);
  access_mode_ = kNone;
}

sk_sp<SkPromiseImageTexture>
ExternalVkImageSkiaRepresentation::BeginReadAccess() {
  DCHECK_EQ(access_mode_, kNone) << "Previous access hasn't ended yet";
  DCHECK(!surface_);

  auto promise_texture = BeginAccess(true /* readonly */);
  if (!promise_texture)
    return nullptr;
  access_mode_ = kRead;
  return promise_texture;
}

void ExternalVkImageSkiaRepresentation::EndReadAccess() {
  DCHECK_EQ(access_mode_, kRead)
      << "EndReadAccess is called before BeginReadAccess";
  EndAccess(true /* readonly */);
  access_mode_ = kNone;
}

sk_sp<SkPromiseImageTexture> ExternalVkImageSkiaRepresentation::BeginAccess(
    bool readonly) {
  DCHECK_EQ(access_mode_, kNone);
  DestroySemaphores(std::move(begin_access_semaphores_), begin_access_fence_);
  begin_access_semaphores_.clear();

  std::vector<SemaphoreHandle> handles;
  if (!backing_impl()->BeginAccess(readonly, &handles))
    return nullptr;

  for (auto& handle : handles) {
    VkSemaphore semaphore = vk_implementation()->ImportSemaphoreHandle(
        vk_device(), std::move(handle));
    if (semaphore != VK_NULL_HANDLE)
      begin_access_semaphores_.push_back(semaphore);
  }

  if (!begin_access_semaphores_.empty()) {
    // Submit wait semaphore to the queue. Note that Skia uses the same queue
    // exposed by vk_queue(), so this will work due to Vulkan queue ordering.
    if (!SubmitWaitVkSemaphores(vk_queue(), begin_access_semaphores_,
                                begin_access_fence_)) {
      LOG(ERROR) << "Failed to wait on semaphore";
      // Since the semaphore was not actually sent to the queue, it is safe to
      // destroy the |begin_access_semaphores_| here.
      DestroySemaphores(std::move(begin_access_semaphores_));
      begin_access_semaphores_.clear();
      return nullptr;
    }
  }

  // Create backend texture from the VkImage.
  GrVkAlloc alloc(backing_impl()->memory(), 0 /* offset */,
                  backing_impl()->memory_size(), 0 /* flags */);
  GrVkImageInfo vk_image_info(backing_impl()->image(), alloc,
                              VK_IMAGE_TILING_OPTIMAL,
                              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                              backing_impl()->vk_format(), 1 /* levelCount */);

  return SkPromiseImageTexture::Make(
      GrBackendTexture(size().width(), size().height(), vk_image_info));
}

void ExternalVkImageSkiaRepresentation::EndAccess(bool readonly) {
  DCHECK_NE(access_mode_, kNone);
  // Cleanup resources for previous accessing.
  DestroySemaphore(end_access_semaphore_, end_access_fence_);

  end_access_semaphore_ =
      vk_implementation()->CreateExternalSemaphore(backing_impl()->device());
  // Submit wait semaphore to the queue. Note that Skia uses the same queue
  // exposed by vk_queue(), so this will work due to Vulkan queue ordering.
  if (!SubmitSignalVkSemaphore(vk_queue(), end_access_semaphore_,
                               end_access_fence_)) {
    LOG(ERROR) << "Failed to wait on semaphore";
    // Since the semaphore was not actually sent to the queue, it is safe to
    // destroy the |end_access_semaphore_| here.
    DestroySemaphore(end_access_semaphore_);
    end_access_semaphore_ = VK_NULL_HANDLE;
  }

  SemaphoreHandle handle;
  if (end_access_semaphore_ != VK_NULL_HANDLE) {
    handle = vk_implementation()->GetSemaphoreHandle(vk_device(),
                                                     end_access_semaphore_);
    if (!handle.is_valid())
      LOG(FATAL) << "Failed to get handle from a semaphore.";
  }
  backing_impl()->EndAccess(readonly, std::move(handle));
}

void ExternalVkImageSkiaRepresentation::DestroySemaphores(
    std::vector<VkSemaphore> semaphores,
    VkFence fence) {
  if (semaphores.empty())
    return;
  if (fence != VK_NULL_HANDLE)
    WaitAndResetFence(fence);
  for (VkSemaphore semaphore : semaphores)
    vkDestroySemaphore(vk_device(), semaphore, nullptr /* pAllocator */);
}

void ExternalVkImageSkiaRepresentation::DestroySemaphore(VkSemaphore semaphore,
                                                         VkFence fence) {
  if (semaphore == VK_NULL_HANDLE)
    return;
  if (fence != VK_NULL_HANDLE)
    WaitAndResetFence(fence);
  vkDestroySemaphore(vk_device(), semaphore, nullptr /* pAllocator */);
}

void ExternalVkImageSkiaRepresentation::WaitAndResetFence(VkFence fence) {
  TRACE_EVENT0("gpu", "ExternalVkImageSkiaRepresentation::WaitAndResetFence");
  VkResult result = vkWaitForFences(vk_device(), 1, &fence, VK_TRUE,
                                    std::numeric_limits<uint64_t>::max());
  LOG_IF(FATAL, result != VK_SUCCESS) << "vkWaitForFences failed.";
  vkResetFences(vk_device(), 1, &fence);
}

}  // namespace gpu
