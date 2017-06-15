// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_VR_METRICS_UTIL_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_VR_METRICS_UTIL_H_

#include "base/macros.h"

#include "chrome/browser/android/vr_shell/vr_core_info.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr_types.h"

namespace vr_shell {

enum class ViewerType {
  UNKNOWN_TYPE = 0,
  CARDBOARD = 1,
  DAYDREAM = 2,
  VIEWER_TYPE_MAX,
};

class VrMetricsUtil {
 public:
  static void LogGvrVersionForVrViewerType(gvr::ViewerType viewer_type,
                                           const VrCoreInfo& vr_core_info);
  static void LogVrViewerType(gvr::ViewerType viewer_type);

 private:
  static ViewerType GetVrViewerType(gvr::ViewerType viewer_type);

  static bool has_logged_vr_runtime_version_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(VrMetricsUtil);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_VR_METRICS_UTIL_H_
