// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/content_navigation_policy.h"

#include <bitset>

#include "base/metrics/field_trial_params.h"
#include "base/system/sys_info.h"
#include "content/public/common/content_features.h"

namespace content {

bool DeviceHasEnoughMemoryForBackForwardCache() {
  // This method make sure that the physical memory of device is greater than
  // the allowed threshold and enables back-forward cache if the feature
  // kBackForwardCacheMemoryControl is enabled.
  // It is important to check the base::FeatureList to avoid activating any
  // field trial groups if BFCache is disabled due to memory threshold.
  if (base::FeatureList::IsEnabled(features::kBackForwardCacheMemoryControl)) {
    int memory_threshold_mb = base::GetFieldTrialParamByFeatureAsInt(
        features::kBackForwardCacheMemoryControl,
        "memory_threshold_for_back_forward_cache_in_mb", 0);
    return base::SysInfo::AmountOfPhysicalMemoryMB() > memory_threshold_mb;
  }

  // If the feature kBackForwardCacheMemoryControl is not enabled, all the
  // devices are included by default.
  return true;
}

bool IsBackForwardCacheEnabled() {
  if (!DeviceHasEnoughMemoryForBackForwardCache())
    return false;
  // The feature needs to be checked last, because checking the feature
  // activates the field trial and assigns the client either to a control or an
  // experiment group - such assignment should be final.
  return base::FeatureList::IsEnabled(features::kBackForwardCache);
}

const char kProactivelySwapBrowsingInstanceLevelParameterName[] = "level";

constexpr base::FeatureParam<ProactivelySwapBrowsingInstanceLevel>::Option
    proactively_swap_browsing_instance_levels[] = {
        {ProactivelySwapBrowsingInstanceLevel::kDisabled, "Disabled"},
        {ProactivelySwapBrowsingInstanceLevel::kCrossSiteSwapProcess,
         "CrossSiteSwapProcess"},
        {ProactivelySwapBrowsingInstanceLevel::kCrossSiteReuseProcess,
         "CrossSiteReuseProcess"}};
const base::FeatureParam<ProactivelySwapBrowsingInstanceLevel>
    proactively_swap_browsing_instance_level{
        &features::kProactivelySwapBrowsingInstance,
        kProactivelySwapBrowsingInstanceLevelParameterName,
        ProactivelySwapBrowsingInstanceLevel::kDisabled,
        &proactively_swap_browsing_instance_levels};

ProactivelySwapBrowsingInstanceLevel GetProactivelySwapBrowsingInstanceLevel() {
  if (base::FeatureList::IsEnabled(features::kProactivelySwapBrowsingInstance))
    return proactively_swap_browsing_instance_level.Get();
  return ProactivelySwapBrowsingInstanceLevel::kDisabled;
}

bool IsProactivelySwapBrowsingInstanceEnabled() {
  return GetProactivelySwapBrowsingInstanceLevel() >=
         ProactivelySwapBrowsingInstanceLevel::kCrossSiteSwapProcess;
}

bool IsProactivelySwapBrowsingInstanceWithProcessReuseEnabled() {
  return GetProactivelySwapBrowsingInstanceLevel() >=
         ProactivelySwapBrowsingInstanceLevel::kCrossSiteReuseProcess;
}
const char kRenderDocumentLevelParameterName[] = "level";

constexpr base::FeatureParam<RenderDocumentLevel>::Option
    render_document_levels[] = {
        {RenderDocumentLevel::kDisabled, "disabled"},
        {RenderDocumentLevel::kCrashedFrame, "crashed-frame"},
        {RenderDocumentLevel::kSubframe, "subframe"}};
const base::FeatureParam<RenderDocumentLevel> render_document_level{
    &features::kRenderDocument, kRenderDocumentLevelParameterName,
    RenderDocumentLevel::kDisabled, &render_document_levels};

RenderDocumentLevel GetRenderDocumentLevel() {
  if (base::FeatureList::IsEnabled(features::kRenderDocument))
    return render_document_level.Get();
  return RenderDocumentLevel::kDisabled;
}

std::string GetRenderDocumentLevelName(RenderDocumentLevel level) {
  return render_document_level.GetName(level);
}

bool CreateNewHostForSameSiteSubframe() {
  return GetRenderDocumentLevel() >= RenderDocumentLevel::kSubframe;
}

}  // namespace content
