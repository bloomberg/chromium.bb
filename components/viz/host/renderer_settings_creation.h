// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_HOST_RENDERER_SETTINGS_CREATION_H_
#define COMPONENTS_VIZ_HOST_RENDERER_SETTINGS_CREATION_H_

#include <stdint.h>

#include "components/viz/common/resources/buffer_to_texture_target_map.h"
#include "components/viz/host/viz_host_export.h"

namespace viz {
class RendererSettings;
class ResourceSettings;
}  // namespace viz

namespace viz {

// |image_targets| is a map from every supported pair of GPU memory buffer
// usage/format to its GL texture target.
VIZ_HOST_EXPORT ResourceSettings CreateResourceSettings(
    const BufferUsageAndFormatList& texture_target_exception_list);

VIZ_HOST_EXPORT RendererSettings CreateRendererSettings(
    const BufferUsageAndFormatList& texture_target_exception_list);

}  // namespace viz

#endif  // COMPONENTS_VIZ_HOST_RENDERER_SETTINGS_CREATION_H_
