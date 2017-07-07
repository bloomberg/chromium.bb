// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/resources/resource_format_utils.h"

#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace viz {

SkColorType ResourceFormatToClosestSkColorType(ResourceFormat format) {
  // Use kN32_SkColorType if there is no corresponding SkColorType.
  switch (format) {
    case RGBA_4444:
      return kARGB_4444_SkColorType;
    case RGBA_8888:
    case BGRA_8888:
      return kN32_SkColorType;
    case ALPHA_8:
      return kAlpha_8_SkColorType;
    case RGB_565:
      return kRGB_565_SkColorType;
    case LUMINANCE_8:
      return kGray_8_SkColorType;
    case ETC1:
    case RED_8:
    case LUMINANCE_F16:
      return kN32_SkColorType;
    case RGBA_F16:
      return kRGBA_F16_SkColorType;
  }
  NOTREACHED();
  return kN32_SkColorType;
}

int BitsPerPixel(ResourceFormat format) {
  switch (format) {
    case RGBA_F16:
      return 64;
    case BGRA_8888:
    case RGBA_8888:
      return 32;
    case RGBA_4444:
    case RGB_565:
    case LUMINANCE_F16:
      return 16;
    case ALPHA_8:
    case LUMINANCE_8:
    case RED_8:
      return 8;
    case ETC1:
      return 4;
  }
  NOTREACHED();
  return 0;
}

GLenum GLDataType(ResourceFormat format) {
  DCHECK_LE(format, RESOURCE_FORMAT_MAX);
  static const GLenum format_gl_data_type[] = {
      GL_UNSIGNED_BYTE,           // RGBA_8888
      GL_UNSIGNED_SHORT_4_4_4_4,  // RGBA_4444
      GL_UNSIGNED_BYTE,           // BGRA_8888
      GL_UNSIGNED_BYTE,           // ALPHA_8
      GL_UNSIGNED_BYTE,           // LUMINANCE_8
      GL_UNSIGNED_SHORT_5_6_5,    // RGB_565,
      GL_UNSIGNED_BYTE,           // ETC1
      GL_UNSIGNED_BYTE,           // RED_8
      GL_HALF_FLOAT_OES,          // LUMINANCE_F16
      GL_HALF_FLOAT_OES,          // RGBA_F16
  };
  static_assert(arraysize(format_gl_data_type) == (RESOURCE_FORMAT_MAX + 1),
                "format_gl_data_type does not handle all cases.");

  return format_gl_data_type[format];
}

GLenum GLDataFormat(ResourceFormat format) {
  DCHECK_LE(format, RESOURCE_FORMAT_MAX);
  static const GLenum format_gl_data_format[] = {
      GL_RGBA,           // RGBA_8888
      GL_RGBA,           // RGBA_4444
      GL_BGRA_EXT,       // BGRA_8888
      GL_ALPHA,          // ALPHA_8
      GL_LUMINANCE,      // LUMINANCE_8
      GL_RGB,            // RGB_565
      GL_ETC1_RGB8_OES,  // ETC1
      GL_RED_EXT,        // RED_8
      GL_LUMINANCE,      // LUMINANCE_F16
      GL_RGBA,           // RGBA_F16
  };
  static_assert(arraysize(format_gl_data_format) == (RESOURCE_FORMAT_MAX + 1),
                "format_gl_data_format does not handle all cases.");
  return format_gl_data_format[format];
}

GLenum GLInternalFormat(ResourceFormat format) {
  // In GLES2, the internal format must match the texture format. (It no longer
  // is true in GLES3, however it still holds for the BGRA extension.)
  return GLDataFormat(format);
}

GLenum GLCopyTextureInternalFormat(ResourceFormat format) {
  // In GLES2, valid formats for glCopyTexImage2D are: GL_ALPHA, GL_LUMINANCE,
  // GL_LUMINANCE_ALPHA, GL_RGB, or GL_RGBA.
  // Extensions typically used for glTexImage2D do not also work for
  // glCopyTexImage2D. For instance GL_BGRA_EXT may not be used for
  // anything but gl(Sub)TexImage2D:
  // https://www.khronos.org/registry/gles/extensions/EXT/EXT_texture_format_BGRA8888.txt
  DCHECK_LE(format, RESOURCE_FORMAT_MAX);
  static const GLenum format_gl_data_format[] = {
      GL_RGBA,       // RGBA_8888
      GL_RGBA,       // RGBA_4444
      GL_RGBA,       // BGRA_8888
      GL_ALPHA,      // ALPHA_8
      GL_LUMINANCE,  // LUMINANCE_8
      GL_RGB,        // RGB_565
      GL_RGB,        // ETC1
      GL_LUMINANCE,  // RED_8
      GL_LUMINANCE,  // LUMINANCE_F16
      GL_RGBA,       // RGBA_F16
  };
  static_assert(arraysize(format_gl_data_format) == (RESOURCE_FORMAT_MAX + 1),
                "format_gl_data_format does not handle all cases.");
  return format_gl_data_format[format];
}

gfx::BufferFormat BufferFormat(ResourceFormat format) {
  switch (format) {
    case BGRA_8888:
      return gfx::BufferFormat::BGRA_8888;
    case RED_8:
      return gfx::BufferFormat::R_8;
    case RGBA_4444:
      return gfx::BufferFormat::RGBA_4444;
    case RGBA_8888:
      return gfx::BufferFormat::RGBA_8888;
    case ETC1:
      return gfx::BufferFormat::ETC1;
    case RGBA_F16:
      return gfx::BufferFormat::RGBA_F16;
    case ALPHA_8:
    case LUMINANCE_8:
    case RGB_565:
    case LUMINANCE_F16:
      break;
  }
  NOTREACHED();
  return gfx::BufferFormat::RGBA_8888;
}

bool IsResourceFormatCompressed(ResourceFormat format) {
  return format == ETC1;
}

bool DoesResourceFormatSupportAlpha(ResourceFormat format) {
  switch (format) {
    case RGBA_4444:
    case RGBA_8888:
    case BGRA_8888:
    case ALPHA_8:
    case RGBA_F16:
      return true;
    case LUMINANCE_8:
    case RGB_565:
    case ETC1:
    case RED_8:
    case LUMINANCE_F16:
      return false;
  }
  NOTREACHED();
  return false;
}

}  // namespace viz
