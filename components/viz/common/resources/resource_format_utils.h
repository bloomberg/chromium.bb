// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_RESOURCES_RESOURCE_FORMAT_UTILS_H_
#define COMPONENTS_VIZ_COMMON_RESOURCES_RESOURCE_FORMAT_UTILS_H_

#include "components/viz/common/resources/resource_format.h"
#include "components/viz/common/viz_resource_format_export.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/gpu/GrTypes.h"
#include "ui/gfx/buffer_types.h"

namespace viz {

VIZ_RESOURCE_FORMAT_EXPORT SkColorType
ResourceFormatToClosestSkColorType(ResourceFormat format);
VIZ_RESOURCE_FORMAT_EXPORT int BitsPerPixel(ResourceFormat format);

// The following functions use unsigned int instead of GLenum, since including
// third_party/khronos/GLES2/gl2.h causes redefinition errors as
// macros/functions defined in it conflict with macros/functions defined in
// ui/gl/gl_bindings.h. See http://crbug.com/512833 for more information.
VIZ_RESOURCE_FORMAT_EXPORT unsigned int GLDataType(ResourceFormat format);
VIZ_RESOURCE_FORMAT_EXPORT unsigned int GLDataFormat(ResourceFormat format);
VIZ_RESOURCE_FORMAT_EXPORT unsigned int GLInternalFormat(ResourceFormat format);
VIZ_RESOURCE_FORMAT_EXPORT unsigned int GLCopyTextureInternalFormat(
    ResourceFormat format);

VIZ_RESOURCE_FORMAT_EXPORT gfx::BufferFormat BufferFormat(
    ResourceFormat format);
VIZ_RESOURCE_FORMAT_EXPORT bool IsResourceFormatCompressed(
    ResourceFormat format);
VIZ_RESOURCE_FORMAT_EXPORT bool DoesResourceFormatSupportAlpha(
    ResourceFormat format);
VIZ_RESOURCE_FORMAT_EXPORT unsigned int TextureStorageFormat(
    ResourceFormat format);
VIZ_RESOURCE_FORMAT_EXPORT GrPixelConfig ToGrPixelConfig(ResourceFormat format);

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_RESOURCES_RESOURCE_FORMAT_UTILS_H_
