// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_feature_checker_impl.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/public/browser/browser_thread.h"

namespace content {

namespace {

// A false return value is always valid, but a true one is only valid if full
// GPU info has been collected in a GPU process.
bool IsFeatureAllowed(GpuDataManagerImpl* manager,
                      gpu::GpuFeatureType feature) {
  // This is mostly for WebStore checking WebGL status, therefore, don't return
  // yes if WebGL is rendering on software, because user experience on the app
  // won't be good in most cases.
  return (manager->GpuAccessAllowed(nullptr) &&
          manager->GetFeatureStatus(feature) == gpu::kGpuFeatureStatusEnabled);
}

}  // namespace

// static
scoped_refptr<GpuFeatureChecker> GpuFeatureChecker::Create(
    gpu::GpuFeatureType feature,
    FeatureAvailableCallback callback) {
  return new GpuFeatureCheckerImpl(feature, callback);
}

GpuFeatureCheckerImpl::GpuFeatureCheckerImpl(gpu::GpuFeatureType feature,
                                             FeatureAvailableCallback callback)
    : feature_(feature), callback_(callback) {}

GpuFeatureCheckerImpl::~GpuFeatureCheckerImpl() {}

void GpuFeatureCheckerImpl::CheckGpuFeatureAvailability() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  GpuDataManagerImpl* manager = GpuDataManagerImpl::GetInstance();
  if (!manager->IsGpuFeatureInfoAvailable()) {
    AddRef();
    manager->AddObserver(this);
    return;
  }

  bool feature_allowed = IsFeatureAllowed(manager, feature_);
  callback_.Run(feature_allowed);
}

void GpuFeatureCheckerImpl::OnGpuInfoUpdate() {
  GpuDataManagerImpl* manager = GpuDataManagerImpl::GetInstance();
  if (manager->IsGpuFeatureInfoAvailable()) {
    manager->RemoveObserver(this);
    bool feature_allowed = IsFeatureAllowed(manager, feature_);
    callback_.Run(feature_allowed);

    // Matches the AddRef in CheckGpuFeatureAvailability().
    Release();
  }
}

}  // namespace content
