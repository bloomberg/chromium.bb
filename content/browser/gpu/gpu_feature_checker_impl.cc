// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_feature_checker_impl.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/gpu_data_manager.h"

namespace content {

namespace {

// A false return value is always valid, but a true one is only valid if full
// GPU info has been collected in a GPU process.
bool IsFeatureAllowed(GpuDataManager* manager, gpu::GpuFeatureType feature) {
  return (manager->GpuAccessAllowed(NULL) &&
          !manager->IsFeatureBlacklisted(feature));
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

  bool finalized = true;
#if defined(OS_LINUX)
  // On Windows and Mac, so far we can always make the final WebGL blacklisting
  // decision based on partial GPU info; on Linux, we need to launch the GPU
  // process to collect full GPU info and make the final decision.
  finalized = false;
#endif

  GpuDataManager* manager = GpuDataManager::GetInstance();
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

void GpuFeatureCheckerImpl::OnGpuInfoUpdate() {
  GpuDataManager* manager = GpuDataManager::GetInstance();
  manager->RemoveObserver(this);
  bool feature_allowed = IsFeatureAllowed(manager, feature_);
  callback_.Run(feature_allowed);

  // Matches the AddRef in CheckGpuFeatureAvailability().
  Release();
}

}  // namespace content
