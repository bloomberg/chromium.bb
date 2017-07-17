// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_RESOURCES_RESOURCE_FORMAT_UTILS_H_
#define COMPONENTS_VIZ_COMMON_RESOURCES_RESOURCE_FORMAT_UTILS_H_

#include "components/viz/common/quads/resource_format.h"
#include "components/viz/common/viz_common_export.h"
#include "third_party/skia/include/core/SkImageInfo.h"

namespace viz {

VIZ_COMMON_EXPORT SkColorType
ResourceFormatToClosestSkColorType(ResourceFormat format);
VIZ_COMMON_EXPORT int BitsPerPixel(ResourceFormat format);
VIZ_COMMON_EXPORT GLenum GLDataType(ResourceFormat format);
VIZ_COMMON_EXPORT GLenum GLDataFormat(ResourceFormat format);
VIZ_COMMON_EXPORT GLenum GLInternalFormat(ResourceFormat format);
VIZ_COMMON_EXPORT GLenum GLCopyTextureInternalFormat(ResourceFormat format);
VIZ_COMMON_EXPORT gfx::BufferFormat BufferFormat(ResourceFormat format);
VIZ_COMMON_EXPORT bool IsResourceFormatCompressed(ResourceFormat format);
VIZ_COMMON_EXPORT bool DoesResourceFormatSupportAlpha(ResourceFormat format);

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_RESOURCES_RESOURCE_FORMAT_UTILS_H_
