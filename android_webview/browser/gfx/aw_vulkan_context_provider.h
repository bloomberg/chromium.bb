// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_AW_VULKAN_CONTEXT_PROVIDER_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_AW_VULKAN_CONTEXT_PROVIDER_H_

#include <memory>

#include "base/macros.h"
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "third_party/skia/include/core/SkRefCnt.h"

struct AwDrawFn_InitVkParams;
class GrContext;
class GrVkSecondaryCBDrawContext;

namespace gpu {
class VulkanImplementation;
class VulkanDeviceQueue;
}  // namespace gpu

namespace android_webview {

class AwVulkanContextProvider final : public viz::VulkanContextProvider {
 public:
  class ScopedDrawContext {
   public:
    ScopedDrawContext(AwVulkanContextProvider* provider,
                      GrVkSecondaryCBDrawContext* draw_context)
        : provider_(provider) {
      provider_->set_draw_context(draw_context);
    }
    ~ScopedDrawContext() { provider_->set_draw_context(nullptr); }

   private:
    AwVulkanContextProvider* const provider_;

    DISALLOW_COPY_AND_ASSIGN(ScopedDrawContext);
  };

  static scoped_refptr<AwVulkanContextProvider> GetOrCreateInstance(
      AwDrawFn_InitVkParams* params);

  // viz::VulkanContextProvider implementation:
  gpu::VulkanImplementation* GetVulkanImplementation() override;
  gpu::VulkanDeviceQueue* GetDeviceQueue() override;
  GrContext* GetGrContext() override;
  GrVkSecondaryCBDrawContext* GetGrSecondaryCBDrawContext() override;

  VkPhysicalDevice physical_device() {
    return device_queue_->GetVulkanPhysicalDevice();
  }
  VkDevice device() { return device_queue_->GetVulkanDevice(); }
  VkQueue queue() { return device_queue_->GetVulkanQueue(); }
  gpu::VulkanImplementation* implementation() { return implementation_.get(); }
  GrContext* gr_context() { return gr_context_.get(); }

 private:
  friend class base::RefCounted<AwVulkanContextProvider>;

  AwVulkanContextProvider();
  ~AwVulkanContextProvider() override;

  bool Initialize(AwDrawFn_InitVkParams* params);

  void set_draw_context(GrVkSecondaryCBDrawContext* draw_context) {
    DCHECK_NE(!!draw_context_, !!draw_context);
    draw_context_ = draw_context;
  }

  std::unique_ptr<gpu::VulkanImplementation> implementation_;
  std::unique_ptr<gpu::VulkanDeviceQueue> device_queue_;
  sk_sp<GrContext> gr_context_;
  GrVkSecondaryCBDrawContext* draw_context_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AwVulkanContextProvider);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_AW_VULKAN_CONTEXT_PROVIDER_H_
