// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/vr_metrics_util.h"

#include "base/metrics/histogram_macros.h"

static constexpr int kVersionEncodingError = -4;
static constexpr int kVrNotSupported = -3;
static constexpr int kGvrNotInstalled = -2;
static constexpr int kGvrTooOld = -1;

namespace vr_shell {

bool VrMetricsUtil::has_logged_vr_runtime_version_ = false;

void VrMetricsUtil::LogGvrVersionForVrViewerType(
    gvr::ViewerType viewer_type,
    const VrCoreInfo& vr_core_info) {
  if (has_logged_vr_runtime_version_) {
    return;
  }

  uint32_t encoded_version = kVersionEncodingError;
  switch (vr_core_info.compatibility) {
    case VrCoreCompatibility::VR_CORE_COMPATIBILITY_VR_NOT_SUPPORTED:
      encoded_version = kVrNotSupported;
      break;
    case VrCoreCompatibility::VR_CORE_COMPATIBILITY_VR_NOT_AVAILABLE:
      encoded_version = kGvrNotInstalled;
      break;
    case VrCoreCompatibility::VR_CORE_COMPATIBILITY_VR_OUT_OF_DATE:
      if (vr_core_info.gvr_sdk_version.major == 0 &&
          vr_core_info.gvr_sdk_version.minor == 0 &&
          vr_core_info.gvr_sdk_version.patch == 0) {
        encoded_version = kGvrTooOld;
        break;
      }
    // Fall through since a version can be logged in this case.
    case VrCoreCompatibility::VR_CORE_COMPATIBILITY_VR_READY:
      encoded_version =
          std::min(vr_core_info.gvr_sdk_version.major, 999) * 1000 * 1000 +
          std::min(vr_core_info.gvr_sdk_version.minor, 999) * 1000 +
          std::min(vr_core_info.gvr_sdk_version.patch, 999);
      break;
  }

  switch (GetVrViewerType(viewer_type)) {
    case ViewerType::CARDBOARD:
      UMA_HISTOGRAM_SPARSE_SLOWLY("VRRuntimeVersion.GVR.Cardboard",
                                  encoded_version);
      break;
    case ViewerType::DAYDREAM:
      UMA_HISTOGRAM_SPARSE_SLOWLY("VRRuntimeVersion.GVR.Daydream",
                                  encoded_version);
      break;
    default:
      UMA_HISTOGRAM_SPARSE_SLOWLY("VRRuntimeVersion.GVR.Unknown",
                                  encoded_version);
      break;
  }

  has_logged_vr_runtime_version_ = true;
}

void VrMetricsUtil::LogVrViewerType(gvr::ViewerType viewer_type) {
  UMA_HISTOGRAM_ENUMERATION("VRViewerType",
                            static_cast<int>(GetVrViewerType(viewer_type)),
                            static_cast<int>(ViewerType::VIEWER_TYPE_MAX));
}

ViewerType VrMetricsUtil::GetVrViewerType(gvr::ViewerType viewer_type) {
  switch (viewer_type) {
    case gvr::ViewerType::GVR_VIEWER_TYPE_DAYDREAM:
      return ViewerType::DAYDREAM;
    case gvr::ViewerType::GVR_VIEWER_TYPE_CARDBOARD:
      return ViewerType::CARDBOARD;
    default:
      NOTREACHED();
      return ViewerType::UNKNOWN_TYPE;
  }
}

}  // namespace vr_shell
