// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_RUNNERS_COMMON_DETECT_VULKAN_HACK_H_
#define FUCHSIA_RUNNERS_COMMON_DETECT_VULKAN_HACK_H_

#include <fuchsia/web/cpp/fidl.h>

// Removes the VULKAN feature flag if Vulkan is not available on the host.
// TODO(https://crbug.com/1021844): Remove this workaround once our test
// environment provides Vulkan.
void DisableVulkanIfUnavailable(fuchsia::web::ContextFeatureFlags* features);

#endif  // FUCHSIA_RUNNERS_COMMON_DETECT_VULKAN_HACK_H_
