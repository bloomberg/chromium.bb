// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_RESOURCES_RESOURCE_FORMAT_UTILS_H_
#define COMPONENTS_VIZ_COMMON_RESOURCES_RESOURCE_FORMAT_UTILS_H_

#include "components/viz/common/quads/resource_format.h"
#include "third_party/skia/include/core/SkImageInfo.h"

namespace viz {

SkColorType ResourceFormatToClosestSkColorType(ResourceFormat format);
int BitsPerPixel(ResourceFormat format);
GLenum GLDataType(ResourceFormat format);
GLenum GLDataFormat(ResourceFormat format);
GLenum GLInternalFormat(ResourceFormat format);
GLenum GLCopyTextureInternalFormat(ResourceFormat format);
gfx::BufferFormat BufferFormat(ResourceFormat format);
bool IsResourceFormatCompressed(ResourceFormat format);
bool DoesResourceFormatSupportAlpha(ResourceFormat format);

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_RESOURCES_RESOURCE_FORMAT_UTILS_H_
