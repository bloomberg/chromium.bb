// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gpu/gpu_feature_checker.h"

#include "base/logging.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/gpu_data_manager.h"

namespace {

// A false return value is always valid, but a true one is only valid if full
// GPU info has been collected in a GPU process.
bool IsFeatureAllowed(content::GpuDataManager* manager,
                      gpu::GpuFeatureType feature) {
  return (manager->GpuAccessAllowed(NULL) &&
          !manager->IsFeatureBlacklisted(feature));
}

}  // namespace

GPUFeatureChecker::GPUFeatureChecker(gpu::GpuFeatureType feature,
                                     FeatureAvailableCallback callback)
    : feature_(feature),
      callback_(callback) {
}

GPUFeatureChecker::~GPUFeatureChecker() {
}

void GPUFeatureChecker::CheckGPUFeatureAvailability() {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  bool finalized = true;
#if defined(OS_LINUX)
  // On Windows and Mac, so far we can always make the final WebGL blacklisting
  // decision based on partial GPU info; on Linux, we need to launch the GPU
  // process to collect full GPU info and make the final decision.
  finalized = false;
#endif

  content::GpuDataManager* manager = content::GpuDataManager::GetInstance();
  if (manager->IsEssentialGpuInfoAvailable())
    finalized = true;

  bool feature_allowed = IsFeatureAllowed(manager, feature_);
  if (!feature_allowed)
    finalized = true;

  if (finalized) {
    callback_.Run(feature_allowed);
  } else {
    // Matched with a Release in OnGpuInfoUpdate.
    AddRef();

    manager->AddObserver(this);
    manager->RequestCompleteGpuInfoIfNeeded();
  }
}

void GPUFeatureChecker::OnGpuInfoUpdate() {
  content::GpuDataManager* manager = content::GpuDataManager::GetInstance();
  manager->RemoveObserver(this);
  bool feature_allowed = IsFeatureAllowed(manager, feature_);
  callback_.Run(feature_allowed);

  // Matches the AddRef in HasFeature().
  Release();
}
