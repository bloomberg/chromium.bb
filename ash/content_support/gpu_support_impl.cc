// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/content_support/gpu_support_impl.h"

#include "content/public/browser/gpu_data_manager.h"
#include "gpu/config/gpu_feature_type.h"

namespace ash {

GPUSupportImpl::GPUSupportImpl() {
}

GPUSupportImpl::~GPUSupportImpl() {
}

bool GPUSupportImpl::IsPanelFittingDisabled() const {
  return content::GpuDataManager::GetInstance()->IsFeatureBlacklisted(
      gpu::GPU_FEATURE_TYPE_PANEL_FITTING);
}

void GPUSupportImpl::DisableGpuWatchdog() {
  content::GpuDataManager::GetInstance()->DisableGpuWatchdog();
}

void GPUSupportImpl::GetGpuProcessHandles(
      const GetGpuProcessHandlesCallback& callback) const {
  content::GpuDataManager::GetInstance()->GetGpuProcessHandles(callback);
}

}  // namespace ash
