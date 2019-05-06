// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_BUNDLE_UTILS_H_
#define BASE_ANDROID_BUNDLE_UTILS_H_

#include <string>

#include "base/base_export.h"

namespace base {
namespace android {

// Utils to help working with android app bundles.
class BASE_EXPORT BundleUtils {
 public:
  // Returns true if the current build is a bundle.
  static bool IsBundle();

  // dlopen variant that works for native libraries in dynamic feature modules.
  static void* DlOpenModuleLibrary(const std::string& libary_name);
};

}  // namespace android
}  // namespace base

#endif  // BASE_ANDROID_BUNDLE_UTILS_H_
