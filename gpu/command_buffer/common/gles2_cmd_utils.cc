// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is here so other GLES2 related files can have a common set of
// includes where appropriate.

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gles2_command_buffer.h>

#include "../common/gles2_cmd_utils.h"
#include "../common/gles2_cmd_format.h"

namespace gpu {
namespace gles2 {

namespace gl_error_bit {
enum GLErrorBit {
  kNoError = 0,
  kInvalidEnum = (1 << 0),
  kInvalidValue = (1 << 1),
  kInvalidOperation = (1 << 2),
  kOutOfMemory = (1 << 3),
  kInvalidFrameBufferOperation = (1 << 4),
};
}

int GLES2Util::GLGetNumValuesReturned(int id) const {
  switch (id) {
    // -- glGetBooleanv, glGetFloatv, glGetIntergerv
    case GL_ACTIVE_TEXTURE:
      return 1;
    case GL_ALIASED_LINE_WIDTH_RANGE:
      return 2;
    case GL_ALIASED_POINT_SIZE_RANGE:
      return 2;
    case GL_ALPHA_BITS:
      return 1;
    case GL_ARRAY_BUFFER_BINDING:
      return 1;
    case GL_BLEND:
      return 1;
    case GL_BLEND_COLOR:
      return 4;
    case GL_BLEND_DST_ALPHA:
      return 1;
    case GL_BLEND_DST_RGB:
      return 1;
    case GL_BLEND_EQUATION_ALPHA:
      return 1;
    case GL_BLEND_EQUATION_RGB:
      return 1;
    case GL_BLEND_SRC_ALPHA:
      return 1;
    case GL_BLEND_SRC_RGB:
      return 1;
    case GL_BLUE_BITS:
      return 1;
    case GL_COLOR_CLEAR_VALUE:
      return 4;
    case GL_COLOR_WRITEMASK:
      return 4;
    case GL_COMPRESSED_TEXTURE_FORMATS:
      return num_compressed_texture_formats_;
    case GL_CULL_FACE:
      return 1;
    case GL_CULL_FACE_MODE:
      return 1;
    case GL_CURRENT_PROGRAM:
      return 1;
    case GL_DEPTH_BITS:
      return 1;
    case GL_DEPTH_CLEAR_VALUE:
      return 1;
    case GL_DEPTH_FUNC:
      return 1;
    case GL_DEPTH_RANGE:
      return 2;
    case GL_DEPTH_TEST:
      return 1;
    case GL_DEPTH_WRITEMASK:
      return 1;
    case GL_DITHER:
      return 1;
    case GL_ELEMENT_ARRAY_BUFFER_BINDING:
      return 1;
    case GL_FRAMEBUFFER_BINDING:
      return 1;
    case GL_FRONT_FACE:
      return 1;
    case GL_GENERATE_MIPMAP_HINT:
      return 1;
    case GL_GREEN_BITS:
      return 1;
    case GL_IMPLEMENTATION_COLOR_READ_FORMAT:
      return 1;
    case GL_IMPLEMENTATION_COLOR_READ_TYPE:
      return 1;
    case GL_LINE_WIDTH:
      return 1;
    case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS:
      return 1;
    case GL_MAX_CUBE_MAP_TEXTURE_SIZE:
      return 1;
    case GL_MAX_FRAGMENT_UNIFORM_VECTORS:
      return 1;
    case GL_MAX_RENDERBUFFER_SIZE:
      return 1;
    case GL_MAX_TEXTURE_IMAGE_UNITS:
      return 1;
    case GL_MAX_TEXTURE_SIZE:
      return 1;
    case GL_MAX_VARYING_VECTORS:
      return 1;
    case GL_MAX_VERTEX_ATTRIBS:
      return 1;
    case GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS:
      return 1;
    case GL_MAX_VERTEX_UNIFORM_VECTORS:
      return 1;
    case GL_MAX_VIEWPORT_DIMS:
      return 2;
    case GL_NUM_COMPRESSED_TEXTURE_FORMATS:
      return 1;
    case GL_PACK_ALIGNMENT:
      return 1;
    case GL_POLYGON_OFFSET_FACTOR:
      return 1;
    case GL_POLYGON_OFFSET_FILL:
      return 1;
    case GL_POLYGON_OFFSET_UNITS:
      return 1;
    case GL_RED_BITS:
      return 1;
    case GL_RENDERBUFFER_BINDING:
      return 1;
    case GL_SAMPLE_BUFFERS:
      return 1;
    case GL_SAMPLE_COVERAGE_INVERT:
      return 1;
    case GL_SAMPLE_COVERAGE_VALUE:
      return 1;
    case GL_SAMPLES:
      return 1;
    case GL_SCISSOR_BOX:
      return 4;
    case GL_SCISSOR_TEST:
      return 1;
    case GL_SHADER_COMPILER:
      return 1;
    case GL_STENCIL_BACK_FAIL:
      return 1;
    case GL_STENCIL_BACK_FUNC:
      return 1;
    case GL_STENCIL_BACK_PASS_DEPTH_FAIL:
      return 1;
    case GL_STENCIL_BACK_PASS_DEPTH_PASS:
      return 1;
    case GL_STENCIL_BACK_REF:
      return 1;
    case GL_STENCIL_BACK_VALUE_MASK:
      return 1;
    case GL_STENCIL_BACK_WRITEMASK:
      return 1;
    case GL_STENCIL_BITS:
      return 1;
    case GL_STENCIL_CLEAR_VALUE:
      return 1;
    case GL_STENCIL_FAIL:
      return 1;
    case GL_STENCIL_FUNC:
      return 1;
    case GL_STENCIL_PASS_DEPTH_FAIL:
      return 1;
    case GL_STENCIL_PASS_DEPTH_PASS:
      return 1;
    case GL_STENCIL_REF:
      return 1;
    case GL_STENCIL_TEST:
      return 1;
    case GL_STENCIL_VALUE_MASK:
      return 1;
    case GL_STENCIL_WRITEMASK:
      return 1;
    case GL_SUBPIXEL_BITS:
      return 1;
    case GL_TEXTURE_BINDING_2D:
      return 1;
    case GL_TEXTURE_BINDING_CUBE_MAP:
      return 1;
    case GL_UNPACK_ALIGNMENT:
      return 1;
    case GL_VIEWPORT:
      return 4;
    // -- glGetBooleanv, glGetFloatv, glGetIntergerv with
    //    GL_CHROMIUM_framebuffer_multisample
    case GL_MAX_SAMPLES_EXT:
      return 1;

    // -- glGetBufferParameteriv
    case GL_BUFFER_SIZE:
      return 1;
    case GL_BUFFER_USAGE:
      return 1;

    // -- glGetFramebufferAttachmentParameteriv
    case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
      return 1;
    case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
      return 1;
    case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL:
      return 1;
    case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE:
      return 1;

    // -- glGetFramebufferAttachmentParameteriv
    case GL_DELETE_STATUS:
      return 1;
    case GL_LINK_STATUS:
      return 1;
    case GL_VALIDATE_STATUS:
      return 1;
    case GL_INFO_LOG_LENGTH:
      return 1;
    case GL_ATTACHED_SHADERS:
      return 1;
    case GL_ACTIVE_ATTRIBUTES:
      return 1;
    case GL_ACTIVE_ATTRIBUTE_MAX_LENGTH:
      return 1;
    case GL_ACTIVE_UNIFORMS:
      return 1;
    case GL_ACTIVE_UNIFORM_MAX_LENGTH:
      return 1;


    // -- glGetRenderbufferAttachmentParameteriv
    case GL_RENDERBUFFER_WIDTH:
      return 1;
    case GL_RENDERBUFFER_HEIGHT:
      return 1;
    case GL_RENDERBUFFER_INTERNAL_FORMAT:
      return 1;
    case GL_RENDERBUFFER_RED_SIZE:
      return 1;
    case GL_RENDERBUFFER_GREEN_SIZE:
      return 1;
    case GL_RENDERBUFFER_BLUE_SIZE:
      return 1;
    case GL_RENDERBUFFER_ALPHA_SIZE:
      return 1;
    case GL_RENDERBUFFER_DEPTH_SIZE:
      return 1;
    case GL_RENDERBUFFER_STENCIL_SIZE:
      return 1;

    // -- glGetShaderiv
    case GL_SHADER_TYPE:
      return 1;
    // Already defined under glGetFramebufferAttachemntParameteriv.
    // case GL_DELETE_STATUS:
    //   return 1;
    case GL_COMPILE_STATUS:
      return 1;
    // Already defined under glGetFramebufferAttachemntParameteriv.
    // case GL_INFO_LOG_LENGTH:
    //   return 1;
    case GL_SHADER_SOURCE_LENGTH:
      return 1;

    // -- glGetTexParameterfv, glGetTexParameteriv
    case GL_TEXTURE_MAG_FILTER:
      return 1;
    case GL_TEXTURE_MIN_FILTER:
      return 1;
    case GL_TEXTURE_WRAP_S:
      return 1;
    case GL_TEXTURE_WRAP_T:
      return 1;

    // -- glGetVertexAttribfv, glGetVertexAttribiv
    case GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING:
      return 1;
    case GL_VERTEX_ATTRIB_ARRAY_ENABLED:
      return 1;
    case GL_VERTEX_ATTRIB_ARRAY_SIZE:
      return 1;
    case GL_VERTEX_ATTRIB_ARRAY_STRIDE:
      return 1;
    case GL_VERTEX_ATTRIB_ARRAY_TYPE:
      return 1;
    case GL_VERTEX_ATTRIB_ARRAY_NORMALIZED:
      return 1;
    case GL_CURRENT_VERTEX_ATTRIB:
      return 4;

    // -- glHint with GL_OES_standard_derivatives
    case GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES:
      return 1;

    // bad enum
    default:
      return 0;
  }
}

namespace {

// Return the number of elements per group of a specified format.
int ElementsPerGroup(int format, int type) {
  switch (type) {
    case GL_UNSIGNED_SHORT_5_6_5:
    case GL_UNSIGNED_SHORT_4_4_4_4:
    case GL_UNSIGNED_SHORT_5_5_5_1:
    case GL_UNSIGNED_INT_24_8_OES:
       return 1;
    default:
       break;
    }

    switch (format) {
    case GL_RGB:
       return 3;
    case GL_LUMINANCE_ALPHA:
    case GL_RGBA:
    case GL_BGRA_EXT:
       return 4;
    case GL_ALPHA:
    case GL_LUMINANCE:
    case GL_DEPTH_COMPONENT:
    case GL_DEPTH_STENCIL_OES:
       return 1;
    default:
       return 0;
  }
}

// Return the number of bytes per element, based on the element type.
int BytesPerElement(int type) {
  switch (type) {
    case GL_FLOAT:
    case GL_UNSIGNED_INT_24_8_OES:
      return 4;
    case GL_HALF_FLOAT_OES:
    case GL_UNSIGNED_SHORT:
    case GL_SHORT:
    case GL_UNSIGNED_SHORT_5_6_5:
    case GL_UNSIGNED_SHORT_4_4_4_4:
    case GL_UNSIGNED_SHORT_5_5_5_1:
       return 2;
    case GL_UNSIGNED_BYTE:
    case GL_BYTE:
       return 1;
    default:
       return 0;
  }
}

}  // anonymous namespace

// Returns the amount of data glTexImage2D or glTexSubImage2D will access.
bool GLES2Util::ComputeImageDataSize(
    int width, int height, int format, int type, int unpack_alignment,
    uint32* size) {
  uint32 bytes_per_group =
      BytesPerElement(type) * ElementsPerGroup(format, type);
  uint32 row_size;
  if (!SafeMultiplyUint32(width, bytes_per_group, &row_size)) {
    return false;
  }
  if (height > 1) {
    uint32 temp;
    if (!SafeAddUint32(row_size, unpack_alignment - 1, &temp)) {
      return false;
    }
    uint32 padded_row_size = (temp / unpack_alignment) * unpack_alignment;
    uint32 size_of_all_but_last_row;
    if (!SafeMultiplyUint32((height - 1), padded_row_size,
                            &size_of_all_but_last_row)) {
      return false;
    }
    if (!SafeAddUint32(size_of_all_but_last_row, row_size, size)) {
      return false;
    }
  } else {
    if (!SafeMultiplyUint32(height, row_size, size)) {
      return false;
    }
  }
  return true;
}

uint32 GLES2Util::GetGLDataTypeSizeForUniforms(int type) {
  switch (type) {
    case GL_FLOAT:
      return sizeof(GLfloat);              // NOLINT
    case GL_FLOAT_VEC2:
      return sizeof(GLfloat) * 2;          // NOLINT
    case GL_FLOAT_VEC3:
      return sizeof(GLfloat) * 3;          // NOLINT
    case GL_FLOAT_VEC4:
      return sizeof(GLfloat) * 4;          // NOLINT
    case GL_INT:
      return sizeof(GLint);                // NOLINT
    case GL_INT_VEC2:
      return sizeof(GLint) * 2;            // NOLINT
    case GL_INT_VEC3:
      return sizeof(GLint) * 3;            // NOLINT
    case GL_INT_VEC4:
      return sizeof(GLint) * 4;            // NOLINT
    case GL_BOOL:
      return sizeof(GLint);                // NOLINT
    case GL_BOOL_VEC2:
      return sizeof(GLint) * 2;            // NOLINT
    case GL_BOOL_VEC3:
      return sizeof(GLint) * 3;            // NOLINT
    case GL_BOOL_VEC4:
      return sizeof(GLint) * 4;            // NOLINT
    case GL_FLOAT_MAT2:
      return sizeof(GLfloat) * 2 * 2;      // NOLINT
    case GL_FLOAT_MAT3:
      return sizeof(GLfloat) * 3 * 3;      // NOLINT
    case GL_FLOAT_MAT4:
      return sizeof(GLfloat) * 4 * 4;      // NOLINT
    case GL_SAMPLER_2D:
      return sizeof(GLint);                // NOLINT
    case GL_SAMPLER_CUBE:
      return sizeof(GLint);                // NOLINT
    default:
      return 0;
  }
}

size_t GLES2Util::GetGLTypeSizeForTexturesAndBuffers(uint32 type) {
  switch (type) {
    case GL_BYTE:
      return sizeof(GLbyte);  // NOLINT
    case GL_UNSIGNED_BYTE:
      return sizeof(GLubyte);  // NOLINT
    case GL_SHORT:
      return sizeof(GLshort);  // NOLINT
    case GL_UNSIGNED_SHORT:
      return sizeof(GLushort);  // NOLINT
    case GL_INT:
      return sizeof(GLint);  // NOLINT
    case GL_UNSIGNED_INT:
      return sizeof(GLuint);  // NOLINT
    case GL_FLOAT:
      return sizeof(GLfloat);  // NOLINT
    case GL_FIXED:
      return sizeof(GLfixed);  // NOLINT
    default:
      return 0;
  }
}

uint32 GLES2Util::GLErrorToErrorBit(uint32 error) {
  switch (error) {
    case GL_INVALID_ENUM:
      return gl_error_bit::kInvalidEnum;
    case GL_INVALID_VALUE:
      return gl_error_bit::kInvalidValue;
    case GL_INVALID_OPERATION:
      return gl_error_bit::kInvalidOperation;
    case GL_OUT_OF_MEMORY:
      return gl_error_bit::kOutOfMemory;
    case GL_INVALID_FRAMEBUFFER_OPERATION:
      return gl_error_bit::kInvalidFrameBufferOperation;
    default:
      GPU_NOTREACHED();
      return gl_error_bit::kNoError;
  }
}

uint32 GLES2Util::GLErrorBitToGLError(uint32 error_bit) {
  switch (error_bit) {
    case gl_error_bit::kInvalidEnum:
      return GL_INVALID_ENUM;
    case gl_error_bit::kInvalidValue:
      return GL_INVALID_VALUE;
    case gl_error_bit::kInvalidOperation:
      return GL_INVALID_OPERATION;
    case gl_error_bit::kOutOfMemory:
      return GL_OUT_OF_MEMORY;
    case gl_error_bit::kInvalidFrameBufferOperation:
      return GL_INVALID_FRAMEBUFFER_OPERATION;
    default:
      GPU_NOTREACHED();
      return GL_NO_ERROR;
  }
}

uint32 GLES2Util::IndexToGLFaceTarget(int index) {
  static uint32 faces[] = {
    GL_TEXTURE_CUBE_MAP_POSITIVE_X,
    GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
    GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
    GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
    GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
    GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
  };
  return faces[index];
}

uint32 GLES2Util::GetChannelsForFormat(int format) {
  switch (format) {
    case GL_ALPHA:
      return 0x0008;
    case GL_LUMINANCE:
      return 0x0007;
    case GL_LUMINANCE_ALPHA:
      return 0x000F;
    case GL_RGB:
    case GL_RGB565:
      return 0x0007;
    case GL_RGBA:
    case GL_RGBA4:
    case GL_RGB5_A1:
      return 0x000F;
    default:
      return 0x0000;
  }
}

}  // namespace gles2
}  // namespace gpu

