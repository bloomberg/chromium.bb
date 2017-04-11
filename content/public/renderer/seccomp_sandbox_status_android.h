// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_SECCOMP_SANDBOX_STATUS_ANDROID_H_
#define CONTENT_PUBLIC_RENDERER_SECCOMP_SANDBOX_STATUS_ANDROID_H_

#include "content/common/content_export.h"

namespace content {

enum class SeccompSandboxStatus {
  NOT_SUPPORTED = 0,  // Seccomp is not supported.
  DETECTION_FAILED,   // Run-time detection of Seccomp+TSYNC failed.
  FEATURE_DISABLED,   // Sandbox was disabled by FeatureList.
  FEATURE_ENABLED,    // Sandbox was enabled by FeatureList.
  ENGAGED,            // Sandbox was enabled and successfully turned on.
  STATUS_MAX
  // This enum is used by an UMA histogram, so only append values.
};

// Gets the SeccompSandboxStatus of the current process.
CONTENT_EXPORT SeccompSandboxStatus GetSeccompSandboxStatus();

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_SECCOMP_SANDBOX_STATUS_ANDROID_H_
