// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated. DO NOT EDIT!

#ifndef GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_VALIDATION_IMPLEMENTATION_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_VALIDATION_IMPLEMENTATION_AUTOGEN_H_

bool ValidateGLenumBufferParameter(GLenum value) {
  switch (value) {
    case GL_BUFFER_SIZE:
    case GL_BUFFER_USAGE:
      return true;
    default:
      return false;
  }
}

bool ValidateGLenumProgramParameter(GLenum value) {
  switch (value) {
    case GL_DELETE_STATUS:
    case GL_LINK_STATUS:
    case GL_VALIDATE_STATUS:
    case GL_INFO_LOG_LENGTH:
    case GL_ATTACHED_SHADERS:
    case GL_ACTIVE_ATTRIBUTES:
    case GL_ACTIVE_ATTRIBUTE_MAX_LENGTH:
    case GL_ACTIVE_UNIFORMS:
    case GL_ACTIVE_UNIFORM_MAX_LENGTH:
      return true;
    default:
      return false;
  }
}

bool ValidateGLenumFaceMode(GLenum value) {
  switch (value) {
    case GL_CW:
    case GL_CCW:
      return true;
    default:
      return false;
  }
}

bool ValidateGLenumShaderParameter(GLenum value) {
  switch (value) {
    case GL_SHADER_TYPE:
    case GL_DELETE_STATUS:
    case GL_COMPILE_STATUS:
    case GL_INFO_LOG_LENGTH:
    case GL_SHADER_SOURCE_LENGTH:
      return true;
    default:
      return false;
  }
}

bool ValidateGLenumIndexType(GLenum value) {
  switch (value) {
    case GL_UNSIGNED_BYTE:
    case GL_UNSIGNED_SHORT:
      return true;
    default:
      return false;
  }
}

bool ValidateGLenumVertexAttribute(GLenum value) {
  switch (value) {
    case GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING:
    case GL_VERTEX_ATTRIB_ARRAY_ENABLED:
    case GL_VERTEX_ATTRIB_ARRAY_SIZE:
    case GL_VERTEX_ATTRIB_ARRAY_STRIDE:
    case GL_VERTEX_ATTRIB_ARRAY_TYPE:
    case GL_VERTEX_ATTRIB_ARRAY_NORMALIZED:
    case GL_CURRENT_VERTEX_ATTRIB:
      return true;
    default:
      return false;
  }
}

bool ValidateGLenumPixelType(GLenum value) {
  switch (value) {
    case GL_UNSIGNED_BYTE:
    case GL_UNSIGNED_SHORT_5_6_5:
    case GL_UNSIGNED_SHORT_4_4_4_4:
    case GL_UNSIGNED_SHORT_5_5_5_1:
      return true;
    default:
      return false;
  }
}

bool ValidateGLenumEquation(GLenum value) {
  switch (value) {
    case GL_FUNC_ADD:
    case GL_FUNC_SUBTRACT:
    case GL_FUNC_REVERSE_SUBTRACT:
      return true;
    default:
      return false;
  }
}

bool ValidateGLenumRenderBufferTarget(GLenum value) {
  switch (value) {
    case GL_RENDERBUFFER:
      return true;
    default:
      return false;
  }
}

bool ValidateGLenumShaderPercision(GLenum value) {
  switch (value) {
    case GL_LOW_FLOAT:
    case GL_MEDIUM_FLOAT:
    case GL_HIGH_FLOAT:
    case GL_LOW_INT:
    case GL_MEDIUM_INT:
    case GL_HIGH_INT:
      return true;
    default:
      return false;
  }
}

bool ValidateGLenumReadPixelFormat(GLenum value) {
  switch (value) {
    case GL_ALPHA:
    case GL_RGB:
    case GL_RGBA:
      return true;
    default:
      return false;
  }
}

bool ValidateGLenumTextureBindTarget(GLenum value) {
  switch (value) {
    case GL_TEXTURE_2D:
    case GL_TEXTURE_CUBE_MAP:
      return true;
    default:
      return false;
  }
}

bool ValidateGLenumCmpFunction(GLenum value) {
  switch (value) {
    case GL_NEVER:
    case GL_LESS:
    case GL_EQUAL:
    case GL_LEQUAL:
    case GL_GREATER:
    case GL_NOTEQUAL:
    case GL_GEQUAL:
    case GL_ALWAYS:
      return true;
    default:
      return false;
  }
}

bool ValidateGLenumCapability(GLenum value) {
  switch (value) {
    case GL_BLEND:
    case GL_CULL_FACE:
    case GL_DEPTH_TEST:
    case GL_DITHER:
    case GL_POLYGON_OFFSET_FILL:
    case GL_SAMPLE_ALPHA_TO_COVERAGE:
    case GL_SAMPLE_COVERAGE:
    case GL_SCISSOR_TEST:
    case GL_STENCIL_TEST:
      return true;
    default:
      return false;
  }
}

bool ValidateGLenumDstBlendFactor(GLenum value) {
  switch (value) {
    case GL_ZERO:
    case GL_ONE:
    case GL_SRC_COLOR:
    case GL_ONE_MINUS_SRC_COLOR:
    case GL_DST_COLOR:
    case GL_ONE_MINUS_DST_COLOR:
    case GL_SRC_ALPHA:
    case GL_ONE_MINUS_SRC_ALPHA:
    case GL_DST_ALPHA:
    case GL_ONE_MINUS_DST_ALPHA:
    case GL_CONSTANT_COLOR:
    case GL_ONE_MINUS_CONSTANT_COLOR:
    case GL_CONSTANT_ALPHA:
    case GL_ONE_MINUS_CONSTANT_ALPHA:
      return true;
    default:
      return false;
  }
}

bool ValidateGLenumAttachment(GLenum value) {
  switch (value) {
    case GL_COLOR_ATTACHMENT0:
    case GL_DEPTH_ATTACHMENT:
    case GL_STENCIL_ATTACHMENT:
      return true;
    default:
      return false;
  }
}

bool ValidateGLenumRenderBufferFormat(GLenum value) {
  switch (value) {
    case GL_RGBA4:
    case GL_RGB565:
    case GL_RGB5_A1:
    case GL_DEPTH_COMPONENT16:
    case GL_STENCIL_INDEX8:
      return true;
    default:
      return false;
  }
}

bool ValidateGLenumDrawMode(GLenum value) {
  switch (value) {
    case GL_POINTS:
    case GL_LINE_STRIP:
    case GL_LINE_LOOP:
    case GL_LINES:
    case GL_TRIANGLE_STRIP:
    case GL_TRIANGLE_FAN:
    case GL_TRIANGLES:
      return true;
    default:
      return false;
  }
}

bool ValidateGLenumBufferUsage(GLenum value) {
  switch (value) {
    case GL_STREAM_DRAW:
    case GL_STATIC_DRAW:
    case GL_DYNAMIC_DRAW:
      return true;
    default:
      return false;
  }
}

bool ValidateGLenumHintTarget(GLenum value) {
  switch (value) {
    case GL_GENERATE_MIPMAP_HINT:
      return true;
    default:
      return false;
  }
}

bool ValidateGLenumHintMode(GLenum value) {
  switch (value) {
    case GL_FASTEST:
    case GL_NICEST:
    case GL_DONT_CARE:
      return true;
    default:
      return false;
  }
}

bool ValidateGLenumTextureFormat(GLenum value) {
  switch (value) {
    case GL_ALPHA:
    case GL_LUMINANCE:
    case GL_LUMINANCE_ALPHA:
    case GL_RGB:
    case GL_RGBA:
      return true;
    default:
      return false;
  }
}

bool ValidateGLenumStringType(GLenum value) {
  switch (value) {
    case GL_VENDOR:
    case GL_RENDERER:
    case GL_VERSION:
    case GL_SHADING_LANGUAGE_VERSION:
    case GL_EXTENSIONS:
      return true;
    default:
      return false;
  }
}

bool ValidateGLenumFrameBufferTarget(GLenum value) {
  switch (value) {
    case GL_FRAMEBUFFER:
      return true;
    default:
      return false;
  }
}

bool ValidateGLbooleanFalse(GLenum value) {
  switch (value) {
    case false:
      return true;
    default:
      return false;
  }
}

bool ValidateGLintPixelStoreAlignment(GLenum value) {
  switch (value) {
    case 1:
    case 2:
    case 4:
    case 8:
      return true;
    default:
      return false;
  }
}

bool ValidateGLenumTextureParameter(GLenum value) {
  switch (value) {
    case GL_TEXTURE_MAG_FILTER:
    case GL_TEXTURE_MIN_FILTER:
    case GL_TEXTURE_WRAP_S:
    case GL_TEXTURE_WRAP_T:
      return true;
    default:
      return false;
  }
}

bool ValidateGLenumTextureTarget(GLenum value) {
  switch (value) {
    case GL_TEXTURE_2D:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
      return true;
    default:
      return false;
  }
}

bool ValidateGLenumShaderType(GLenum value) {
  switch (value) {
    case GL_VERTEX_SHADER:
    case GL_FRAGMENT_SHADER:
      return true;
    default:
      return false;
  }
}

bool ValidateGLenumFaceType(GLenum value) {
  switch (value) {
    case GL_FRONT:
    case GL_BACK:
    case GL_FRONT_AND_BACK:
      return true;
    default:
      return false;
  }
}

bool ValidateGLenumSrcBlendFactor(GLenum value) {
  switch (value) {
    case GL_ZERO:
    case GL_ONE:
    case GL_SRC_COLOR:
    case GL_ONE_MINUS_SRC_COLOR:
    case GL_DST_COLOR:
    case GL_ONE_MINUS_DST_COLOR:
    case GL_SRC_ALPHA:
    case GL_ONE_MINUS_SRC_ALPHA:
    case GL_DST_ALPHA:
    case GL_ONE_MINUS_DST_ALPHA:
    case GL_CONSTANT_COLOR:
    case GL_ONE_MINUS_CONSTANT_COLOR:
    case GL_CONSTANT_ALPHA:
    case GL_ONE_MINUS_CONSTANT_ALPHA:
    case GL_SRC_ALPHA_SATURATE:
      return true;
    default:
      return false;
  }
}

bool ValidateGLenumVertexAttribType(GLenum value) {
  switch (value) {
    case GL_BYTE:
    case GL_UNSIGNED_BYTE:
    case GL_SHORT:
    case GL_UNSIGNED_SHORT:
    case GL_FLOAT:
      return true;
    default:
      return false;
  }
}

bool ValidateGLenumStencilOp(GLenum value) {
  switch (value) {
    case GL_KEEP:
    case GL_ZERO:
    case GL_REPLACE:
    case GL_INCR:
    case GL_INCR_WRAP:
    case GL_DECR:
    case GL_DECR_WRAP:
    case GL_INVERT:
      return true;
    default:
      return false;
  }
}

bool ValidateGLintVertexAttribSize(GLenum value) {
  switch (value) {
    case 1:
    case 2:
    case 3:
    case 4:
      return true;
    default:
      return false;
  }
}

bool ValidateGLenumBufferTarget(GLenum value) {
  switch (value) {
    case GL_ARRAY_BUFFER:
    case GL_ELEMENT_ARRAY_BUFFER:
      return true;
    default:
      return false;
  }
}

bool ValidateGLenumPixelStore(GLenum value) {
  switch (value) {
    case GL_PACK_ALIGNMENT:
    case GL_UNPACK_ALIGNMENT:
      return true;
    default:
      return false;
  }
}

bool ValidateGLenumRenderBufferParameter(GLenum value) {
  switch (value) {
    case GL_RENDERBUFFER_WIDTH:
    case GL_RENDERBUFFER_HEIGHT:
    case GL_RENDERBUFFER_INTERNAL_FORMAT:
    case GL_RENDERBUFFER_RED_SIZE:
    case GL_RENDERBUFFER_GREEN_SIZE:
    case GL_RENDERBUFFER_BLUE_SIZE:
    case GL_RENDERBUFFER_ALPHA_SIZE:
    case GL_RENDERBUFFER_DEPTH_SIZE:
    case GL_RENDERBUFFER_STENCIL_SIZE:
      return true;
    default:
      return false;
  }
}

bool ValidateGLenumFrameBufferParameter(GLenum value) {
  switch (value) {
    case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
    case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
    case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL:
    case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE:
      return true;
    default:
      return false;
  }
}

bool ValidateGLenumVertexPointer(GLenum value) {
  switch (value) {
    case GL_VERTEX_ATTRIB_ARRAY_POINTER:
      return true;
    default:
      return false;
  }
}

#endif  // GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_VALIDATION_IMPLEMENTATION_AUTOGEN_H_

