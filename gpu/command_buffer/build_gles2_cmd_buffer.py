#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""code generator for GLES2 command buffers."""

import os
import os.path
import sys
import re
from optparse import OptionParser

_SIZE_OF_UINT32 = 4
_SIZE_OF_COMMAND_HEADER = 4
_FIRST_SPECIFIC_COMMAND_ID = 256

_LICENSE = """// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"""

_DO_NOT_EDIT_WARNING = """// This file is auto-generated from
// gpu/command_buffer/build_gles2_cmd_buffer.py
// DO NOT EDIT!

"""

# This string is copied directly out of the gl2.h file from GLES2.0
#
# Edits:
#
# *) Any argument that is a resourceID has been changed to GLid<Type>.
#    (not pointer arguments) and if it's allowed to be zero it's GLidZero<Type>
#    If it's allowed to not exist it's GLidBind<Type>
#
# *) All GLenums have been changed to GLenumTypeOfEnum
#
_GL_TYPES = {
  'GLenum': 'unsigned int',
  'GLboolean': 'unsigned char',
  'GLbitfield': 'unsigned int',
  'GLbyte': 'signed char',
  'GLshort': 'short',
  'GLint': 'int',
  'GLsizei': 'int',
  'GLubyte': 'unsigned char',
  'GLushort': 'unsigned short',
  'GLuint': 'unsigned int',
  'GLfloat': 'float',
  'GLclampf': 'float',
  'GLvoid': 'void',
  'GLfixed': 'int',
  'GLclampx': 'int',
  'GLintptr': 'long int',
  'GLsizeiptr': 'long int',
}

# This is a list of enum names and their valid values. It is used to map
# GLenum arguments to a specific set of valid values.
_ENUM_LISTS = {
  'BlitFilter': {
    'type': 'GLenum',
    'valid': [
      'GL_NEAREST',
      'GL_LINEAR',
    ],
    'invalid': [
      'GL_LINEAR_MIPMAP_LINEAR',
    ],
  },
  'FrameBufferTarget': {
    'type': 'GLenum',
    'valid': [
      'GL_FRAMEBUFFER',
    ],
    'invalid': [
      'GL_READ_FRAMEBUFFER' ,
    ],
  },
  'RenderBufferTarget': {
    'type': 'GLenum',
    'valid': [
      'GL_RENDERBUFFER',
    ],
    'invalid': [
      'GL_FRAMEBUFFER',
    ],
  },
  'BufferTarget': {
    'type': 'GLenum',
    'valid': [
      'GL_ARRAY_BUFFER',
      'GL_ELEMENT_ARRAY_BUFFER',
    ],
    'invalid': [
      'GL_RENDERBUFFER',
    ],
  },
  'BufferUsage': {
    'type': 'GLenum',
    'valid': [
      'GL_STREAM_DRAW',
      'GL_STATIC_DRAW',
      'GL_DYNAMIC_DRAW',
    ],
    'invalid': [
      'GL_STATIC_READ',
    ],
  },
  'CompressedTextureFormat': {
    'type': 'GLenum',
    'valid': [
    ],
  },
  'GLState': {
    'type': 'GLenum',
    'valid': [
      'GL_ACTIVE_TEXTURE',
      'GL_ALIASED_LINE_WIDTH_RANGE',
      'GL_ALIASED_POINT_SIZE_RANGE',
      'GL_ALPHA_BITS',
      'GL_ARRAY_BUFFER_BINDING',
      'GL_BLEND',
      'GL_BLEND_COLOR',
      'GL_BLEND_DST_ALPHA',
      'GL_BLEND_DST_RGB',
      'GL_BLEND_EQUATION_ALPHA',
      'GL_BLEND_EQUATION_RGB',
      'GL_BLEND_SRC_ALPHA',
      'GL_BLEND_SRC_RGB',
      'GL_BLUE_BITS',
      'GL_COLOR_CLEAR_VALUE',
      'GL_COLOR_WRITEMASK',
      'GL_COMPRESSED_TEXTURE_FORMATS',
      'GL_CULL_FACE',
      'GL_CULL_FACE_MODE',
      'GL_CURRENT_PROGRAM',
      'GL_DEPTH_BITS',
      'GL_DEPTH_CLEAR_VALUE',
      'GL_DEPTH_FUNC',
      'GL_DEPTH_RANGE',
      'GL_DEPTH_TEST',
      'GL_DEPTH_WRITEMASK',
      'GL_DITHER',
      'GL_ELEMENT_ARRAY_BUFFER_BINDING',
      'GL_FRAMEBUFFER_BINDING',
      'GL_FRONT_FACE',
      'GL_GENERATE_MIPMAP_HINT',
      'GL_GREEN_BITS',
      'GL_IMPLEMENTATION_COLOR_READ_FORMAT',
      'GL_IMPLEMENTATION_COLOR_READ_TYPE',
      'GL_LINE_WIDTH',
      'GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS',
      'GL_MAX_CUBE_MAP_TEXTURE_SIZE',
      'GL_MAX_FRAGMENT_UNIFORM_VECTORS',
      'GL_MAX_RENDERBUFFER_SIZE',
      'GL_MAX_TEXTURE_IMAGE_UNITS',
      'GL_MAX_TEXTURE_SIZE',
      'GL_MAX_VARYING_VECTORS',
      'GL_MAX_VERTEX_ATTRIBS',
      'GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS',
      'GL_MAX_VERTEX_UNIFORM_VECTORS',
      'GL_MAX_VIEWPORT_DIMS',
      'GL_NUM_COMPRESSED_TEXTURE_FORMATS',
      'GL_NUM_SHADER_BINARY_FORMATS',
      'GL_PACK_ALIGNMENT',
      'GL_POLYGON_OFFSET_FACTOR',
      'GL_POLYGON_OFFSET_FILL',
      'GL_POLYGON_OFFSET_UNITS',
      'GL_RED_BITS',
      'GL_RENDERBUFFER_BINDING',
      'GL_SAMPLE_BUFFERS',
      'GL_SAMPLE_COVERAGE_INVERT',
      'GL_SAMPLE_COVERAGE_VALUE',
      'GL_SAMPLES',
      'GL_SCISSOR_BOX',
      'GL_SCISSOR_TEST',
      'GL_SHADER_BINARY_FORMATS',
      'GL_SHADER_COMPILER',
      'GL_STENCIL_BACK_FAIL',
      'GL_STENCIL_BACK_FUNC',
      'GL_STENCIL_BACK_PASS_DEPTH_FAIL',
      'GL_STENCIL_BACK_PASS_DEPTH_PASS',
      'GL_STENCIL_BACK_REF',
      'GL_STENCIL_BACK_VALUE_MASK',
      'GL_STENCIL_BACK_WRITEMASK',
      'GL_STENCIL_BITS',
      'GL_STENCIL_CLEAR_VALUE',
      'GL_STENCIL_FAIL',
      'GL_STENCIL_FUNC',
      'GL_STENCIL_PASS_DEPTH_FAIL',
      'GL_STENCIL_PASS_DEPTH_PASS',
      'GL_STENCIL_REF',
      'GL_STENCIL_TEST',
      'GL_STENCIL_VALUE_MASK',
      'GL_STENCIL_WRITEMASK',
      'GL_SUBPIXEL_BITS',
      'GL_TEXTURE_BINDING_2D',
      'GL_TEXTURE_BINDING_CUBE_MAP',
      'GL_UNPACK_ALIGNMENT',
      'GL_VIEWPORT',
    ],
    'invalid': [
      'GL_FOG_HINT',
    ],
  },
  'GetTexParamTarget': {
    'type': 'GLenum',
    'valid': [
      'GL_TEXTURE_2D',
      'GL_TEXTURE_CUBE_MAP_POSITIVE_X',
      'GL_TEXTURE_CUBE_MAP_NEGATIVE_X',
      'GL_TEXTURE_CUBE_MAP_POSITIVE_Y',
      'GL_TEXTURE_CUBE_MAP_NEGATIVE_Y',
      'GL_TEXTURE_CUBE_MAP_POSITIVE_Z',
      'GL_TEXTURE_CUBE_MAP_NEGATIVE_Z',
    ],
    'invalid': [
      'GL_PROXY_TEXTURE_CUBE_MAP',
    ]
  },
  'TextureTarget': {
    'type': 'GLenum',
    'valid': [
      'GL_TEXTURE_2D',
      'GL_TEXTURE_CUBE_MAP_POSITIVE_X',
      'GL_TEXTURE_CUBE_MAP_NEGATIVE_X',
      'GL_TEXTURE_CUBE_MAP_POSITIVE_Y',
      'GL_TEXTURE_CUBE_MAP_NEGATIVE_Y',
      'GL_TEXTURE_CUBE_MAP_POSITIVE_Z',
      'GL_TEXTURE_CUBE_MAP_NEGATIVE_Z',
    ],
    'invalid': [
      'GL_PROXY_TEXTURE_CUBE_MAP',
    ]
  },
  'TextureBindTarget': {
    'type': 'GLenum',
    'valid': [
      'GL_TEXTURE_2D',
      'GL_TEXTURE_CUBE_MAP',
    ],
    'invalid': [
      'GL_TEXTURE_1D',
      'GL_TEXTURE_3D',
    ],
  },
  'ShaderType': {
    'type': 'GLenum',
    'valid': [
      'GL_VERTEX_SHADER',
      'GL_FRAGMENT_SHADER',
    ],
    'invalid': [
      'GL_GEOMETRY_SHADER',
    ],
  },
  'FaceType': {
    'type': 'GLenum',
    'valid': [
      'GL_FRONT',
      'GL_BACK',
      'GL_FRONT_AND_BACK',
    ],
  },
  'FaceMode': {
    'type': 'GLenum',
    'valid': [
      'GL_CW',
      'GL_CCW',
    ],
  },
  'CmpFunction': {
    'type': 'GLenum',
    'valid': [
      'GL_NEVER',
      'GL_LESS',
      'GL_EQUAL',
      'GL_LEQUAL',
      'GL_GREATER',
      'GL_NOTEQUAL',
      'GL_GEQUAL',
      'GL_ALWAYS',
    ],
  },
  'Equation': {
    'type': 'GLenum',
    'valid': [
      'GL_FUNC_ADD',
      'GL_FUNC_SUBTRACT',
      'GL_FUNC_REVERSE_SUBTRACT',
    ],
    'invalid': [
      'GL_MIN',
      'GL_MAX',
    ],
  },
  'SrcBlendFactor': {
    'type': 'GLenum',
    'valid': [
      'GL_ZERO',
      'GL_ONE',
      'GL_SRC_COLOR',
      'GL_ONE_MINUS_SRC_COLOR',
      'GL_DST_COLOR',
      'GL_ONE_MINUS_DST_COLOR',
      'GL_SRC_ALPHA',
      'GL_ONE_MINUS_SRC_ALPHA',
      'GL_DST_ALPHA',
      'GL_ONE_MINUS_DST_ALPHA',
      'GL_CONSTANT_COLOR',
      'GL_ONE_MINUS_CONSTANT_COLOR',
      'GL_CONSTANT_ALPHA',
      'GL_ONE_MINUS_CONSTANT_ALPHA',
      'GL_SRC_ALPHA_SATURATE',
    ],
  },
  'DstBlendFactor': {
    'type': 'GLenum',
    'valid': [
      'GL_ZERO',
      'GL_ONE',
      'GL_SRC_COLOR',
      'GL_ONE_MINUS_SRC_COLOR',
      'GL_DST_COLOR',
      'GL_ONE_MINUS_DST_COLOR',
      'GL_SRC_ALPHA',
      'GL_ONE_MINUS_SRC_ALPHA',
      'GL_DST_ALPHA',
      'GL_ONE_MINUS_DST_ALPHA',
      'GL_CONSTANT_COLOR',
      'GL_ONE_MINUS_CONSTANT_COLOR',
      'GL_CONSTANT_ALPHA',
      'GL_ONE_MINUS_CONSTANT_ALPHA',
    ],
  },
  'Capability': {
    'type': 'GLenum',
    'valid': [
      'GL_DITHER',  # 1st one is a non-cached value so autogen unit tests work.
      'GL_BLEND',
      'GL_CULL_FACE',
      'GL_DEPTH_TEST',
      'GL_POLYGON_OFFSET_FILL',
      'GL_SAMPLE_ALPHA_TO_COVERAGE',
      'GL_SAMPLE_COVERAGE',
      'GL_SCISSOR_TEST',
      'GL_STENCIL_TEST',
    ],
    'invalid': [
      'GL_CLIP_PLANE0',
      'GL_POINT_SPRITE',
    ],
  },
  'DrawMode': {
    'type': 'GLenum',
    'valid': [
      'GL_POINTS',
      'GL_LINE_STRIP',
      'GL_LINE_LOOP',
      'GL_LINES',
      'GL_TRIANGLE_STRIP',
      'GL_TRIANGLE_FAN',
      'GL_TRIANGLES',
    ],
    'invalid': [
      'GL_QUADS',
      'GL_POLYGON',
    ],
  },
  'IndexType': {
    'type': 'GLenum',
    'valid': [
      'GL_UNSIGNED_BYTE',
      'GL_UNSIGNED_SHORT',
    ],
    'invalid': [
      'GL_UNSIGNED_INT',
      'GL_INT',
    ],
  },
  'GetMaxIndexType': {
    'type': 'GLenum',
    'valid': [
      'GL_UNSIGNED_BYTE',
      'GL_UNSIGNED_SHORT',
      'GL_UNSIGNED_INT',
    ],
    'invalid': [
      'GL_INT',
    ],
  },
  'Attachment': {
    'type': 'GLenum',
    'valid': [
      'GL_COLOR_ATTACHMENT0',
      'GL_DEPTH_ATTACHMENT',
      'GL_STENCIL_ATTACHMENT',
    ],
  },
  'BufferParameter': {
    'type': 'GLenum',
    'valid': [
      'GL_BUFFER_SIZE',
      'GL_BUFFER_USAGE',
    ],
    'invalid': [
      'GL_PIXEL_PACK_BUFFER',
    ],
  },
  'FrameBufferParameter': {
    'type': 'GLenum',
    'valid': [
      'GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE',
      'GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME',
      'GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL',
      'GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE',
    ],
  },
  'ProgramParameter': {
    'type': 'GLenum',
    'valid': [
      'GL_DELETE_STATUS',
      'GL_LINK_STATUS',
      'GL_VALIDATE_STATUS',
      'GL_INFO_LOG_LENGTH',
      'GL_ATTACHED_SHADERS',
      'GL_ACTIVE_ATTRIBUTES',
      'GL_ACTIVE_ATTRIBUTE_MAX_LENGTH',
      'GL_ACTIVE_UNIFORMS',
      'GL_ACTIVE_UNIFORM_MAX_LENGTH',
    ],
  },
  'QueryObjectParameter': {
    'type': 'GLenum',
    'valid': [
      'GL_QUERY_RESULT_EXT',
      'GL_QUERY_RESULT_AVAILABLE_EXT',
    ],
  },
  'QueryParameter': {
    'type': 'GLenum',
    'valid': [
      'GL_CURRENT_QUERY_EXT',
    ],
  },
  'QueryTarget': {
    'type': 'GLenum',
    'valid': [
      'GL_ANY_SAMPLES_PASSED_EXT',
      'GL_ANY_SAMPLES_PASSED_CONSERVATIVE_EXT',
      'GL_COMMANDS_ISSUED_CHROMIUM',
    ],
  },
  'RenderBufferParameter': {
    'type': 'GLenum',
    'valid': [
      'GL_RENDERBUFFER_RED_SIZE',
      'GL_RENDERBUFFER_GREEN_SIZE',
      'GL_RENDERBUFFER_BLUE_SIZE',
      'GL_RENDERBUFFER_ALPHA_SIZE',
      'GL_RENDERBUFFER_DEPTH_SIZE',
      'GL_RENDERBUFFER_STENCIL_SIZE',
      'GL_RENDERBUFFER_WIDTH',
      'GL_RENDERBUFFER_HEIGHT',
      'GL_RENDERBUFFER_INTERNAL_FORMAT',
    ],
  },
  'ShaderParameter': {
    'type': 'GLenum',
    'valid': [
      'GL_SHADER_TYPE',
      'GL_DELETE_STATUS',
      'GL_COMPILE_STATUS',
      'GL_INFO_LOG_LENGTH',
      'GL_SHADER_SOURCE_LENGTH',
      'GL_TRANSLATED_SHADER_SOURCE_LENGTH_ANGLE',
    ],
  },
  'ShaderPrecision': {
    'type': 'GLenum',
    'valid': [
      'GL_LOW_FLOAT',
      'GL_MEDIUM_FLOAT',
      'GL_HIGH_FLOAT',
      'GL_LOW_INT',
      'GL_MEDIUM_INT',
      'GL_HIGH_INT',
    ],
  },
  'StringType': {
    'type': 'GLenum',
    'valid': [
      'GL_VENDOR',
      'GL_RENDERER',
      'GL_VERSION',
      'GL_SHADING_LANGUAGE_VERSION',
      'GL_EXTENSIONS',
    ],
  },
  'TextureParameter': {
    'type': 'GLenum',
    'valid': [
      'GL_TEXTURE_MAG_FILTER',
      'GL_TEXTURE_MIN_FILTER',
      'GL_TEXTURE_WRAP_S',
      'GL_TEXTURE_WRAP_T',
    ],
    'invalid': [
      'GL_GENERATE_MIPMAP',
    ],
  },
  'TextureWrapMode': {
    'type': 'GLenum',
    'valid': [
      'GL_CLAMP_TO_EDGE',
      'GL_MIRRORED_REPEAT',
      'GL_REPEAT',
    ],
  },
  'TextureMinFilterMode': {
    'type': 'GLenum',
    'valid': [
      'GL_NEAREST',
      'GL_LINEAR',
      'GL_NEAREST_MIPMAP_NEAREST',
      'GL_LINEAR_MIPMAP_NEAREST',
      'GL_NEAREST_MIPMAP_LINEAR',
      'GL_LINEAR_MIPMAP_LINEAR',
    ],
  },
  'TextureMagFilterMode': {
    'type': 'GLenum',
    'valid': [
      'GL_NEAREST',
      'GL_LINEAR',
    ],
  },
  'TextureUsage': {
    'type': 'GLenum',
    'valid': [
      'GL_NONE',
      'GL_FRAMEBUFFER_ATTACHMENT_ANGLE',
    ],
  },
  'VertexAttribute': {
    'type': 'GLenum',
    'valid': [
      # some enum that the decoder actually passes through to GL needs
      # to be the first listed here since it's used in unit tests.
      'GL_VERTEX_ATTRIB_ARRAY_NORMALIZED',
      'GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING',
      'GL_VERTEX_ATTRIB_ARRAY_ENABLED',
      'GL_VERTEX_ATTRIB_ARRAY_SIZE',
      'GL_VERTEX_ATTRIB_ARRAY_STRIDE',
      'GL_VERTEX_ATTRIB_ARRAY_TYPE',
      'GL_CURRENT_VERTEX_ATTRIB',
    ],
  },
  'VertexPointer': {
    'type': 'GLenum',
    'valid': [
      'GL_VERTEX_ATTRIB_ARRAY_POINTER',
    ],
  },
  'HintTarget': {
    'type': 'GLenum',
    'valid': [
      'GL_GENERATE_MIPMAP_HINT',
    ],
    'invalid': [
      'GL_PERSPECTIVE_CORRECTION_HINT',
    ],
  },
  'HintMode': {
    'type': 'GLenum',
    'valid': [
      'GL_FASTEST',
      'GL_NICEST',
      'GL_DONT_CARE',
    ],
  },
  'PixelStore': {
    'type': 'GLenum',
    'valid': [
      'GL_PACK_ALIGNMENT',
      'GL_UNPACK_ALIGNMENT',
      'GL_UNPACK_FLIP_Y_CHROMIUM',
      'GL_UNPACK_PREMULTIPLY_ALPHA_CHROMIUM',
    ],
    'invalid': [
      'GL_PACK_SWAP_BYTES',
      'GL_UNPACK_SWAP_BYTES',
    ],
  },
  'PixelStoreAlignment': {
    'type': 'GLint',
    'valid': [
      '1',
      '2',
      '4',
      '8',
    ],
    'invalid': [
      '3',
      '9',
    ],
  },
  'ReadPixelFormat': {
    'type': 'GLenum',
    'valid': [
      'GL_ALPHA',
      'GL_RGB',
      'GL_RGBA',
    ],
  },
  'PixelType': {
    'type': 'GLenum',
    'valid': [
      'GL_UNSIGNED_BYTE',
      'GL_UNSIGNED_SHORT_5_6_5',
      'GL_UNSIGNED_SHORT_4_4_4_4',
      'GL_UNSIGNED_SHORT_5_5_5_1',
    ],
    'invalid': [
      'GL_SHORT',
      'GL_INT',
    ],
  },
  'RenderBufferFormat': {
    'type': 'GLenum',
    'valid': [
      'GL_RGBA4',
      'GL_RGB565',
      'GL_RGB5_A1',
      'GL_DEPTH_COMPONENT16',
      'GL_STENCIL_INDEX8',
    ],
  },
  'ShaderBinaryFormat': {
    'type': 'GLenum',
    'valid': [
    ],
  },
  'StencilOp': {
    'type': 'GLenum',
    'valid': [
      'GL_KEEP',
      'GL_ZERO',
      'GL_REPLACE',
      'GL_INCR',
      'GL_INCR_WRAP',
      'GL_DECR',
      'GL_DECR_WRAP',
      'GL_INVERT',
    ],
  },
  'TextureFormat': {
    'type': 'GLenum',
    'valid': [
      'GL_ALPHA',
      'GL_LUMINANCE',
      'GL_LUMINANCE_ALPHA',
      'GL_RGB',
      'GL_RGBA',
    ],
    'invalid': [
      'GL_BGRA',
      'GL_BGR',
    ],
  },
  'TextureInternalFormat': {
    'type': 'GLenum',
    'valid': [
      'GL_ALPHA',
      'GL_LUMINANCE',
      'GL_LUMINANCE_ALPHA',
      'GL_RGB',
      'GL_RGBA',
    ],
    'invalid': [
      'GL_BGRA',
      'GL_BGR',
    ],
  },
  'TextureInternalFormatStorage': {
    'type': 'GLenum',
    'valid': [
      'GL_RGB565',
      'GL_RGBA4',
      'GL_RGB5_A1',
      'GL_ALPHA8_EXT',
      'GL_LUMINANCE8_EXT',
      'GL_LUMINANCE8_ALPHA8_EXT',
      'GL_RGB8_OES',
      'GL_RGBA8_OES',
    ],
  },
  'VertexAttribType': {
    'type': 'GLenum',
    'valid': [
      'GL_BYTE',
      'GL_UNSIGNED_BYTE',
      'GL_SHORT',
      'GL_UNSIGNED_SHORT',
    #  'GL_FIXED',  // This is not available on Desktop GL.
      'GL_FLOAT',
    ],
    'invalid': [
      'GL_DOUBLE',
    ],
  },
  'TextureBorder': {
    'type': 'GLint',
    'valid': [
      '0',
    ],
    'invalid': [
      '1',
    ],
  },
  'VertexAttribSize': {
    'type': 'GLint',
    'valid': [
      '1',
      '2',
      '3',
      '4',
    ],
    'invalid': [
      '0',
      '5',
    ],
  },
  'ZeroOnly': {
    'type': 'GLint',
    'valid': [
      '0',
    ],
    'invalid': [
      '1',
    ],
  },
  'FalseOnly': {
    'type': 'GLboolean',
    'valid': [
      'false',
    ],
    'invalid': [
      'true',
    ],
  },
}

# This table specifies the different pepper interfaces that are supported for
# GL commands. 'dev' is true if it's a dev interface.
_PEPPER_INTERFACES = [
  {'name': '', 'dev': False},
  {'name': 'InstancedArrays', 'dev': False},
  {'name': 'FramebufferBlit', 'dev': False},
  {'name': 'FramebufferMultisample', 'dev': False},
  {'name': 'ChromiumEnableFeature', 'dev': False},
  {'name': 'ChromiumMapSub', 'dev': False},
  {'name': 'Query', 'dev': False},
]

# This table specifies types and other special data for the commands that
# will be generated.
#
# cmd_comment:  A comment added to the cmd format.
# type:         defines which handler will be used to generate code.
# decoder_func: defines which function to call in the decoder to execute the
#               corresponding GL command. If not specified the GL command will
#               be called directly.
# gl_test_func: GL function that is expected to be called when testing.
# cmd_args:     The arguments to use for the command. This overrides generating
#               them based on the GL function arguments.
#               a NonImmediate type is a type that stays a pointer even in
#               and immediate version of acommand.
# gen_cmd:      Whether or not this function geneates a command. Default = True.
# immediate:    Whether or not to generate an immediate command for the GL
#               function. The default is if there is exactly 1 pointer argument
#               in the GL function an immediate command is generated.
# bucket:       True to generate a bucket version of the command.
# impl_func:    Whether or not to generate the GLES2Implementation part of this
#               command.
# impl_decl:    Whether or not to generate the GLES2Implementation declaration
#               for this command.
# needs_size:   If true a data_size field is added to the command.
# data_type:    The type of data the command uses. For PUTn or PUT types.
# count:        The number of units per element. For PUTn or PUT types.
# unit_test:    If False no service side unit test will be generated.
# client_test:  If False no client side unit test will be generated.
# expectation:  If False the unit test will have no expected calls.
# gen_func:     Name of function that generates GL resource for corresponding
#               bind function.
# valid_args:   A dictionary of argument indices to args to use in unit tests
#               when they can not be automatically determined.
# pepper_interface: The pepper interface that is used for this extension
# invalid_test: False if no invalid test needed.

_FUNCTION_INFO = {
  'ActiveTexture': {
    'decoder_func': 'DoActiveTexture',
    'unit_test': False,
    'impl_func': False,
    'client_test': False,
  },
  'AttachShader': {'decoder_func': 'DoAttachShader'},
  'BindAttribLocation': {'type': 'GLchar', 'bucket': True, 'needs_size': True},
  'BindBuffer': {
    'type': 'Bind',
    'decoder_func': 'DoBindBuffer',
    'gen_func': 'GenBuffersARB',
  },
  'BindFramebuffer': {
    'type': 'Bind',
    'decoder_func': 'DoBindFramebuffer',
    'gl_test_func': 'glBindFramebufferEXT',
    'gen_func': 'GenFramebuffersEXT',
  },
  'BindRenderbuffer': {
    'type': 'Bind',
    'decoder_func': 'DoBindRenderbuffer',
    'gl_test_func': 'glBindRenderbufferEXT',
    'gen_func': 'GenRenderbuffersEXT',
  },
  'BindTexture': {
    'type': 'Bind',
    'decoder_func': 'DoBindTexture',
    'gen_func': 'GenTextures',
  },
  'BlitFramebufferEXT': {
    'decoder_func': 'DoBlitFramebufferEXT',
    'unit_test': False,
    'extension': True,
    'pepper_interface': 'FramebufferBlit',
  },
  'BufferData': {
    'type': 'Manual',
    'immediate': True,
    'client_test': False,
  },
  'BufferSubData': {
    'type': 'Data',
    'client_test': False,
    'decoder_func': 'DoBufferSubData',
  },
  'CheckFramebufferStatus': {
    'type': 'Is',
    'decoder_func': 'DoCheckFramebufferStatus',
    'gl_test_func': 'glCheckFramebufferStatusEXT',
    'error_value': 'GL_FRAMEBUFFER_UNSUPPORTED',
    'result': ['GLenum'],
  },
  'Clear': {'decoder_func': 'DoClear'},
  'ClearColor': {'decoder_func': 'DoClearColor'},
  'ClearDepthf': {
    'decoder_func': 'DoClearDepthf',
    'gl_test_func': 'glClearDepth',
  },
  'ColorMask': {'decoder_func': 'DoColorMask', 'expectation': False},
  'ConsumeTextureCHROMIUM': {
    'decoder_func': 'DoConsumeTextureCHROMIUM',
    'type': 'PUT',
    'data_type': 'GLbyte',
    'count': 64,
    'unit_test': False,
    'extension': True,
    'chromium': True,
  },
  'ClearStencil': {'decoder_func': 'DoClearStencil'},
  'EnableFeatureCHROMIUM': {
    'type': 'Custom',
    'immediate': False,
    'decoder_func': 'DoEnableFeatureCHROMIUM',
    'expectation': False,
    'cmd_args': 'GLuint bucket_id, GLint* result',
    'result': ['GLint'],
    'extension': True,
    'chromium': True,
    'pepper_interface': 'ChromiumEnableFeature',
  },
  'CompileShader': {'decoder_func': 'DoCompileShader', 'unit_test': False},
  'CompressedTexImage2D': {
    'type': 'Manual',
    'immediate': True,
    'bucket': True,
  },
  'CompressedTexSubImage2D': {
    'type': 'Data',
    'bucket': True,
    'decoder_func': 'DoCompressedTexSubImage2D',
  },
  'CopyTexImage2D': {
    'decoder_func': 'DoCopyTexImage2D',
    'unit_test': False,
  },
  'CopyTexSubImage2D': {
    'decoder_func': 'DoCopyTexSubImage2D',
  },
  'CreateProgram': {
    'type': 'Create',
    'client_test': False,
  },
  'CreateShader': {
    'type': 'Create',
    'client_test': False,
  },
  'DeleteBuffers': {
    'type': 'DELn',
    'gl_test_func': 'glDeleteBuffersARB',
    'resource_type': 'Buffer',
    'resource_types': 'Buffers',
  },
  'DeleteFramebuffers': {
    'type': 'DELn',
    'gl_test_func': 'glDeleteFramebuffersEXT',
    'resource_type': 'Framebuffer',
    'resource_types': 'Framebuffers',
  },
  'DeleteProgram': {'type': 'Delete', 'decoder_func': 'DoDeleteProgram'},
  'DeleteRenderbuffers': {
    'type': 'DELn',
    'gl_test_func': 'glDeleteRenderbuffersEXT',
    'resource_type': 'Renderbuffer',
    'resource_types': 'Renderbuffers',
  },
  'DeleteShader': {'type': 'Delete', 'decoder_func': 'DoDeleteShader'},
  'DeleteSharedIdsCHROMIUM': {
    'type': 'Custom',
    'decoder_func': 'DoDeleteSharedIdsCHROMIUM',
    'impl_func': False,
    'expectation': False,
    'immediate': False,
    'extension': True,
    'chromium': True,
  },
  'DeleteTextures': {
    'type': 'DELn',
    'resource_type': 'Texture',
    'resource_types': 'Textures',
  },
  'DepthRangef': {'decoder_func': 'glDepthRange'},
  'DepthMask': {'decoder_func': 'DoDepthMask', 'expectation': False},
  'DetachShader': {'decoder_func': 'DoDetachShader'},
  'Disable': {
    'decoder_func': 'DoDisable',
    'impl_func': False,
  },
  'DisableVertexAttribArray': {
    'decoder_func': 'DoDisableVertexAttribArray',
    'impl_decl': False,
  },
  'DrawArrays': {
    'type': 'Manual',
    'cmd_args': 'GLenumDrawMode mode, GLint first, GLsizei count',
  },
  'DrawElements': {
    'type': 'Manual',
    'cmd_args': 'GLenumDrawMode mode, GLsizei count, '
                'GLenumIndexType type, GLuint index_offset',
    'client_test': False
  },
  'Enable': {
    'decoder_func': 'DoEnable',
    'impl_func': False,
  },
  'EnableVertexAttribArray': {
    'decoder_func': 'DoEnableVertexAttribArray',
    'impl_decl': False,
  },
  'Finish': {
    'impl_func': False,
    'client_test': False,
    'decoder_func': 'DoFinish',
  },
  'Flush': {
    'impl_func': False,
    'decoder_func': 'DoFlush',
  },
  'FramebufferRenderbuffer': {
    'decoder_func': 'DoFramebufferRenderbuffer',
    'gl_test_func': 'glFramebufferRenderbufferEXT',
  },
  'FramebufferTexture2D': {
    'decoder_func': 'DoFramebufferTexture2D',
    'gl_test_func': 'glFramebufferTexture2DEXT',
  },
  'GenerateMipmap': {
    'decoder_func': 'DoGenerateMipmap',
    'gl_test_func': 'glGenerateMipmapEXT',
  },
  'GenBuffers': {
    'type': 'GENn',
    'gl_test_func': 'glGenBuffersARB',
    'resource_type': 'Buffer',
    'resource_types': 'Buffers',
  },
  'GenMailboxCHROMIUM': {
    'type': 'Manual',
    'cmd_args': 'GLuint bucket_id',
    'result': ['SizedResult<GLint>'],
    'client_test': False,
    'unit_test': False,
    'extension': True,
    'chromium': True,
  },
  'GenFramebuffers': {
    'type': 'GENn',
    'gl_test_func': 'glGenFramebuffersEXT',
    'resource_type': 'Framebuffer',
    'resource_types': 'Framebuffers',
  },
  'GenRenderbuffers': {
    'type': 'GENn', 'gl_test_func': 'glGenRenderbuffersEXT',
    'resource_type': 'Renderbuffer',
    'resource_types': 'Renderbuffers',
  },
  'GenTextures': {
    'type': 'GENn',
    'gl_test_func': 'glGenTextures',
    'resource_type': 'Texture',
    'resource_types': 'Textures',
  },
  'GenSharedIdsCHROMIUM': {
    'type': 'Custom',
    'decoder_func': 'DoGenSharedIdsCHROMIUM',
    'impl_func': False,
    'expectation': False,
    'immediate': False,
    'extension': True,
    'chromium': True,
  },
  'GetActiveAttrib': {
    'type': 'Custom',
    'immediate': False,
    'cmd_args':
        'GLidProgram program, GLuint index, uint32 name_bucket_id, '
        'void* result',
    'result': [
      'int32 success',
      'int32 size',
      'uint32 type',
    ],
  },
  'GetActiveUniform': {
    'type': 'Custom',
    'immediate': False,
    'cmd_args':
        'GLidProgram program, GLuint index, uint32 name_bucket_id, '
        'void* result',
    'result': [
      'int32 success',
      'int32 size',
      'uint32 type',
    ],
  },
  'GetAttachedShaders': {
    'type': 'Custom',
    'immediate': False,
    'cmd_args': 'GLidProgram program, void* result, uint32 result_size',
    'result': ['SizedResult<GLuint>'],
  },
  'GetAttribLocation': {
    'type': 'HandWritten',
    'immediate': True,
    'bucket': True,
    'needs_size': True,
    'cmd_args':
        'GLidProgram program, const char* name, NonImmediate GLint* location',
    'result': ['GLint'],
  },
  'GetBooleanv': {
    'type': 'GETn',
    'result': ['SizedResult<GLboolean>'],
    'decoder_func': 'DoGetBooleanv',
    'gl_test_func': 'glGetBooleanv',
  },
  'GetBufferParameteriv': {'type': 'GETn', 'result': ['SizedResult<GLint>']},
  'GetError': {
    'type': 'Is',
    'decoder_func': 'GetGLError',
    'impl_func': False,
    'result': ['GLenum'],
    'client_test': False,
  },
  'GetFloatv': {
    'type': 'GETn',
    'result': ['SizedResult<GLfloat>'],
    'decoder_func': 'DoGetFloatv',
    'gl_test_func': 'glGetFloatv',
  },
  'GetFramebufferAttachmentParameteriv': {
    'type': 'GETn',
    'decoder_func': 'DoGetFramebufferAttachmentParameteriv',
    'gl_test_func': 'glGetFramebufferAttachmentParameterivEXT',
    'result': ['SizedResult<GLint>'],
  },
  'GetIntegerv': {
    'type': 'GETn',
    'result': ['SizedResult<GLint>'],
    'decoder_func': 'DoGetIntegerv',
    'client_test': False,
  },
  'GetMaxValueInBufferCHROMIUM': {
    'type': 'Is',
    'decoder_func': 'DoGetMaxValueInBufferCHROMIUM',
    'result': ['GLuint'],
    'unit_test': False,
    'client_test': False,
    'extension': True,
    'chromium': True,
    'impl_func': False,
  },
  'GetMultipleIntegervCHROMIUM': {
    'type': 'Custom',
    'immediate': False,
    'expectation': False,
    'extension': True,
    'chromium': True,
    'client_test': False,
  },
  'GetProgramiv': {
    'type': 'GETn',
    'decoder_func': 'DoGetProgramiv',
    'result': ['SizedResult<GLint>'],
    'expectation': False,
  },
  'GetProgramInfoCHROMIUM': {
    'type': 'Custom',
    'immediate': False,
    'expectation': False,
    'impl_func': False,
    'extension': True,
    'chromium': True,
    'client_test': False,
    'cmd_args': 'GLidProgram program, uint32 bucket_id',
    'result': [
      'uint32 link_status',
      'uint32 num_attribs',
      'uint32 num_uniforms',
    ],
  },
  'GetProgramInfoLog': {
    'type': 'STRn',
    'expectation': False,
  },
  'GetRenderbufferParameteriv': {
    'type': 'GETn',
    'decoder_func': 'DoGetRenderbufferParameteriv',
    'gl_test_func': 'glGetRenderbufferParameterivEXT',
    'result': ['SizedResult<GLint>'],
  },
  'GetShaderiv': {
    'type': 'GETn',
    'decoder_func': 'DoGetShaderiv',
    'result': ['SizedResult<GLint>'],
  },
  'GetShaderInfoLog': {
    'type': 'STRn',
    'get_len_func': 'glGetShaderiv',
    'get_len_enum': 'GL_INFO_LOG_LENGTH',
    'unit_test': False,
  },
  'GetShaderPrecisionFormat': {
    'type': 'Custom',
    'immediate': False,
    'cmd_args':
      'GLenumShaderType shadertype, GLenumShaderPrecision precisiontype, '
      'void* result',
    'result': [
      'int32 success',
      'int32 min_range',
      'int32 max_range',
      'int32 precision',
    ],
  },
  'GetShaderSource': {
    'type': 'STRn',
    'get_len_func': 'DoGetShaderiv',
    'get_len_enum': 'GL_SHADER_SOURCE_LENGTH',
    'unit_test': False,
    'client_test': False,
    },
  'GetString': {
      'type': 'Custom',
      'client_test': False,
      'cmd_args': 'GLenumStringType name, uint32 bucket_id',
  },
  'GetTexParameterfv': {'type': 'GETn', 'result': ['SizedResult<GLfloat>']},
  'GetTexParameteriv': {'type': 'GETn', 'result': ['SizedResult<GLint>']},
  'GetTranslatedShaderSourceANGLE': {
    'type': 'STRn',
    'get_len_func': 'DoGetShaderiv',
    'get_len_enum': 'GL_TRANSLATED_SHADER_SOURCE_LENGTH_ANGLE',
    'unit_test': False,
    'extension': True,
    },
  'GetUniformfv': {
    'type': 'Custom',
    'immediate': False,
    'result': ['SizedResult<GLfloat>'],
  },
  'GetUniformiv': {
    'type': 'Custom',
    'immediate': False,
    'result': ['SizedResult<GLint>'],
  },
  'GetUniformLocation': {
    'type': 'HandWritten',
    'immediate': True,
    'bucket': True,
    'needs_size': True,
    'cmd_args':
        'GLidProgram program, const char* name, NonImmediate GLint* location',
    'result': ['GLint'],
  },
  'GetVertexAttribfv': {
    'type': 'GETn',
    'result': ['SizedResult<GLfloat>'],
    'impl_decl': False,
    'decoder_func': 'DoGetVertexAttribfv',
    'expectation': False,
    'client_test': False,
  },
  'GetVertexAttribiv': {
    'type': 'GETn',
    'result': ['SizedResult<GLint>'],
    'impl_decl': False,
    'decoder_func': 'DoGetVertexAttribiv',
    'expectation': False,
    'client_test': False,
  },
  'GetVertexAttribPointerv': {
    'type': 'Custom',
    'immediate': False,
    'result': ['SizedResult<GLuint>'],
    'client_test': False,
  },
  'IsBuffer': {
    'type': 'Is',
    'decoder_func': 'DoIsBuffer',
    'expectation': False,
  },
  'IsEnabled': {
    'type': 'Is',
    'decoder_func': 'DoIsEnabled',
    'impl_func': False,
  },
  'IsFramebuffer': {
    'type': 'Is',
    'decoder_func': 'DoIsFramebuffer',
    'expectation': False,
  },
  'IsProgram': {
    'type': 'Is',
    'decoder_func': 'DoIsProgram',
    'expectation': False,
  },
  'IsRenderbuffer': {
    'type': 'Is',
    'decoder_func': 'DoIsRenderbuffer',
    'expectation': False,
  },
  'IsShader': {
    'type': 'Is',
    'decoder_func': 'DoIsShader',
    'expectation': False,
  },
  'IsTexture': {
    'type': 'Is',
    'decoder_func': 'DoIsTexture',
    'expectation': False,
  },
  'LinkProgram': {
    'decoder_func': 'DoLinkProgram',
    'impl_func':  False,
  },
  'MapBufferSubDataCHROMIUM': {
    'gen_cmd': False,
    'extension': True,
    'chromium': True,
    'client_test': False,
    'pepper_interface': 'ChromiumMapSub',
  },
  'MapTexSubImage2DCHROMIUM': {
    'gen_cmd': False,
    'extension': True,
    'chromium': True,
    'client_test': False,
    'pepper_interface': 'ChromiumMapSub',
  },
  'PixelStorei': {'type': 'Manual'},
  'PostSubBufferCHROMIUM': {
      'type': 'Custom',
      'impl_func': False,
      'unit_test': False,
      'client_test': False,
      'extension': True,
      'chromium': True,
  },
  'ProduceTextureCHROMIUM': {
    'decoder_func': 'DoProduceTextureCHROMIUM',
    'type': 'PUT',
    'data_type': 'GLbyte',
    'count': 64,
    'unit_test': False,
    'extension': True,
    'chromium': True,
  },
  'RenderbufferStorage': {
    'decoder_func': 'DoRenderbufferStorage',
    'gl_test_func': 'glRenderbufferStorageEXT',
    'expectation': False,
  },
  'RenderbufferStorageMultisampleEXT': {
    'decoder_func': 'DoRenderbufferStorageMultisample',
    'gl_test_func': 'glRenderbufferStorageMultisampleEXT',
    'expectation': False,
    'unit_test': False,
    'extension': True,
    'pepper_interface': 'FramebufferMultisample',
  },
  'ReadPixels': {
    'cmd_comment':
        '// ReadPixels has the result separated from the pixel buffer so that\n'
        '// it is easier to specify the result going to some specific place\n'
        '// that exactly fits the rectangle of pixels.\n',
    'type': 'Custom',
    'immediate': False,
    'impl_func': False,
    'client_test': False,
    'cmd_args':
        'GLint x, GLint y, GLsizei width, GLsizei height, '
        'GLenumReadPixelFormat format, GLenumPixelType type, '
        'uint32 pixels_shm_id, uint32 pixels_shm_offset, '
        'uint32 result_shm_id, uint32 result_shm_offset',
    'result': ['uint32'],
  },
  'RegisterSharedIdsCHROMIUM': {
    'type': 'Custom',
    'decoder_func': 'DoRegisterSharedIdsCHROMIUM',
    'impl_func': False,
    'expectation': False,
    'immediate': False,
    'extension': True,
    'chromium': True,
  },
  'ReleaseShaderCompiler': {
    'decoder_func': 'DoReleaseShaderCompiler',
    'unit_test': False,
  },
  'ShaderBinary': {
    'type': 'Custom',
    'client_test': False,
  },
  'ShaderSource': {
    'type': 'Manual',
    'immediate': True,
    'bucket': True,
    'needs_size': True,
    'client_test': False,
    'cmd_args':
        'GLuint shader, const char* data',
  },
  'StencilMask': {'decoder_func': 'DoStencilMask', 'expectation': False},
  'StencilMaskSeparate': {
    'decoder_func': 'DoStencilMaskSeparate',
    'expectation': False,
  },
  'SwapBuffers': {
    'type': 'Custom',
    'impl_func': False,
    'unit_test': False,
    'client_test': False,
    'extension': True,
  },
  'TexImage2D': {
    'type': 'Manual',
    'immediate': True,
    'client_test': False,
  },
  'TexParameterf': {
    'decoder_func': 'DoTexParameterf',
    'valid_args': {
      '2': 'GL_NEAREST'
    },
  },
  'TexParameteri': {
    'decoder_func': 'DoTexParameteri',
    'valid_args': {
      '2': 'GL_NEAREST'
    },
  },
  'TexParameterfv': {
    'type': 'PUT',
    'data_type': 'GLfloat',
    'data_value': 'GL_NEAREST',
    'count': 1,
    'decoder_func': 'DoTexParameterfv',
  },
  'TexParameteriv': {
    'type': 'PUT',
    'data_type': 'GLint',
    'data_value': 'GL_NEAREST',
    'count': 1,
    'decoder_func': 'DoTexParameteriv',
  },
  'TexSubImage2D': {
    'type': 'Manual',
    'immediate': True,
    'client_test': False,
    'cmd_args': 'GLenumTextureTarget target, GLint level, '
                'GLint xoffset, GLint yoffset, '
                'GLsizei width, GLsizei height, '
                'GLenumTextureFormat format, GLenumPixelType type, '
                'const void* pixels, GLboolean internal'
  },
  'Uniform1f': {'type': 'PUTXn', 'data_type': 'GLfloat', 'count': 1},
  'Uniform1fv': {
    'type': 'PUTn',
    'data_type': 'GLfloat',
    'count': 1,
    'decoder_func': 'DoUniform1fv',
  },
  'Uniform1i': {'decoder_func': 'DoUniform1i', 'unit_test': False},
  'Uniform1iv': {
    'type': 'PUTn',
    'data_type': 'GLint',
    'count': 1,
    'decoder_func': 'DoUniform1iv',
    'unit_test': False,
  },
  'Uniform2i': {'type': 'PUTXn', 'data_type': 'GLint', 'count': 2},
  'Uniform2f': {'type': 'PUTXn', 'data_type': 'GLfloat', 'count': 2},
  'Uniform2fv': {
    'type': 'PUTn',
    'data_type': 'GLfloat',
    'count': 2,
    'decoder_func': 'DoUniform2fv',
  },
  'Uniform2iv': {
    'type': 'PUTn',
    'data_type': 'GLint',
    'count': 2,
    'decoder_func': 'DoUniform2iv',
  },
  'Uniform3i': {'type': 'PUTXn', 'data_type': 'GLint', 'count': 3},
  'Uniform3f': {'type': 'PUTXn', 'data_type': 'GLfloat', 'count': 3},
  'Uniform3fv': {
    'type': 'PUTn',
    'data_type': 'GLfloat',
    'count': 3,
    'decoder_func': 'DoUniform3fv',
  },
  'Uniform3iv': {
    'type': 'PUTn',
    'data_type': 'GLint',
    'count': 3,
    'decoder_func': 'DoUniform3iv',
  },
  'Uniform4i': {'type': 'PUTXn', 'data_type': 'GLint', 'count': 4},
  'Uniform4f': {'type': 'PUTXn', 'data_type': 'GLfloat', 'count': 4},
  'Uniform4fv': {
    'type': 'PUTn',
    'data_type': 'GLfloat',
    'count': 4,
    'decoder_func': 'DoUniform4fv',
  },
  'Uniform4iv': {
    'type': 'PUTn',
    'data_type': 'GLint',
    'count': 4,
    'decoder_func': 'DoUniform4iv',
  },
  'UniformMatrix2fv': {
    'type': 'PUTn',
    'data_type': 'GLfloat',
    'count': 4,
    'decoder_func': 'DoUniformMatrix2fv',
  },
  'UniformMatrix3fv': {
    'type': 'PUTn',
    'data_type': 'GLfloat',
    'count': 9,
    'decoder_func': 'DoUniformMatrix3fv',
  },
  'UniformMatrix4fv': {
    'type': 'PUTn',
    'data_type': 'GLfloat',
    'count': 16,
    'decoder_func': 'DoUniformMatrix4fv',
  },
  'UnmapBufferSubDataCHROMIUM': {
    'gen_cmd': False,
    'extension': True,
    'chromium': True,
    'client_test': False,
    'pepper_interface': 'ChromiumMapSub',
  },
  'UnmapTexSubImage2DCHROMIUM': {
    'gen_cmd': False,
    'extension': True,
    'chromium': True,
    'client_test': False,
    'pepper_interface': 'ChromiumMapSub',
  },
  'UseProgram': {'decoder_func': 'DoUseProgram', 'unit_test': False},
  'ValidateProgram': {'decoder_func': 'DoValidateProgram'},
  'VertexAttrib1f': {'decoder_func': 'DoVertexAttrib1f'},
  'VertexAttrib1fv': {
    'type': 'PUT',
    'data_type': 'GLfloat',
    'count': 1,
    'decoder_func': 'DoVertexAttrib1fv',
  },
  'VertexAttrib2f': {'decoder_func': 'DoVertexAttrib2f'},
  'VertexAttrib2fv': {
    'type': 'PUT',
    'data_type': 'GLfloat',
    'count': 2,
    'decoder_func': 'DoVertexAttrib2fv',
  },
  'VertexAttrib3f': {'decoder_func': 'DoVertexAttrib3f'},
  'VertexAttrib3fv': {
    'type': 'PUT',
    'data_type': 'GLfloat',
    'count': 3,
    'decoder_func': 'DoVertexAttrib3fv',
  },
  'VertexAttrib4f': {'decoder_func': 'DoVertexAttrib4f'},
  'VertexAttrib4fv': {
    'type': 'PUT',
    'data_type': 'GLfloat',
    'count': 4,
    'decoder_func': 'DoVertexAttrib4fv',
  },
  'VertexAttribPointer': {
      'type': 'Manual',
      'cmd_args': 'GLuint indx, GLintVertexAttribSize size, '
                  'GLenumVertexAttribType type, GLboolean normalized, '
                  'GLsizei stride, GLuint offset',
      'client_test': False,
  },
  'Viewport': {
    'decoder_func': 'DoViewport',
  },
  'ResizeCHROMIUM': {
      'type': 'Custom',
      'impl_func': False,
      'unit_test': False,
      'extension': True,
      'chromium': True,
  },
  'GetRequestableExtensionsCHROMIUM': {
    'type': 'Custom',
    'impl_func': False,
    'immediate': False,
    'cmd_args': 'uint32 bucket_id',
    'extension': True,
    'chromium': True,
  },
  'RequestExtensionCHROMIUM': {
    'type': 'Custom',
    'impl_func': False,
    'immediate': False,
    'client_test': False,
    'cmd_args': 'uint32 bucket_id',
    'extension': True,
    'chromium': True,
  },
  'RateLimitOffscreenContextCHROMIUM': {
    'gen_cmd': False,
    'extension': True,
    'chromium': True,
    'client_test': False,
  },
  'CreateStreamTextureCHROMIUM':  {
    'type': 'Custom',
    'cmd_args': 'GLuint client_id, void* result',
    'result': ['GLuint'],
    'immediate': False,
    'impl_func': False,
    'expectation': False,
    'extension': True,
    'chromium': True,
    'client_test': False,
   },
  'DestroyStreamTextureCHROMIUM':  {
    'type': 'Custom',
    'impl_func': False,
    'expectation': False,
    'extension': True,
    'chromium': True,
   },
  'TexImageIOSurface2DCHROMIUM': {
    'decoder_func': 'DoTexImageIOSurface2DCHROMIUM',
    'unit_test': False,
    'extension': True,
    'chromium': True,
  },
  'CopyTextureCHROMIUM': {
    'decoder_func': 'DoCopyTextureCHROMIUM',
    'unit_test': False,
    'extension': True,
    'chromium': True,
  },
  'TexStorage2DEXT': {
    'unit_test': False,
    'extension': True,
    'decoder_func': 'DoTexStorage2DEXT',
  },
  'DrawArraysInstancedANGLE': {
    'type': 'Manual',
    'cmd_args': 'GLenumDrawMode mode, GLint first, GLsizei count, '
                'GLsizei primcount',
    'extension': True,
    'unit_test': False,
    'pepper_interface': 'InstancedArrays',
  },
  'DrawElementsInstancedANGLE': {
    'type': 'Manual',
    'cmd_args': 'GLenumDrawMode mode, GLsizei count, '
                'GLenumIndexType type, GLuint index_offset, GLsizei primcount',
    'extension': True,
    'unit_test': False,
    'client_test': False,
    'pepper_interface': 'InstancedArrays',
  },
  'VertexAttribDivisorANGLE': {
    'type': 'Manual',
    'cmd_args': 'GLuint index, GLuint divisor',
    'extension': True,
    'unit_test': False,
    'pepper_interface': 'InstancedArrays',
  },
  'GenQueriesEXT': {
    'type': 'GENn',
    'gl_test_func': 'glGenQueriesARB',
    'resource_type': 'Query',
    'resource_types': 'Queries',
    'unit_test': False,
    'pepper_interface': 'Query',
  },
  'DeleteQueriesEXT': {
    'type': 'DELn',
    'gl_test_func': 'glDeleteQueriesARB',
    'resource_type': 'Query',
    'resource_types': 'Queries',
    'unit_test': False,
    'pepper_interface': 'Query',
  },
  'IsQueryEXT': {
    'gen_cmd': False,
    'client_test': False,
    'pepper_interface': 'Query',
  },
  'BeginQueryEXT': {
    'type': 'Manual',
    'cmd_args': 'GLenumQueryTarget target, GLidQuery id, void* sync_data',
    'immediate': False,
    'gl_test_func': 'glBeginQuery',
    'pepper_interface': 'Query',
  },
  'EndQueryEXT': {
    'type': 'Manual',
    'cmd_args': 'GLenumQueryTarget target, GLuint submit_count',
    'gl_test_func': 'glEndnQuery',
    'client_test': False,
    'pepper_interface': 'Query',
  },
  'GetQueryivEXT': {
    'gen_cmd': False,
    'client_test': False,
    'gl_test_func': 'glGetQueryiv',
    'pepper_interface': 'Query',
  },
  'GetQueryObjectuivEXT': {
    'gen_cmd': False,
    'client_test': False,
    'gl_test_func': 'glGetQueryObjectuiv',
    'pepper_interface': 'Query',
  },
}


def SplitWords(input_string):
  """Transforms a input_string into a list of lower-case components.

  Args:
    input_string: the input string.

  Returns:
    a list of lower-case words.
  """
  if input_string.find('_') > -1:
    # 'some_TEXT_' -> 'some text'
    return input_string.replace('_', ' ').strip().lower().split()
  else:
    if re.search('[A-Z]', input_string) and re.search('[a-z]', input_string):
      # mixed case.
      # look for capitalization to cut input_strings
      # 'SomeText' -> 'Some Text'
      input_string = re.sub('([A-Z])', r' \1', input_string).strip()
      # 'Vector3' -> 'Vector 3'
      input_string = re.sub('([^0-9])([0-9])', r'\1 \2', input_string)
    return input_string.lower().split()


def Lower(words):
  """Makes a lower-case identifier from words.

  Args:
    words: a list of lower-case words.

  Returns:
    the lower-case identifier.
  """
  return '_'.join(words)


def ToUnderscore(input_string):
  """converts CamelCase to camel_case."""
  words = SplitWords(input_string)
  return Lower(words)


class CWriter(object):
  """Writes to a file formatting it for Google's style guidelines."""

  def __init__(self, filename):
    self.filename = filename
    self.file_num = 0
    self.content = []

  def SetFileNum(self, num):
    """Used to help write number files and tests."""
    self.file_num = num

  def Write(self, string):
    """Writes a string to a file spliting if it's > 80 characters."""
    lines = string.splitlines()
    num_lines = len(lines)
    for ii in range(0, num_lines):
      self.__WriteLine(lines[ii], ii < (num_lines - 1) or string[-1] == '\n')

  def __FindSplit(self, string):
    """Finds a place to split a string."""
    splitter = string.find('=')
    if splitter >= 1 and not string[splitter + 1] == '=' and splitter < 80:
      return splitter
    # parts = string.split('(')
    parts = re.split("(?<=[^\"])\((?!\")", string)
    fptr = re.compile('\*\w*\)')
    if len(parts) > 1:
      splitter = len(parts[0])
      for ii in range(1, len(parts)):
        # Don't split on the dot in "if (.condition)".
        if (not parts[ii - 1][-3:] == "if " and
            # Don't split "(.)" or "(.*fptr)".
            (len(parts[ii]) > 0 and
                not parts[ii][0] == ")" and not fptr.match(parts[ii]))
            and splitter < 80):
          return splitter
        splitter += len(parts[ii]) + 1
    done = False
    end = len(string)
    last_splitter = -1
    while not done:
      splitter = string[0:end].rfind(',')
      if splitter < 0 or (splitter > 0 and string[splitter - 1] == '"'):
        return last_splitter
      elif splitter >= 80:
        end = splitter
      else:
        return splitter

  def __WriteLine(self, line, ends_with_eol):
    """Given a signle line, writes it to a file, splitting if it's > 80 chars"""
    if len(line) >= 80:
      i = self.__FindSplit(line)
      if i > 0:
        line1 = line[0:i + 1]
        if line1[-1] == ' ':
          line1 = line1[:-1]
        lineend = ''
        if line1[0] == '#':
          lineend = ' \\'
        nolint = ''
        if len(line1) > 80:
          nolint = '  // NOLINT'
        self.__AddLine(line1 + nolint + lineend + '\n')
        match = re.match("( +)", line1)
        indent = ""
        if match:
          indent = match.group(1)
        splitter = line[i]
        if not splitter == ',':
          indent = "    " + indent
        self.__WriteLine(indent + line[i + 1:].lstrip(), True)
        return
    nolint = ''
    if len(line) > 80:
      nolint = '  // NOLINT'
    self.__AddLine(line + nolint)
    if ends_with_eol:
      self.__AddLine('\n')

  def __AddLine(self, line):
    self.content.append(line)

  def Close(self):
    """Close the file."""
    content = "".join(self.content)
    write_file = True
    if os.path.exists(self.filename):
      old_file = open(self.filename, "rb");
      old_content = old_file.read()
      old_file.close();
      if content == old_content:
        write_file = False
    if write_file:
      file = open(self.filename, "wb")
      file.write(content)
      file.close()


class CHeaderWriter(CWriter):
  """Writes a C Header file."""

  _non_alnum_re = re.compile(r'[^a-zA-Z0-9]')

  def __init__(self, filename, file_comment = None, guard_depth = 3):
    CWriter.__init__(self, filename)

    base = os.path.dirname(os.path.abspath(filename))
    for i in range(guard_depth):
      base = os.path.dirname(base)

    hpath = os.path.abspath(filename)[len(base) + 1:]
    self.guard = self._non_alnum_re.sub('_', hpath).upper() + '_'

    self.Write(_LICENSE)
    self.Write(_DO_NOT_EDIT_WARNING)
    if not file_comment == None:
      self.Write(file_comment)
    self.Write("#ifndef %s\n" % self.guard)
    self.Write("#define %s\n\n" % self.guard)

  def Close(self):
    self.Write("#endif  // %s\n\n" % self.guard)
    CWriter.Close(self)

class TypeHandler(object):
  """This class emits code for a particular type of function."""

  _remove_expected_call_re = re.compile(r'  EXPECT_CALL.*?;\n', re.S)

  def __init__(self):
    pass

  def InitFunction(self, func):
    """Add or adjust anything type specific for this function."""
    if func.GetInfo('needs_size') and not func.name.endswith('Bucket'):
      func.AddCmdArg(DataSizeArgument('data_size'))

  def AddImmediateFunction(self, generator, func):
    """Adds an immediate version of a function."""
    # Generate an immediate command if there is only 1 pointer arg.
    immediate = func.GetInfo('immediate')  # can be True, False or None
    if immediate == True or immediate == None:
      if func.num_pointer_args == 1 or immediate:
        generator.AddFunction(ImmediateFunction(func))

  def AddBucketFunction(self, generator, func):
    """Adds a bucket version of a function."""
    # Generate an immediate command if there is only 1 pointer arg.
    bucket = func.GetInfo('bucket')  # can be True, False or None
    if bucket:
      generator.AddFunction(BucketFunction(func))

  def WriteStruct(self, func, file):
    """Writes a structure that matches the arguments to a function."""
    comment = func.GetInfo('cmd_comment')
    if not comment == None:
      file.Write(comment)
    file.Write("struct %s {\n" % func.name)
    file.Write("  typedef %s ValueType;\n" % func.name)
    file.Write("  static const CommandId kCmdId = k%s;\n" % func.name)
    func.WriteCmdArgFlag(file)
    file.Write("\n")
    result = func.GetInfo('result')
    if not result == None:
      if len(result) == 1:
        file.Write("  typedef %s Result;\n\n" % result[0])
      else:
        file.Write("  struct Result {\n")
        for line in result:
          file.Write("    %s;\n" % line)
        file.Write("  };\n\n")

    func.WriteCmdComputeSize(file)
    func.WriteCmdSetHeader(file)
    func.WriteCmdInit(file)
    func.WriteCmdSet(file)

    file.Write("  gpu::CommandHeader header;\n")
    args = func.GetCmdArgs()
    for arg in args:
      file.Write("  %s %s;\n" % (arg.cmd_type, arg.name))
    file.Write("};\n")
    file.Write("\n")

    size = len(args) * _SIZE_OF_UINT32 + _SIZE_OF_COMMAND_HEADER
    file.Write("COMPILE_ASSERT(sizeof(%s) == %d,\n" % (func.name, size))
    file.Write("               Sizeof_%s_is_not_%d);\n" % (func.name, size))
    file.Write("COMPILE_ASSERT(offsetof(%s, header) == 0,\n" % func.name)
    file.Write("               OffsetOf_%s_header_not_0);\n" % func.name)
    offset = _SIZE_OF_COMMAND_HEADER
    for arg in args:
      file.Write("COMPILE_ASSERT(offsetof(%s, %s) == %d,\n" %
                 (func.name, arg.name, offset))
      file.Write("               OffsetOf_%s_%s_not_%d);\n" %
                 (func.name, arg.name, offset))
      offset += _SIZE_OF_UINT32
    if not result == None and len(result) > 1:
      offset = 0;
      for line in result:
        parts = line.split()
        name = parts[-1]
        check = """
COMPILE_ASSERT(offsetof(%(cmd_name)s::Result, %(field_name)s) == %(offset)d,
               OffsetOf_%(cmd_name)s_Result_%(field_name)s_not_%(offset)d);
"""
        file.Write((check.strip() + "\n") % {
              'cmd_name': func.name,
              'field_name': name,
              'offset': offset,
            })
        offset += _SIZE_OF_UINT32
    file.Write("\n")

  def WriteHandlerImplementation(self, func, file):
    """Writes the handler implementation for this command."""
    file.Write("  %s(%s);\n" %
               (func.GetGLFunctionName(), func.MakeOriginalArgString("")))

  def WriteCmdSizeTest(self, func, file):
    """Writes the size test for a command."""
    file.Write("  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);\n")

  def WriteFormatTest(self, func, file):
    """Writes a format test for a command."""
    file.Write("TEST_F(GLES2FormatTest, %s) {\n" % func.name)
    file.Write("  %s& cmd = *GetBufferAs<%s>();\n" % (func.name, func.name))
    file.Write("  void* next_cmd = cmd.Set(\n")
    file.Write("      &cmd")
    args = func.GetCmdArgs()
    value = 11
    for arg in args:
      file.Write(",\n      static_cast<%s>(%d)" % (arg.type, value))
      value += 1
    file.Write(");\n")
    value = 11
    file.Write("  EXPECT_EQ(static_cast<uint32>(%s::kCmdId),\n" % func.name)
    file.Write("            cmd.header.command);\n")
    func.type_handler.WriteCmdSizeTest(func, file)
    for arg in args:
      file.Write("  EXPECT_EQ(static_cast<%s>(%d), cmd.%s);\n" %
                 (arg.type, value, arg.name))
      value += 1
    file.Write("  CheckBytesWrittenMatchesExpectedSize(\n")
    file.Write("      next_cmd, sizeof(cmd));\n")
    file.Write("}\n")
    file.Write("\n")

  def WriteImmediateFormatTest(self, func, file):
    """Writes a format test for an immediate version of a command."""
    pass

  def WriteBucketFormatTest(self, func, file):
    """Writes a format test for a bucket version of a command."""
    pass

  def WriteGetDataSizeCode(self, func, file):
    """Writes the code to set data_size used in validation"""
    pass

  def WriteImmediateCmdSizeTest(self, func, file):
    """Writes a size test for an immediate version of a command."""
    file.Write("  // TODO(gman): Compute correct size.\n")
    file.Write("  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);\n")

  def WriteImmediateHandlerImplementation (self, func, file):
    """Writes the handler impl for the immediate version of a command."""
    file.Write("  %s(%s);\n" %
               (func.GetGLFunctionName(), func.MakeOriginalArgString("")))

  def WriteBucketHandlerImplementation (self, func, file):
    """Writes the handler impl for the bucket version of a command."""
    file.Write("  %s(%s);\n" %
               (func.GetGLFunctionName(), func.MakeOriginalArgString("")))

  def WriteServiceImplementation(self, func, file):
    """Writes the service implementation for a command."""
    file.Write(
        "error::Error GLES2DecoderImpl::Handle%s(\n" % func.name)
    file.Write(
        "    uint32 immediate_data_size, const gles2::%s& c) {\n" % func.name)
    if len(func.GetOriginalArgs()) > 0:
      last_arg = func.GetLastOriginalArg()
      all_but_last_arg = func.GetOriginalArgs()[:-1]
      for arg in all_but_last_arg:
        arg.WriteGetCode(file)
      self.WriteGetDataSizeCode(func, file)
      last_arg.WriteGetCode(file)
    func.WriteHandlerValidation(file)
    func.WriteHandlerImplementation(file)
    file.Write("  return error::kNoError;\n")
    file.Write("}\n")
    file.Write("\n")

  def WriteImmediateServiceImplementation(self, func, file):
    """Writes the service implementation for an immediate version of command."""
    file.Write(
        "error::Error GLES2DecoderImpl::Handle%s(\n" % func.name)
    file.Write(
        "    uint32 immediate_data_size, const gles2::%s& c) {\n" % func.name)
    last_arg = func.GetLastOriginalArg()
    all_but_last_arg = func.GetOriginalArgs()[:-1]
    for arg in all_but_last_arg:
      arg.WriteGetCode(file)
    self.WriteGetDataSizeCode(func, file)
    last_arg.WriteGetCode(file)
    func.WriteHandlerValidation(file)
    func.WriteHandlerImplementation(file)
    file.Write("  return error::kNoError;\n")
    file.Write("}\n")
    file.Write("\n")

  def WriteBucketServiceImplementation(self, func, file):
    """Writes the service implementation for a bucket version of command."""
    file.Write(
        "error::Error GLES2DecoderImpl::Handle%s(\n" % func.name)
    file.Write(
        "    uint32 immediate_data_size, const gles2::%s& c) {\n" % func.name)
    last_arg = func.GetLastOriginalArg()
    all_but_last_arg = func.GetOriginalArgs()[:-1]
    for arg in all_but_last_arg:
      arg.WriteGetCode(file)
    self.WriteGetDataSizeCode(func, file)
    last_arg.WriteGetCode(file)
    func.WriteHandlerValidation(file)
    func.WriteHandlerImplementation(file)
    file.Write("  return error::kNoError;\n")
    file.Write("}\n")
    file.Write("\n")

  def WriteValidUnitTest(self, func, file, test, extra = {}):
    """Writes a valid unit test."""
    if func.GetInfo('expectation') == False:
      test = self._remove_expected_call_re.sub('', test)
    name = func.name
    arg_strings = []
    count = 0
    for arg in func.GetOriginalArgs():
      arg_strings.append(arg.GetValidArg(func, count, 0))
      count += 1
    gl_arg_strings = []
    count = 0
    for arg in func.GetOriginalArgs():
      gl_arg_strings.append(arg.GetValidGLArg(func, count, 0))
      count += 1
    gl_func_name = func.GetGLTestFunctionName()
    vars = {
      'test_name': 'GLES2DecoderTest%d' % file.file_num,
      'name':name,
      'gl_func_name': gl_func_name,
      'args': ", ".join(arg_strings),
      'gl_args': ", ".join(gl_arg_strings),
    }
    vars.update(extra)
    file.Write(test % vars)

  def WriteInvalidUnitTest(self, func, file, test, extra = {}):
    """Writes a invalid unit test."""
    arg_index = 0
    for arg in func.GetOriginalArgs():
      num_invalid_values = arg.GetNumInvalidValues(func)
      for value_index in range(0, num_invalid_values):
        arg_strings = []
        parse_result = "kNoError"
        gl_error = None
        count = 0
        for arg in func.GetOriginalArgs():
          if count == arg_index:
            (arg_string, parse_result, gl_error) = arg.GetInvalidArg(
                count, value_index)
          else:
            arg_string = arg.GetValidArg(func, count, 0)
          arg_strings.append(arg_string)
          count += 1
        gl_arg_strings = []
        count = 0
        for arg in func.GetOriginalArgs():
          gl_arg_strings.append("_")
          count += 1
        gl_func_name = func.GetGLTestFunctionName()
        gl_error_test = ''
        if not gl_error == None:
          gl_error_test = '\n  EXPECT_EQ(%s, GetGLError());' % gl_error

        vars = {
          'test_name': 'GLES2DecoderTest%d' % file.file_num ,
          'name': func.name,
          'arg_index': arg_index,
          'value_index': value_index,
          'gl_func_name': gl_func_name,
          'args': ", ".join(arg_strings),
          'all_but_last_args': ", ".join(arg_strings[:-1]),
          'gl_args': ", ".join(gl_arg_strings),
          'parse_result': parse_result,
            'gl_error_test': gl_error_test,
        }
        vars.update(extra)
        file.Write(test % vars)
      arg_index += 1

  def WriteServiceUnitTest(self, func, file):
    """Writes the service unit test for a command."""
    valid_test = """
TEST_F(%(test_name)s, %(name)sValidArgs) {
  EXPECT_CALL(*gl_, %(gl_func_name)s(%(gl_args)s));
  SpecializedSetup<%(name)s, 0>(true);
  %(name)s cmd;
  cmd.Init(%(args)s);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}
"""
    self.WriteValidUnitTest(func, file, valid_test)

    invalid_test = """
TEST_F(%(test_name)s, %(name)sInvalidArgs%(arg_index)d_%(value_index)d) {
  EXPECT_CALL(*gl_, %(gl_func_name)s(%(gl_args)s)).Times(0);
  SpecializedSetup<%(name)s, 0>(false);
  %(name)s cmd;
  cmd.Init(%(args)s);
  EXPECT_EQ(error::%(parse_result)s, ExecuteCmd(cmd));%(gl_error_test)s
}
"""
    self.WriteInvalidUnitTest(func, file, invalid_test)

  def WriteImmediateServiceUnitTest(self, func, file):
    """Writes the service unit test for an immediate command."""
    file.Write("// TODO(gman): %s\n" % func.name)

  def WriteImmediateValidationCode(self, func, file):
    """Writes the validation code for an immediate version of a command."""
    pass

  def WriteBucketServiceUnitTest(self, func, file):
    """Writes the service unit test for a bucket command."""
    file.Write("// TODO(gman): %s\n" % func.name)

  def WriteBucketValidationCode(self, func, file):
    """Writes the validation code for a bucket version of a command."""
    file.Write("// TODO(gman): %s\n" % func.name)

  def WriteGLES2ImplementationDeclaration(self, func, file):
    """Writes the GLES2 Implemention declaration."""
    impl_decl = func.GetInfo('impl_decl')
    if impl_decl == None or impl_decl == True:
      file.Write("%s %s(%s);\n" %
                 (func.return_type, func.original_name,
                  func.MakeTypedOriginalArgString("")))
      file.Write("\n")

  def WriteGLES2CLibImplementation(self, func, file):
    file.Write("%s GLES2%s(%s) {\n" %
               (func.return_type, func.name,
                func.MakeTypedOriginalArgString("")))
    result_string = "return "
    if func.return_type == "void":
      result_string = ""
    file.Write("  %sgles2::GetGLContext()->%s(%s);\n" %
               (result_string, func.original_name,
                func.MakeOriginalArgString("")))
    file.Write("}\n")

  def WriteClientGLCallLog(self, func, file):
    """Writes a logging macro for the client side code."""
    comma = ""
    if len(func.GetOriginalArgs()):
      comma = " << "
    file.Write(
        '  GPU_CLIENT_LOG("[" << this << "] gl%s("%s%s << ")");\n' %
        (func.original_name, comma, func.MakeLogArgString()))

  def WriteClientGLReturnLog(self, func, file):
    """Writes the return value logging code."""
    if func.return_type != "void":
      file.Write('  GPU_CLIENT_LOG("return:" << result)\n')

  def WriteGLES2ImplementationHeader(self, func, file):
    """Writes the GLES2 Implemention."""
    impl_func = func.GetInfo('impl_func')
    impl_decl = func.GetInfo('impl_decl')
    gen_cmd = func.GetInfo('gen_cmd')
    if (func.can_auto_generate and
        (impl_func == None or impl_func == True) and
        (impl_decl == None or impl_decl == True) and
        (gen_cmd == None or gen_cmd == True)):
      file.Write("%s %s(%s) {\n" %
                 (func.return_type, func.original_name,
                  func.MakeTypedOriginalArgString("")))
      file.Write("  GPU_CLIENT_SINGLE_THREAD_CHECK();\n")
      self.WriteClientGLCallLog(func, file)
      func.WriteDestinationInitalizationValidation(file)
      for arg in func.GetOriginalArgs():
        arg.WriteClientSideValidationCode(file, func)
      file.Write("  helper_->%s(%s);\n" %
                 (func.name, func.MakeOriginalArgString("")))
      self.WriteClientGLReturnLog(func, file)
      file.Write("}\n")
      file.Write("\n")
    else:
      self.WriteGLES2ImplementationDeclaration(func, file)

  def WriteGLES2ImplementationUnitTest(self, func, file):
    """Writes the GLES2 Implemention unit test."""
    client_test = func.GetInfo('client_test')
    if (func.can_auto_generate and
        (client_test == None or client_test == True)):
      code = """
TEST_F(GLES2ImplementationTest, %(name)s) {
  struct Cmds {
    %(name)s cmd;
  };
  Cmds expected;
  expected.cmd.Init(%(cmd_args)s);

  gl_->%(name)s(%(args)s);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}
"""
      cmd_arg_strings = []
      count = 0
      for arg in func.GetCmdArgs():
        cmd_arg_strings.append(arg.GetValidClientSideCmdArg(func, count, 0))
        count += 1
      gl_arg_strings = []
      count = 0
      for arg in func.GetOriginalArgs():
        gl_arg_strings.append(arg.GetValidClientSideArg(func, count, 0))
        count += 1
      file.Write(code % {
            'name': func.name,
            'args': ", ".join(gl_arg_strings),
            'cmd_args': ", ".join(cmd_arg_strings),
          })
    else:
      if client_test != False:
        file.Write("// TODO: Implement unit test for %s\n" % func.name)

  def WriteDestinationInitalizationValidation(self, func, file):
    """Writes the client side destintion initialization validation."""
    for arg in func.GetOriginalArgs():
      arg.WriteDestinationInitalizationValidation(file, func)

  def WriteImmediateCmdComputeSize(self, func, file):
    """Writes the size computation code for the immediate version of a cmd."""
    file.Write("  static uint32 ComputeSize(uint32 size_in_bytes) {\n")
    file.Write("    return static_cast<uint32>(\n")
    file.Write("        sizeof(ValueType) +  // NOLINT\n")
    file.Write("        RoundSizeToMultipleOfEntries(size_in_bytes));\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdSetHeader(self, func, file):
    """Writes the SetHeader function for the immediate version of a cmd."""
    file.Write("  void SetHeader(uint32 size_in_bytes) {\n")
    file.Write("    header.SetCmdByTotalSize<ValueType>(size_in_bytes);\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdInit(self, func, file):
    """Writes the Init function for the immediate version of a command."""
    raise NotImplementedError(func.name)

  def WriteImmediateCmdSet(self, func, file):
    """Writes the Set function for the immediate version of a command."""
    raise NotImplementedError(func.name)

  def WriteCmdHelper(self, func, file):
    """Writes the cmd helper definition for a cmd."""
    code = """  void %(name)s(%(typed_args)s) {
    gles2::%(name)s* c = GetCmdSpace<gles2::%(name)s>();
    if (c) {
      c->Init(%(args)s);
    }
  }

"""
    file.Write(code % {
          "name": func.name,
          "typed_args": func.MakeTypedCmdArgString(""),
          "args": func.MakeCmdArgString(""),
        })

  def WriteImmediateCmdHelper(self, func, file):
    """Writes the cmd helper definition for the immediate version of a cmd."""
    code = """  void %(name)s(%(typed_args)s) {
    const uint32 s = 0;  // TODO(gman): compute correct size
    gles2::%(name)s* c =
        GetImmediateCmdSpaceTotalSize<gles2::%(name)s>(s);
    if (c) {
      c->Init(%(args)s);
    }
  }

"""
    file.Write(code % {
           "name": func.name,
           "typed_args": func.MakeTypedCmdArgString(""),
           "args": func.MakeCmdArgString(""),
        })


class CustomHandler(TypeHandler):
  """Handler for commands that are auto-generated but require minor tweaks."""

  def __init__(self):
    TypeHandler.__init__(self)

  def WriteServiceImplementation(self, func, file):
    """Overrriden from TypeHandler."""
    pass

  def WriteImmediateServiceImplementation(self, func, file):
    """Overrriden from TypeHandler."""
    pass

  def WriteBucketServiceImplementation(self, func, file):
    """Overrriden from TypeHandler."""
    pass

  def WriteServiceUnitTest(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("// TODO(gman): %s\n\n" % func.name)

  def WriteImmediateServiceUnitTest(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("// TODO(gman): %s\n\n" % func.name)

  def WriteImmediateCmdGetTotalSize(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("    uint32 total_size = 0;  // TODO(gman): get correct size.\n")

  def WriteImmediateCmdInit(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  void Init(%s) {\n" % func.MakeTypedCmdArgString("_"))
    self.WriteImmediateCmdGetTotalSize(func, file)
    file.Write("    SetHeader(total_size);\n")
    args = func.GetCmdArgs()
    for arg in args:
      file.Write("    %s = _%s;\n" % (arg.name, arg.name))
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdSet(self, func, file):
    """Overrriden from TypeHandler."""
    copy_args = func.MakeCmdArgString("_", False)
    file.Write("  void* Set(void* cmd%s) {\n" %
               func.MakeTypedCmdArgString("_", True))
    self.WriteImmediateCmdGetTotalSize(func, file)
    file.Write("    static_cast<ValueType*>(cmd)->Init(%s);\n" % copy_args)
    file.Write("    return NextImmediateCmdAddressTotalSize<ValueType>("
               "cmd, total_size);\n")
    file.Write("  }\n")
    file.Write("\n")


class TodoHandler(CustomHandler):
  """Handle for commands that are not yet implemented."""

  def AddImmediateFunction(self, generator, func):
    """Overrriden from TypeHandler."""
    pass

  def WriteImmediateFormatTest(self, func, file):
    """Overrriden from TypeHandler."""
    pass

  def WriteGLES2ImplementationUnitTest(self, func, file):
    """Overrriden from TypeHandler."""
    pass

  def WriteGLES2ImplementationHeader(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("%s %s(%s) {\n" %
               (func.return_type, func.original_name,
                func.MakeTypedOriginalArgString("")))
    file.Write("  // TODO: for now this is a no-op\n")
    file.Write(
        "  SetGLError(GL_INVALID_OPERATION, \"gl%s not implemented\");\n" %
        func.name)
    if func.return_type != "void":
      file.Write("  return 0;\n")
    file.Write("}\n")
    file.Write("\n")

  def WriteServiceImplementation(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write(
        "error::Error GLES2DecoderImpl::Handle%s(\n" % func.name)
    file.Write(
        "    uint32 immediate_data_size, const gles2::%s& c) {\n" % func.name)
    file.Write("  // TODO: for now this is a no-op\n")
    file.Write(
        "  SetGLError(GL_INVALID_OPERATION, \"gl%s not implemented\");\n" %
        func.name)
    file.Write("  return error::kNoError;\n")
    file.Write("}\n")
    file.Write("\n")


class HandWrittenHandler(CustomHandler):
  """Handler for comands where everything must be written by hand."""

  def InitFunction(self, func):
    """Add or adjust anything type specific for this function."""
    CustomHandler.InitFunction(self, func)
    func.can_auto_generate = False

  def WriteStruct(self, func, file):
    """Overrriden from TypeHandler."""
    pass

  def WriteDocs(self, func, file):
    """Overrriden from TypeHandler."""
    pass

  def WriteServiceUnitTest(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("// TODO(gman): %s\n\n" % func.name)

  def WriteImmediateServiceUnitTest(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("// TODO(gman): %s\n\n" % func.name)

  def WriteBucketServiceUnitTest(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("// TODO(gman): %s\n\n" % func.name)

  def WriteServiceImplementation(self, func, file):
    """Overrriden from TypeHandler."""
    pass

  def WriteImmediateServiceImplementation(self, func, file):
    """Overrriden from TypeHandler."""
    pass

  def WriteBucketServiceImplementation(self, func, file):
    """Overrriden from TypeHandler."""
    pass

  def WriteImmediateCmdHelper(self, func, file):
    """Overrriden from TypeHandler."""
    pass

  def WriteBucketCmdHelper(self, func, file):
    """Overrriden from TypeHandler."""
    pass

  def WriteCmdHelper(self, func, file):
    """Overrriden from TypeHandler."""
    pass

  def WriteFormatTest(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("// TODO(gman): Write test for %s\n" % func.name)

  def WriteImmediateFormatTest(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("// TODO(gman): Write test for %s\n" % func.name)

  def WriteBucketFormatTest(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("// TODO(gman): Write test for %s\n" % func.name)



class ManualHandler(CustomHandler):
  """Handler for commands who's handlers must be written by hand."""

  def __init__(self):
    CustomHandler.__init__(self)

  def InitFunction(self, func):
    """Overrriden from TypeHandler."""
    if (func.name == 'CompressedTexImage2DBucket'):
      func.cmd_args = func.cmd_args[:-1]
      func.AddCmdArg(Argument('bucket_id', 'GLuint'))
    else:
      CustomHandler.InitFunction(self, func)

  def WriteServiceImplementation(self, func, file):
    """Overrriden from TypeHandler."""
    pass

  def WriteBucketServiceImplementation(self, func, file):
    """Overrriden from TypeHandler."""
    pass

  def WriteServiceUnitTest(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("// TODO(gman): %s\n\n" % func.name)

  def WriteImmediateServiceUnitTest(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("// TODO(gman): %s\n\n" % func.name)

  def WriteImmediateServiceImplementation(self, func, file):
    """Overrriden from TypeHandler."""
    pass

  def WriteImmediateFormatTest(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("// TODO(gman): Implement test for %s\n" % func.name)

  def WriteGLES2ImplementationHeader(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("%s %s(%s);\n" %
               (func.return_type, func.original_name,
                func.MakeTypedOriginalArgString("")))
    file.Write("\n")

  def WriteImmediateCmdGetTotalSize(self, func, file):
    """Overrriden from TypeHandler."""
    # TODO(gman): Move this data to _FUNCTION_INFO?
    if func.name == 'ShaderSourceImmediate':
      file.Write("    uint32 total_size = ComputeSize(_data_size);\n")
    else:
      CustomHandler.WriteImmediateCmdGetTotalSize(self, func, file)


class DataHandler(TypeHandler):
  """Handler for glBufferData, glBufferSubData, glTexImage2D, glTexSubImage2D,
     glCompressedTexImage2D, glCompressedTexImageSub2D."""
  def __init__(self):
    TypeHandler.__init__(self)

  def InitFunction(self, func):
    """Overrriden from TypeHandler."""
    if func.name == 'CompressedTexSubImage2DBucket':
      func.cmd_args = func.cmd_args[:-1]
      func.AddCmdArg(Argument('bucket_id', 'GLuint'))

  def WriteGetDataSizeCode(self, func, file):
    """Overrriden from TypeHandler."""
    # TODO(gman): Move this data to _FUNCTION_INFO?
    name = func.name
    if name.endswith("Immediate"):
      name = name[0:-9]
    if name == 'BufferData' or name == 'BufferSubData':
      file.Write("  uint32 data_size = size;\n")
    elif (name == 'CompressedTexImage2D' or
          name == 'CompressedTexSubImage2D'):
      file.Write("  uint32 data_size = imageSize;\n")
    elif (name == 'CompressedTexSubImage2DBucket'):
      file.Write("  Bucket* bucket = GetBucket(c.bucket_id);\n")
      file.Write("  uint32 data_size = bucket->size();\n")
      file.Write("  GLsizei imageSize = data_size;\n")
    elif name == 'TexImage2D' or name == 'TexSubImage2D':
      code = """  uint32 data_size;
  if (!GLES2Util::ComputeImageDataSize(
      width, height, format, type, unpack_alignment_, &data_size)) {
    return error::kOutOfBounds;
  }
"""
      file.Write(code)
    else:
      file.Write("// uint32 data_size = 0;  // TODO(gman): get correct size!\n")

  def WriteImmediateCmdGetTotalSize(self, func, file):
    """Overrriden from TypeHandler."""
    # TODO(gman): Move this data to _FUNCTION_INFO?
    if func.name == 'BufferDataImmediate':
      file.Write("    uint32 total_size = ComputeSize(_size);\n")
    elif func.name == 'BufferSubDataImmediate':
        file.Write("    uint32 total_size = ComputeSize(_size);\n")
    elif func.name == 'CompressedTexImage2DImmediate':
        file.Write("    uint32 total_size = ComputeSize(_imageSize);\n")
    elif func.name == 'CompressedTexSubImage2DImmediate':
        file.Write("    uint32 total_size = ComputeSize(_imageSize);\n")
    elif func.name == 'TexImage2DImmediate':
      file.Write(
          "    uint32 total_size = 0;  // TODO(gman): get correct size\n")
    elif func.name == 'TexSubImage2DImmediate':
      file.Write(
          "    uint32 total_size = 0;  // TODO(gman): get correct size\n")

  def WriteImmediateCmdSizeTest(self, func, file):
    """Overrriden from TypeHandler."""
    # TODO(gman): Move this data to _FUNCTION_INFO?
    if func.name == 'BufferDataImmediate':
      file.Write("    uint32 total_size = cmd.ComputeSize(cmd.size);\n")
    elif func.name == 'BufferSubDataImmediate':
        file.Write("    uint32 total_size = cmd.ComputeSize(cmd.size);\n")
    elif func.name == 'CompressedTexImage2DImmediate':
        file.Write("    uint32 total_size = cmd.ComputeSize(cmd.imageSize);\n")
    elif func.name == 'CompressedTexSubImage2DImmediate':
        file.Write("    uint32 total_size = cmd.ComputeSize(cmd.imageSize);\n")
    elif func.name == 'TexImage2DImmediate':
      file.Write(
          "    uint32 total_size = 0;  // TODO(gman): get correct size\n")
    elif func.name == 'TexSubImage2DImmediate':
      file.Write(
          "    uint32 total_size = 0;  // TODO(gman): get correct size\n")
    file.Write("  EXPECT_EQ(sizeof(cmd), total_size);\n")

  def WriteImmediateCmdInit(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  void Init(%s) {\n" % func.MakeTypedCmdArgString("_"))
    self.WriteImmediateCmdGetTotalSize(func, file)
    file.Write("    SetHeader(total_size);\n")
    args = func.GetCmdArgs()
    for arg in args:
      file.Write("    %s = _%s;\n" % (arg.name, arg.name))
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdSet(self, func, file):
    """Overrriden from TypeHandler."""
    copy_args = func.MakeCmdArgString("_", False)
    file.Write("  void* Set(void* cmd%s) {\n" %
               func.MakeTypedCmdArgString("_", True))
    self.WriteImmediateCmdGetTotalSize(func, file)
    file.Write("    static_cast<ValueType*>(cmd)->Init(%s);\n" % copy_args)
    file.Write("    return NextImmediateCmdAddressTotalSize<ValueType>("
               "cmd, total_size);\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateFormatTest(self, func, file):
    """Overrriden from TypeHandler."""
    # TODO(gman): Remove this exception.
    file.Write("// TODO(gman): Implement test for %s\n" % func.name)
    return

  def WriteServiceUnitTest(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("// TODO(gman): %s\n\n" % func.name)

  def WriteImmediateServiceUnitTest(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("// TODO(gman): %s\n\n" % func.name)

  def WriteBucketServiceImplementation(self, func, file):
    """Overrriden from TypeHandler."""
    if not func.name == 'CompressedTexSubImage2DBucket':
      TypeHandler.WriteBucketServiceImplemenation(self, func, file)


class BindHandler(TypeHandler):
  """Handler for glBind___ type functions."""

  def __init__(self):
    TypeHandler.__init__(self)

  def WriteServiceUnitTest(self, func, file):
    """Overrriden from TypeHandler."""
    valid_test = """
TEST_F(%(test_name)s, %(name)sValidArgs) {
  EXPECT_CALL(*gl_, %(gl_func_name)s(%(gl_args)s));
  SpecializedSetup<%(name)s, 0>(true);
  %(name)s cmd;
  cmd.Init(%(args)s);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(%(test_name)s, %(name)sValidArgsNewId) {
  EXPECT_CALL(*gl_, %(gl_func_name)s(%(first_gl_arg)s, kNewServiceId));
  EXPECT_CALL(*gl_, %(gl_gen_func_name)s(1, _))
     .WillOnce(SetArgumentPointee<1>(kNewServiceId));
  SpecializedSetup<%(name)s, 0>(true);
  %(name)s cmd;
  cmd.Init(%(first_arg)s, kNewClientId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_TRUE(Get%(resource_type)sInfo(kNewClientId) != NULL);
}
"""
    gen_func_names = {
    }
    self.WriteValidUnitTest(func, file, valid_test, {
        'first_arg': func.GetOriginalArgs()[0].GetValidArg(func, 0, 0),
        'first_gl_arg': func.GetOriginalArgs()[0].GetValidGLArg(func, 0, 0),
        'resource_type': func.GetOriginalArgs()[1].resource_type,
        'gl_gen_func_name': func.GetInfo("gen_func"),
    })

    invalid_test = """
TEST_F(%(test_name)s, %(name)sInvalidArgs%(arg_index)d_%(value_index)d) {
  EXPECT_CALL(*gl_, %(gl_func_name)s(%(gl_args)s)).Times(0);
  SpecializedSetup<%(name)s, 0>(false);
  %(name)s cmd;
  cmd.Init(%(args)s);
  EXPECT_EQ(error::%(parse_result)s, ExecuteCmd(cmd));%(gl_error_test)s
}
"""
    self.WriteInvalidUnitTest(func, file, invalid_test)

  def WriteGLES2ImplementationHeader(self, func, file):
    """Writes the GLES2 Implemention."""
    impl_func = func.GetInfo('impl_func')
    impl_decl = func.GetInfo('impl_decl')
    if (func.can_auto_generate and
        (impl_func == None or impl_func == True) and
        (impl_decl == None or impl_decl == True)):
      file.Write("%s %s(%s) {\n" %
                 (func.return_type, func.original_name,
                  func.MakeTypedOriginalArgString("")))
      file.Write("  GPU_CLIENT_SINGLE_THREAD_CHECK();\n")
      func.WriteDestinationInitalizationValidation(file)
      self.WriteClientGLCallLog(func, file)
      for arg in func.GetOriginalArgs():
        arg.WriteClientSideValidationCode(file, func)
      code = """  if (Is%(type)sReservedId(%(id)s)) {
    SetGLError(GL_INVALID_OPERATION, "%(name)s: %(id)s reserved id");
    return;
  }
  Bind%(type)sHelper(%(arg_string)s);
  helper_->%(name)s(%(arg_string)s);
}

"""
      file.Write(code % {
          'name': func.name,
          'arg_string': func.MakeOriginalArgString(""),
          'id': func.GetOriginalArgs()[1].name,
          'type': func.GetOriginalArgs()[1].resource_type,
          'lc_type': func.GetOriginalArgs()[1].resource_type.lower(),
        })
    else:
      self.WriteGLES2ImplementationDeclaration(func, file)


class GENnHandler(TypeHandler):
  """Handler for glGen___ type functions."""

  def __init__(self):
    TypeHandler.__init__(self)

  def InitFunction(self, func):
    """Overrriden from TypeHandler."""
    pass

  def WriteGetDataSizeCode(self, func, file):
    """Overrriden from TypeHandler."""
    code = """  uint32 data_size;
  if (!SafeMultiplyUint32(n, sizeof(GLuint), &data_size)) {
    return error::kOutOfBounds;
  }
"""
    file.Write(code)

  def WriteHandlerImplementation (self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  if (!%sHelper(n, %s)) {\n"
               "    return error::kInvalidArguments;\n"
               "  }\n" %
               (func.name, func.GetLastOriginalArg().name))

  def WriteImmediateHandlerImplementation(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  if (!%sHelper(n, %s)) {\n"
               "    return error::kInvalidArguments;\n"
               "  }\n" %
               (func.original_name, func.GetLastOriginalArg().name))

  def WriteGLES2ImplementationHeader(self, func, file):
    """Overrriden from TypeHandler."""
    log_code = ("""  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < n; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << %s[i]);
    }
  });""" % func.GetOriginalArgs()[1].name)
    args = {
        'log_code': log_code,
        'return_type': func.return_type,
        'name': func.original_name,
        'typed_args': func.MakeTypedOriginalArgString(""),
        'args': func.MakeOriginalArgString(""),
        'resource_types': func.GetInfo('resource_types'),
        'count_name': func.GetOriginalArgs()[0].name,
      }
    file.Write("%(return_type)s %(name)s(%(typed_args)s) {\n" % args)
    func.WriteDestinationInitalizationValidation(file)
    self.WriteClientGLCallLog(func, file)
    for arg in func.GetOriginalArgs():
      arg.WriteClientSideValidationCode(file, func)
    code = """  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GetIdHandler(id_namespaces::k%(resource_types)s)->
      MakeIds(this, 0, %(args)s);
  helper_->%(name)sImmediate(%(args)s);
%(log_code)s
}

"""
    file.Write(code % args)

  def WriteGLES2ImplementationUnitTest(self, func, file):
    """Overrriden from TypeHandler."""
    code = """
TEST_F(GLES2ImplementationTest, %(name)s) {
  GLuint ids[2] = { 0, };
  struct Cmds {
    %(name)sImmediate gen;
    GLuint data[2];
  };
  Cmds expected;
  expected.gen.Init(arraysize(ids), &ids[0]);
  expected.data[0] = k%(types)sStartId;
  expected.data[1] = k%(types)sStartId + 1;
  gl_->%(name)s(arraysize(ids), &ids[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(k%(types)sStartId, ids[0]);
  EXPECT_EQ(k%(types)sStartId + 1, ids[1]);
}
"""
    file.Write(code % {
          'name': func.name,
          'types': func.GetInfo('resource_types'),
        })

  def WriteServiceUnitTest(self, func, file):
    """Overrriden from TypeHandler."""
    valid_test = """
TEST_F(%(test_name)s, %(name)sValidArgs) {
  EXPECT_CALL(*gl_, %(gl_func_name)s(1, _))
      .WillOnce(SetArgumentPointee<1>(kNewServiceId));
  GetSharedMemoryAs<GLuint*>()[0] = kNewClientId;
  SpecializedSetup<%(name)s, 0>(true);
  %(name)s cmd;
  cmd.Init(%(args)s);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_TRUE(Get%(resource_name)sInfo(kNewClientId) != NULL);
}
"""
    self.WriteValidUnitTest(func, file, valid_test, {
        'resource_name': func.GetInfo('resource_type'),
      })
    invalid_test = """
TEST_F(%(test_name)s, %(name)sInvalidArgs) {
  EXPECT_CALL(*gl_, %(gl_func_name)s(_, _)).Times(0);
  GetSharedMemoryAs<GLuint*>()[0] = client_%(resource_name)s_id_;
  SpecializedSetup<%(name)s, 0>(false);
  %(name)s cmd;
  cmd.Init(%(args)s);
  EXPECT_EQ(error::kInvalidArguments, ExecuteCmd(cmd));
}
"""
    self.WriteValidUnitTest(func, file, invalid_test, {
          'resource_name': func.GetInfo('resource_type').lower(),
        })

  def WriteImmediateServiceUnitTest(self, func, file):
    """Overrriden from TypeHandler."""
    valid_test = """
TEST_F(%(test_name)s, %(name)sValidArgs) {
  EXPECT_CALL(*gl_, %(gl_func_name)s(1, _))
      .WillOnce(SetArgumentPointee<1>(kNewServiceId));
  %(name)s* cmd = GetImmediateAs<%(name)s>();
  GLuint temp = kNewClientId;
  SpecializedSetup<%(name)s, 0>(true);
  cmd->Init(1, &temp);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(*cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_TRUE(Get%(resource_name)sInfo(kNewClientId) != NULL);
}
"""
    self.WriteValidUnitTest(func, file, valid_test, {
        'resource_name': func.GetInfo('resource_type'),
      })
    invalid_test = """
TEST_F(%(test_name)s, %(name)sInvalidArgs) {
  EXPECT_CALL(*gl_, %(gl_func_name)s(_, _)).Times(0);
  %(name)s* cmd = GetImmediateAs<%(name)s>();
  SpecializedSetup<%(name)s, 0>(false);
  cmd->Init(1, &client_%(resource_name)s_id_);
  EXPECT_EQ(error::kInvalidArguments,
            ExecuteImmediateCmd(*cmd, sizeof(&client_%(resource_name)s_id_)));
}
"""
    self.WriteValidUnitTest(func, file, invalid_test, {
          'resource_name': func.GetInfo('resource_type').lower(),
        })

  def WriteImmediateCmdComputeSize(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  static uint32 ComputeDataSize(GLsizei n) {\n")
    file.Write(
        "    return static_cast<uint32>(sizeof(GLuint) * n);  // NOLINT\n")
    file.Write("  }\n")
    file.Write("\n")
    file.Write("  static uint32 ComputeSize(GLsizei n) {\n")
    file.Write("    return static_cast<uint32>(\n")
    file.Write("        sizeof(ValueType) + ComputeDataSize(n));  // NOLINT\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdSetHeader(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  void SetHeader(GLsizei n) {\n")
    file.Write("    header.SetCmdByTotalSize<ValueType>(ComputeSize(n));\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdInit(self, func, file):
    """Overrriden from TypeHandler."""
    last_arg = func.GetLastOriginalArg()
    file.Write("  void Init(%s, %s _%s) {\n" %
               (func.MakeTypedCmdArgString("_"),
                last_arg.type, last_arg.name))
    file.Write("    SetHeader(_n);\n")
    args = func.GetCmdArgs()
    for arg in args:
      file.Write("    %s = _%s;\n" % (arg.name, arg.name))
    file.Write("    memcpy(ImmediateDataAddress(this),\n")
    file.Write("           _%s, ComputeDataSize(_n));\n" % last_arg.name)
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdSet(self, func, file):
    """Overrriden from TypeHandler."""
    last_arg = func.GetLastOriginalArg()
    copy_args = func.MakeCmdArgString("_", False)
    file.Write("  void* Set(void* cmd%s, %s _%s) {\n" %
               (func.MakeTypedCmdArgString("_", True),
                last_arg.type, last_arg.name))
    file.Write("    static_cast<ValueType*>(cmd)->Init(%s, _%s);\n" %
               (copy_args, last_arg.name))
    file.Write("    const uint32 size = ComputeSize(_n);\n")
    file.Write("    return NextImmediateCmdAddressTotalSize<ValueType>("
               "cmd, size);\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdHelper(self, func, file):
    """Overrriden from TypeHandler."""
    code = """  void %(name)s(%(typed_args)s) {
    const uint32 size = gles2::%(name)s::ComputeSize(n);
    gles2::%(name)s* c =
        GetImmediateCmdSpaceTotalSize<gles2::%(name)s>(size);
    if (c) {
      c->Init(%(args)s);
    }
  }

"""
    file.Write(code % {
          "name": func.name,
          "typed_args": func.MakeTypedOriginalArgString(""),
          "args": func.MakeOriginalArgString(""),
        })

  def WriteImmediateFormatTest(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("TEST_F(GLES2FormatTest, %s) {\n" % func.name)
    file.Write("  static GLuint ids[] = { 12, 23, 34, };\n")
    file.Write("  %s& cmd = *GetBufferAs<%s>();\n" % (func.name, func.name))
    file.Write("  void* next_cmd = cmd.Set(\n")
    file.Write("      &cmd, static_cast<GLsizei>(arraysize(ids)), ids);\n")
    file.Write("  EXPECT_EQ(static_cast<uint32>(%s::kCmdId),\n" % func.name)
    file.Write("            cmd.header.command);\n")
    file.Write("  EXPECT_EQ(sizeof(cmd) +\n")
    file.Write("            RoundSizeToMultipleOfEntries(cmd.n * 4u),\n")
    file.Write("            cmd.header.size * 4u);\n")
    file.Write("  EXPECT_EQ(static_cast<GLsizei>(arraysize(ids)), cmd.n);\n");
    file.Write("  CheckBytesWrittenMatchesExpectedSize(\n")
    file.Write("      next_cmd, sizeof(cmd) +\n")
    file.Write("      RoundSizeToMultipleOfEntries(arraysize(ids) * 4u));\n")
    file.Write("  // TODO(gman): Check that ids were inserted;\n")
    file.Write("}\n")
    file.Write("\n")


class CreateHandler(TypeHandler):
  """Handler for glCreate___ type functions."""

  def __init__(self):
    TypeHandler.__init__(self)

  def InitFunction(self, func):
    """Overrriden from TypeHandler."""
    func.AddCmdArg(Argument("client_id", 'uint32'))

  def WriteServiceUnitTest(self, func, file):
    """Overrriden from TypeHandler."""
    valid_test = """
TEST_F(%(test_name)s, %(name)sValidArgs) {
  EXPECT_CALL(*gl_, %(gl_func_name)s(%(gl_args)s))
      .WillOnce(Return(kNewServiceId));
  SpecializedSetup<%(name)s, 0>(true);
  %(name)s cmd;
  cmd.Init(%(args)s%(comma)skNewClientId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_TRUE(Get%(resource_type)sInfo(kNewClientId) != NULL);
}
"""
    comma = ""
    if len(func.GetOriginalArgs()):
      comma =", "
    self.WriteValidUnitTest(func, file, valid_test, {
          'comma': comma,
          'resource_type': func.name[6:],
        })
    invalid_test = """
TEST_F(%(test_name)s, %(name)sInvalidArgs%(arg_index)d_%(value_index)d) {
  EXPECT_CALL(*gl_, %(gl_func_name)s(%(gl_args)s)).Times(0);
  SpecializedSetup<%(name)s, 0>(false);
  %(name)s cmd;
  cmd.Init(%(args)s%(comma)skNewClientId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));%(gl_error_test)s
}
"""
    self.WriteInvalidUnitTest(func, file, invalid_test, {
          'comma': comma,
        })

  def WriteHandlerImplementation (self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  uint32 client_id = c.client_id;\n")
    file.Write("  if (!%sHelper(%s)) {\n" %
               (func.name, func.MakeCmdArgString("")))
    file.Write("    return error::kInvalidArguments;\n")
    file.Write("  }\n")

  def WriteGLES2ImplementationHeader(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("%s %s(%s) {\n" %
               (func.return_type, func.original_name,
                func.MakeTypedOriginalArgString("")))
    file.Write("  GPU_CLIENT_SINGLE_THREAD_CHECK();\n")
    func.WriteDestinationInitalizationValidation(file)
    self.WriteClientGLCallLog(func, file)
    for arg in func.GetOriginalArgs():
      arg.WriteClientSideValidationCode(file, func)
    file.Write("  GLuint client_id;\n")
    file.Write(
        "  GetIdHandler(id_namespaces::kProgramsAndShaders)->\n")
    file.Write("      MakeIds(this, 0, 1, &client_id);\n")
    file.Write("  helper_->%s(%s);\n" %
               (func.name, func.MakeCmdArgString("")))
    file.Write('  GPU_CLIENT_LOG("returned " << client_id);\n')
    file.Write("  return client_id;\n")
    file.Write("}\n")
    file.Write("\n")


class DeleteHandler(TypeHandler):
  """Handler for glDelete___ single resource type functions."""

  def __init__(self):
    TypeHandler.__init__(self)

  def WriteServiceImplementation(self, func, file):
    """Overrriden from TypeHandler."""
    pass

  def WriteGLES2ImplementationHeader(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("%s %s(%s) {\n" %
               (func.return_type, func.original_name,
                func.MakeTypedOriginalArgString("")))
    file.Write("  GPU_CLIENT_SINGLE_THREAD_CHECK();\n")
    func.WriteDestinationInitalizationValidation(file)
    self.WriteClientGLCallLog(func, file)
    for arg in func.GetOriginalArgs():
      arg.WriteClientSideValidationCode(file, func)
    file.Write(
        "  GPU_CLIENT_DCHECK(%s != 0);\n" % func.GetOriginalArgs()[-1].name)
    file.Write("  %sHelper(%s);\n" %
               (func.original_name, func.GetOriginalArgs()[-1].name))
    file.Write("}\n")
    file.Write("\n")


class DELnHandler(TypeHandler):
  """Handler for glDelete___ type functions."""

  def __init__(self):
    TypeHandler.__init__(self)

  def WriteGetDataSizeCode(self, func, file):
    """Overrriden from TypeHandler."""
    code = """  uint32 data_size;
  if (!SafeMultiplyUint32(n, sizeof(GLuint), &data_size)) {
    return error::kOutOfBounds;
  }
"""
    file.Write(code)

  def WriteGLES2ImplementationUnitTest(self, func, file):
    """Overrriden from TypeHandler."""
    code = """
TEST_F(GLES2ImplementationTest, %(name)s) {
  GLuint ids[2] = { k%(types)sStartId, k%(types)sStartId + 1 };
  struct Cmds {
    %(name)sImmediate del;
    GLuint data[2];
  };
  Cmds expected;
  expected.del.Init(arraysize(ids), &ids[0]);
  expected.data[0] = k%(types)sStartId;
  expected.data[1] = k%(types)sStartId + 1;
  gl_->%(name)s(arraysize(ids), &ids[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}
"""
    file.Write(code % {
          'name': func.name,
          'types': func.GetInfo('resource_types'),
        })

  def WriteServiceUnitTest(self, func, file):
    """Overrriden from TypeHandler."""
    valid_test = """
TEST_F(%(test_name)s, %(name)sValidArgs) {
  EXPECT_CALL(
      *gl_,
      %(gl_func_name)s(1, Pointee(kService%(upper_resource_name)sId)))
      .Times(1);
  GetSharedMemoryAs<GLuint*>()[0] = client_%(resource_name)s_id_;
  SpecializedSetup<%(name)s, 0>(true);
  %(name)s cmd;
  cmd.Init(%(args)s);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_TRUE(
      Get%(upper_resource_name)sInfo(client_%(resource_name)s_id_) == NULL);
}
"""
    self.WriteValidUnitTest(func, file, valid_test, {
          'resource_name': func.GetInfo('resource_type').lower(),
          'upper_resource_name': func.GetInfo('resource_type'),
        })
    invalid_test = """
TEST_F(%(test_name)s, %(name)sInvalidArgs) {
  GetSharedMemoryAs<GLuint*>()[0] = kInvalidClientId;
  SpecializedSetup<%(name)s, 0>(false);
  %(name)s cmd;
  cmd.Init(%(args)s);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}
"""
    self.WriteValidUnitTest(func, file, invalid_test)

  def WriteImmediateServiceUnitTest(self, func, file):
    """Overrriden from TypeHandler."""
    valid_test = """
TEST_F(%(test_name)s, %(name)sValidArgs) {
  EXPECT_CALL(
      *gl_,
      %(gl_func_name)s(1, Pointee(kService%(upper_resource_name)sId)))
      .Times(1);
  %(name)s& cmd = *GetImmediateAs<%(name)s>();
  SpecializedSetup<%(name)s, 0>(true);
  cmd.Init(1, &client_%(resource_name)s_id_);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(client_%(resource_name)s_id_)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_TRUE(
      Get%(upper_resource_name)sInfo(client_%(resource_name)s_id_) == NULL);
}
"""
    self.WriteValidUnitTest(func, file, valid_test, {
          'resource_name': func.GetInfo('resource_type').lower(),
          'upper_resource_name': func.GetInfo('resource_type'),
        })
    invalid_test = """
TEST_F(%(test_name)s, %(name)sInvalidArgs) {
  %(name)s& cmd = *GetImmediateAs<%(name)s>();
  SpecializedSetup<%(name)s, 0>(false);
  GLuint temp = kInvalidClientId;
  cmd.Init(1, &temp);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
}
"""
    self.WriteValidUnitTest(func, file, invalid_test)

  def WriteHandlerImplementation (self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  %sHelper(n, %s);\n" %
               (func.name, func.GetLastOriginalArg().name))

  def WriteImmediateHandlerImplementation (self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  %sHelper(n, %s);\n" %
               (func.original_name, func.GetLastOriginalArg().name))

  def WriteGLES2ImplementationHeader(self, func, file):
    """Overrriden from TypeHandler."""
    impl_decl = func.GetInfo('impl_decl')
    if impl_decl == None or impl_decl == True:
      args = {
          'return_type': func.return_type,
          'name': func.original_name,
          'typed_args': func.MakeTypedOriginalArgString(""),
          'args': func.MakeOriginalArgString(""),
          'resource_type': func.GetInfo('resource_type').lower(),
          'count_name': func.GetOriginalArgs()[0].name,
        }
      file.Write("%(return_type)s %(name)s(%(typed_args)s) {\n" % args)
      file.Write("  GPU_CLIENT_SINGLE_THREAD_CHECK();\n")
      func.WriteDestinationInitalizationValidation(file)
      self.WriteClientGLCallLog(func, file)
      file.Write("""  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < n; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << %s[i]);
    }
  });
""" % func.GetOriginalArgs()[1].name)
      file.Write("""  GPU_CLIENT_DCHECK_CODE_BLOCK({
    for (GLsizei i = 0; i < n; ++i) {
      GPU_DCHECK(%s[i] != 0);
    }
  });
""" % func.GetOriginalArgs()[1].name)
      for arg in func.GetOriginalArgs():
        arg.WriteClientSideValidationCode(file, func)
      code = """  %(name)sHelper(%(args)s);
}

"""
      file.Write(code % args)

  def WriteImmediateCmdComputeSize(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  static uint32 ComputeDataSize(GLsizei n) {\n")
    file.Write(
        "    return static_cast<uint32>(sizeof(GLuint) * n);  // NOLINT\n")
    file.Write("  }\n")
    file.Write("\n")
    file.Write("  static uint32 ComputeSize(GLsizei n) {\n")
    file.Write("    return static_cast<uint32>(\n")
    file.Write("        sizeof(ValueType) + ComputeDataSize(n));  // NOLINT\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdSetHeader(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  void SetHeader(GLsizei n) {\n")
    file.Write("    header.SetCmdByTotalSize<ValueType>(ComputeSize(n));\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdInit(self, func, file):
    """Overrriden from TypeHandler."""
    last_arg = func.GetLastOriginalArg()
    file.Write("  void Init(%s, %s _%s) {\n" %
               (func.MakeTypedCmdArgString("_"),
                last_arg.type, last_arg.name))
    file.Write("    SetHeader(_n);\n")
    args = func.GetCmdArgs()
    for arg in args:
      file.Write("    %s = _%s;\n" % (arg.name, arg.name))
    file.Write("    memcpy(ImmediateDataAddress(this),\n")
    file.Write("           _%s, ComputeDataSize(_n));\n" % last_arg.name)
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdSet(self, func, file):
    """Overrriden from TypeHandler."""
    last_arg = func.GetLastOriginalArg()
    copy_args = func.MakeCmdArgString("_", False)
    file.Write("  void* Set(void* cmd%s, %s _%s) {\n" %
               (func.MakeTypedCmdArgString("_", True),
                last_arg.type, last_arg.name))
    file.Write("    static_cast<ValueType*>(cmd)->Init(%s, _%s);\n" %
               (copy_args, last_arg.name))
    file.Write("    const uint32 size = ComputeSize(_n);\n")
    file.Write("    return NextImmediateCmdAddressTotalSize<ValueType>("
               "cmd, size);\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdHelper(self, func, file):
    """Overrriden from TypeHandler."""
    code = """  void %(name)s(%(typed_args)s) {
    const uint32 size = gles2::%(name)s::ComputeSize(n);
    gles2::%(name)s* c =
        GetImmediateCmdSpaceTotalSize<gles2::%(name)s>(size);
    if (c) {
      c->Init(%(args)s);
    }
  }

"""
    file.Write(code % {
          "name": func.name,
          "typed_args": func.MakeTypedOriginalArgString(""),
          "args": func.MakeOriginalArgString(""),
        })

  def WriteImmediateFormatTest(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("TEST_F(GLES2FormatTest, %s) {\n" % func.name)
    file.Write("  static GLuint ids[] = { 12, 23, 34, };\n")
    file.Write("  %s& cmd = *GetBufferAs<%s>();\n" % (func.name, func.name))
    file.Write("  void* next_cmd = cmd.Set(\n")
    file.Write("      &cmd, static_cast<GLsizei>(arraysize(ids)), ids);\n")
    file.Write("  EXPECT_EQ(static_cast<uint32>(%s::kCmdId),\n" % func.name)
    file.Write("            cmd.header.command);\n")
    file.Write("  EXPECT_EQ(sizeof(cmd) +\n")
    file.Write("            RoundSizeToMultipleOfEntries(cmd.n * 4u),\n")
    file.Write("            cmd.header.size * 4u);\n")
    file.Write("  EXPECT_EQ(static_cast<GLsizei>(arraysize(ids)), cmd.n);\n");
    file.Write("  CheckBytesWrittenMatchesExpectedSize(\n")
    file.Write("      next_cmd, sizeof(cmd) +\n")
    file.Write("      RoundSizeToMultipleOfEntries(arraysize(ids) * 4u));\n")
    file.Write("  // TODO(gman): Check that ids were inserted;\n")
    file.Write("}\n")
    file.Write("\n")


class GETnHandler(TypeHandler):
  """Handler for GETn for glGetBooleanv, glGetFloatv, ... type functions."""

  def __init__(self):
    TypeHandler.__init__(self)

  def AddImmediateFunction(self, generator, func):
    """Overrriden from TypeHandler."""
    pass

  def WriteServiceImplementation(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write(
        "error::Error GLES2DecoderImpl::Handle%s(\n" % func.name)
    file.Write(
        "    uint32 immediate_data_size, const gles2::%s& c) {\n" % func.name)
    last_arg = func.GetLastOriginalArg()

    all_but_last_args = func.GetOriginalArgs()[:-1]
    for arg in all_but_last_args:
      arg.WriteGetCode(file)

    code = """  typedef %(func_name)s::Result Result;
  GLsizei num_values = 0;
  GetNumValuesReturnedForGLGet(pname, &num_values);
  Result* result = GetSharedMemoryAs<Result*>(
      c.params_shm_id, c.params_shm_offset, Result::ComputeSize(num_values));
  %(last_arg_type)s params = result ? result->GetData() : NULL;
"""
    file.Write(code % {
        'last_arg_type': last_arg.type,
        'func_name': func.name,
      })
    func.WriteHandlerValidation(file)
    code = """  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }
  CopyRealGLErrorsToWrapper();
"""
    file.Write(code)
    func.WriteHandlerImplementation(file)
    code = """  GLenum error = glGetError();
  if (error == GL_NO_ERROR) {
    result->SetNumResults(num_values);
  } else {
    SetGLError(error, NULL);
  }
  return error::kNoError;
}

"""
    file.Write(code)

  def WriteGLES2ImplementationHeader(self, func, file):
    """Overrriden from TypeHandler."""
    impl_decl = func.GetInfo('impl_decl')
    if impl_decl == None or impl_decl == True:
      file.Write("%s %s(%s) {\n" %
                 (func.return_type, func.original_name,
                  func.MakeTypedOriginalArgString("")))
      file.Write("  GPU_CLIENT_SINGLE_THREAD_CHECK();\n")
      func.WriteDestinationInitalizationValidation(file)
      self.WriteClientGLCallLog(func, file)
      for arg in func.GetOriginalArgs():
        arg.WriteClientSideValidationCode(file, func)
      all_but_last_args = func.GetOriginalArgs()[:-1]
      arg_string = (
          ", ".join(["%s" % arg.name for arg in all_but_last_args]))
      all_arg_string = (
          ", ".join(["%s" % arg.name for arg in func.GetOriginalArgs()]))
      code = """  if (%(func_name)sHelper(%(all_arg_string)s)) {
    return;
  }
  typedef %(func_name)s::Result Result;
  Result* result = GetResultAs<Result*>();
  if (!result) {
    return;
  }
  result->SetNumResults(0);
  helper_->%(func_name)s(%(arg_string)s,
      GetResultShmId(), GetResultShmOffset());
  WaitForCmd();
  result->CopyResult(params);
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (int32 i = 0; i < result->GetNumResults(); ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << result->GetData()[i]);
    }
  });
}
"""
      file.Write(code % {
          'func_name': func.name,
          'arg_string': arg_string,
          'all_arg_string': all_arg_string,
        })

  def WriteGLES2ImplementationUnitTest(self, func, file):
    """Writes the GLES2 Implemention unit test."""
    code = """
TEST_F(GLES2ImplementationTest, %(name)s) {
  struct Cmds {
    %(name)s cmd;
  };
  typedef %(name)s::Result Result;
  Result::Type result = 0;
  Cmds expected;
  ExpectedMemoryInfo result1 = GetExpectedResultMemory(4);
  expected.cmd.Init(%(cmd_args)s, result1.id, result1.offset);
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, SizedResultHelper<Result::Type>(1)))
      .RetiresOnSaturation();
  gl_->%(name)s(%(args)s, &result);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(static_cast<Result::Type>(1), result);
}
"""
    cmd_arg_strings = []
    count = 0
    for arg in func.GetCmdArgs()[0:-2]:
      cmd_arg_strings.append(arg.GetValidClientSideCmdArg(func, count, 0))
      count += 1
    cmd_arg_strings[0] = '123'
    gl_arg_strings = []
    count = 0
    for arg in func.GetOriginalArgs()[0:-1]:
      gl_arg_strings.append(arg.GetValidClientSideArg(func, count, 0))
      count += 1
    gl_arg_strings[0] = '123'
    file.Write(code % {
          'name': func.name,
          'args': ", ".join(gl_arg_strings),
          'cmd_args': ", ".join(cmd_arg_strings),
        })

  def WriteServiceUnitTest(self, func, file):
    """Overrriden from TypeHandler."""
    valid_test = """
TEST_F(%(test_name)s, %(name)sValidArgs) {
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  SpecializedSetup<%(name)s, 0>(true);
  typedef %(name)s::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  EXPECT_CALL(*gl_, %(gl_func_name)s(%(local_gl_args)s));
  result->size = 0;
  %(name)s cmd;
  cmd.Init(%(args)s);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(decoder_->GetGLES2Util()->GLGetNumValuesReturned(
                %(valid_pname)s),
            result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}
"""
    gl_arg_strings = []
    valid_pname = ''
    count = 0
    for arg in func.GetOriginalArgs()[:-1]:
      arg_value = arg.GetValidGLArg(func, count, 0)
      gl_arg_strings.append(arg_value)
      if arg.name == 'pname':
        valid_pname = arg_value
      count += 1
    if func.GetInfo('gl_test_func') == 'glGetIntegerv':
      gl_arg_strings.append("_")
    else:
      gl_arg_strings.append("result->GetData()")

    self.WriteValidUnitTest(func, file, valid_test, {
        'local_gl_args': ", ".join(gl_arg_strings),
        'valid_pname': valid_pname,
      })

    invalid_test = """
TEST_F(%(test_name)s, %(name)sInvalidArgs%(arg_index)d_%(value_index)d) {
  EXPECT_CALL(*gl_, %(gl_func_name)s(%(gl_args)s)).Times(0);
  SpecializedSetup<%(name)s, 0>(false);
  %(name)s::Result* result =
      static_cast<%(name)s::Result*>(shared_memory_address_);
  result->size = 0;
  %(name)s cmd;
  cmd.Init(%(args)s);
  EXPECT_EQ(error::%(parse_result)s, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);%(gl_error_test)s
}
"""
    self.WriteInvalidUnitTest(func, file, invalid_test)


class PUTHandler(TypeHandler):
  """Handler for glTexParameter_v, glVertexAttrib_v functions."""

  def __init__(self):
    TypeHandler.__init__(self)

  def WriteServiceUnitTest(self, func, file):
    """Writes the service unit test for a command."""
    valid_test = """
TEST_F(%(test_name)s, %(name)sValidArgs) {
  EXPECT_CALL(*gl_, %(gl_func_name)s(%(gl_args)s));
  SpecializedSetup<%(name)s, 0>(true);
  %(name)s cmd;
  cmd.Init(%(args)s);
  GetSharedMemoryAs<%(data_type)s*>()[0] = %(data_value)s;
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}
"""
    extra = {
      'data_type': func.GetInfo('data_type'),
      'data_value': func.GetInfo('data_value') or '0',
    }
    self.WriteValidUnitTest(func, file, valid_test, extra)

    invalid_test = """
TEST_F(%(test_name)s, %(name)sInvalidArgs%(arg_index)d_%(value_index)d) {
  EXPECT_CALL(*gl_, %(gl_func_name)s(%(gl_args)s)).Times(0);
  SpecializedSetup<%(name)s, 0>(false);
  %(name)s cmd;
  cmd.Init(%(args)s);
  GetSharedMemoryAs<%(data_type)s*>()[0] = %(data_value)s;
  EXPECT_EQ(error::%(parse_result)s, ExecuteCmd(cmd));%(gl_error_test)s
}
"""
    self.WriteInvalidUnitTest(func, file, invalid_test, extra)

  def WriteImmediateServiceUnitTest(self, func, file):
    """Writes the service unit test for a command."""
    valid_test = """
TEST_F(%(test_name)s, %(name)sValidArgs) {
  %(name)s& cmd = *GetImmediateAs<%(name)s>();
  EXPECT_CALL(
      *gl_,
      %(gl_func_name)s(%(gl_args)s,
          reinterpret_cast<%(data_type)s*>(ImmediateDataAddress(&cmd))));
  SpecializedSetup<%(name)s, 0>(true);
  %(data_type)s temp[%(data_count)s] = { %(data_value)s, };
  cmd.Init(%(gl_args)s, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}
"""
    gl_arg_strings = []
    gl_any_strings = []
    count = 0
    for arg in func.GetOriginalArgs()[0:-1]:
      gl_arg_strings.append(arg.GetValidGLArg(func, count, 0))
      gl_any_strings.append("_")
      count += 1
    extra = {
      'data_type': func.GetInfo('data_type'),
      'data_count': func.GetInfo('count'),
      'data_value': func.GetInfo('data_value') or '0',
      'gl_args': ", ".join(gl_arg_strings),
      'gl_any_args': ", ".join(gl_any_strings),
    }
    self.WriteValidUnitTest(func, file, valid_test, extra)

    invalid_test = """
TEST_F(%(test_name)s, %(name)sInvalidArgs%(arg_index)d_%(value_index)d) {
  %(name)s& cmd = *GetImmediateAs<%(name)s>();
  EXPECT_CALL(*gl_, %(gl_func_name)s(%(gl_any_args)s, _)).Times(0);
  SpecializedSetup<%(name)s, 0>(false);
  %(data_type)s temp[%(data_count)s] = { %(data_value)s, };
  cmd.Init(%(all_but_last_args)s, &temp[0]);
  EXPECT_EQ(error::%(parse_result)s,
            ExecuteImmediateCmd(cmd, sizeof(temp)));%(gl_error_test)s
}
"""
    self.WriteInvalidUnitTest(func, file, invalid_test, extra)

  def WriteGetDataSizeCode(self, func, file):
    """Overrriden from TypeHandler."""
    code = """  uint32 data_size;
  if (!ComputeDataSize(1, sizeof(%s), %d, &data_size)) {
    return error::kOutOfBounds;
  }
"""
    file.Write(code % (func.info.data_type, func.info.count))
    if func.is_immediate:
      file.Write("  if (data_size > immediate_data_size) {\n")
      file.Write("    return error::kOutOfBounds;\n")
      file.Write("  }\n")

  def WriteGLES2ImplementationHeader(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("%s %s(%s) {\n" %
               (func.return_type, func.original_name,
                func.MakeTypedOriginalArgString("")))
    file.Write("  GPU_CLIENT_SINGLE_THREAD_CHECK();\n")
    func.WriteDestinationInitalizationValidation(file)
    self.WriteClientGLCallLog(func, file)
    last_arg_name = func.GetLastOriginalArg().name
    values_str = ' << ", " << '.join(
        ["%s[%d]" % (last_arg_name, ndx) for ndx in range(0, func.info.count)])
    file.Write('  GPU_CLIENT_LOG("values: " << %s);\n' % values_str)
    for arg in func.GetOriginalArgs():
      arg.WriteClientSideValidationCode(file, func)
    file.Write("  helper_->%sImmediate(%s);\n" %
               (func.name, func.MakeOriginalArgString("")))
    file.Write("}\n")
    file.Write("\n")

  def WriteGLES2ImplementationUnitTest(self, func, file):
    """Writes the GLES2 Implemention unit test."""
    code = """
TEST_F(GLES2ImplementationTest, %(name)s) {
  struct Cmds {
    %(name)sImmediate cmd;
    %(type)s data[%(count)d];
  };

  Cmds expected;
  for (int jj = 0; jj < %(count)d; ++jj) {
    expected.data[jj] = static_cast<%(type)s>(jj);
  }
  expected.cmd.Init(%(cmd_args)s, &expected.data[0]);
  gl_->%(name)s(%(args)s, &expected.data[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}
"""
    cmd_arg_strings = []
    count = 0
    for arg in func.GetCmdArgs()[0:-2]:
      cmd_arg_strings.append(arg.GetValidClientSideCmdArg(func, count, 0))
      count += 1
    gl_arg_strings = []
    count = 0
    for arg in func.GetOriginalArgs()[0:-1]:
      gl_arg_strings.append(arg.GetValidClientSideArg(func, count, 0))
      count += 1
    file.Write(code % {
          'name': func.name,
          'type': func.GetInfo('data_type'),
          'count': func.GetInfo('count'),
          'args': ", ".join(gl_arg_strings),
          'cmd_args': ", ".join(cmd_arg_strings),
        })

  def WriteImmediateCmdComputeSize(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  static uint32 ComputeDataSize() {\n")
    file.Write("    return static_cast<uint32>(\n")
    file.Write("        sizeof(%s) * %d);  // NOLINT\n" %
               (func.info.data_type, func.info.count))
    file.Write("  }\n")
    file.Write("\n")
    file.Write("  static uint32 ComputeSize() {\n")
    file.Write("    return static_cast<uint32>(\n")
    file.Write(
        "        sizeof(ValueType) + ComputeDataSize());  // NOLINT\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdSetHeader(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  void SetHeader() {\n")
    file.Write(
        "    header.SetCmdByTotalSize<ValueType>(ComputeSize());\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdInit(self, func, file):
    """Overrriden from TypeHandler."""
    last_arg = func.GetLastOriginalArg()
    file.Write("  void Init(%s, %s _%s) {\n" %
               (func.MakeTypedCmdArgString("_"),
                last_arg.type, last_arg.name))
    file.Write("    SetHeader();\n")
    args = func.GetCmdArgs()
    for arg in args:
      file.Write("    %s = _%s;\n" % (arg.name, arg.name))
    file.Write("    memcpy(ImmediateDataAddress(this),\n")
    file.Write("           _%s, ComputeDataSize());\n" % last_arg.name)
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdSet(self, func, file):
    """Overrriden from TypeHandler."""
    last_arg = func.GetLastOriginalArg()
    copy_args = func.MakeCmdArgString("_", False)
    file.Write("  void* Set(void* cmd%s, %s _%s) {\n" %
               (func.MakeTypedCmdArgString("_", True),
                last_arg.type, last_arg.name))
    file.Write("    static_cast<ValueType*>(cmd)->Init(%s, _%s);\n" %
               (copy_args, last_arg.name))
    file.Write("    const uint32 size = ComputeSize();\n")
    file.Write("    return NextImmediateCmdAddressTotalSize<ValueType>("
               "cmd, size);\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdHelper(self, func, file):
    """Overrriden from TypeHandler."""
    code = """  void %(name)s(%(typed_args)s) {
    const uint32 size = gles2::%(name)s::ComputeSize();
    gles2::%(name)s* c =
        GetImmediateCmdSpaceTotalSize<gles2::%(name)s>(size);
    if (c) {
      c->Init(%(args)s);
    }
  }

"""
    file.Write(code % {
          "name": func.name,
          "typed_args": func.MakeTypedOriginalArgString(""),
          "args": func.MakeOriginalArgString(""),
        })

  def WriteImmediateFormatTest(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("TEST_F(GLES2FormatTest, %s) {\n" % func.name)
    file.Write("  const int kSomeBaseValueToTestWith = 51;\n")
    file.Write("  static %s data[] = {\n" % func.info.data_type)
    for v in range(0, func.info.count):
      file.Write("    static_cast<%s>(kSomeBaseValueToTestWith + %d),\n" %
                 (func.info.data_type, v))
    file.Write("  };\n")
    file.Write("  %s& cmd = *GetBufferAs<%s>();\n" % (func.name, func.name))
    file.Write("  void* next_cmd = cmd.Set(\n")
    file.Write("      &cmd")
    args = func.GetCmdArgs()
    value = 11
    for arg in args:
      file.Write(",\n      static_cast<%s>(%d)" % (arg.type, value))
      value += 1
    file.Write(",\n      data);\n")
    args = func.GetCmdArgs()
    value = 11
    file.Write("  EXPECT_EQ(static_cast<uint32>(%s::kCmdId),\n" % func.name)
    file.Write("            cmd.header.command);\n")
    file.Write("  EXPECT_EQ(sizeof(cmd) +\n")
    file.Write("            RoundSizeToMultipleOfEntries(sizeof(data)),\n")
    file.Write("            cmd.header.size * 4u);\n")
    for arg in args:
      file.Write("  EXPECT_EQ(static_cast<%s>(%d), cmd.%s);\n" %
                 (arg.type, value, arg.name))
      value += 1
    file.Write("  CheckBytesWrittenMatchesExpectedSize(\n")
    file.Write("      next_cmd, sizeof(cmd) +\n")
    file.Write("      RoundSizeToMultipleOfEntries(sizeof(data)));\n")
    file.Write("  // TODO(gman): Check that data was inserted;\n")
    file.Write("}\n")
    file.Write("\n")


class PUTnHandler(TypeHandler):
  """Handler for PUTn 'glUniform__v' type functions."""

  def __init__(self):
    TypeHandler.__init__(self)

  def WriteServiceUnitTest(self, func, file):
    """Overridden from TypeHandler."""
    TypeHandler.WriteServiceUnitTest(self, func, file)

    valid_test = """
TEST_F(%(test_name)s, %(name)sValidArgsCountTooLarge) {
  EXPECT_CALL(*gl_, %(gl_func_name)s(%(gl_args)s));
  SpecializedSetup<%(name)s, 0>(true);
  %(name)s cmd;
  cmd.Init(%(args)s);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}
"""
    gl_arg_strings = []
    arg_strings = []
    count = 0
    for arg in func.GetOriginalArgs():
      # hardcoded to match unit tests.
      if count == 0:
        # the location of the second element of the 2nd uniform.
        # defined in GLES2DecoderBase::SetupShaderForUniform
        gl_arg_strings.append("3")
        arg_strings.append(
            "program_manager()->SwizzleLocation(ProgramManager::"
            "ProgramInfo::GetFakeLocation(1, 1))")
      elif count == 1:
        # the number of elements that gl will be called with.
        gl_arg_strings.append("3")
        # the number of elements requested in the command.
        arg_strings.append("5")
      else:
        gl_arg_strings.append(arg.GetValidGLArg(func, count, 0))
        arg_strings.append(arg.GetValidArg(func, count, 0))
      count += 1
    extra = {
      'gl_args': ", ".join(gl_arg_strings),
      'args': ", ".join(arg_strings),
    }
    self.WriteValidUnitTest(func, file, valid_test, extra)

  def WriteImmediateServiceUnitTest(self, func, file):
    """Overridden from TypeHandler."""
    valid_test = """
TEST_F(%(test_name)s, %(name)sValidArgs) {
  %(name)s& cmd = *GetImmediateAs<%(name)s>();
  EXPECT_CALL(
      *gl_,
      %(gl_func_name)s(%(gl_args)s,
          reinterpret_cast<%(data_type)s*>(ImmediateDataAddress(&cmd))));
  SpecializedSetup<%(name)s, 0>(true);
  %(data_type)s temp[%(data_count)s * 2] = { 0, };
  cmd.Init(%(args)s, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}
"""
    gl_arg_strings = []
    gl_any_strings = []
    arg_strings = []
    count = 0
    for arg in func.GetOriginalArgs()[0:-1]:
      gl_arg_strings.append(arg.GetValidGLArg(func, count, 0))
      gl_any_strings.append("_")
      arg_strings.append(arg.GetValidArg(func, count, 0))
      count += 1
    extra = {
      'data_type': func.GetInfo('data_type'),
      'data_count': func.GetInfo('count'),
      'args': ", ".join(arg_strings),
      'gl_args': ", ".join(gl_arg_strings),
      'gl_any_args': ", ".join(gl_any_strings),
    }
    self.WriteValidUnitTest(func, file, valid_test, extra)

    invalid_test = """
TEST_F(%(test_name)s, %(name)sInvalidArgs%(arg_index)d_%(value_index)d) {
  %(name)s& cmd = *GetImmediateAs<%(name)s>();
  EXPECT_CALL(*gl_, %(gl_func_name)s(%(gl_any_args)s, _)).Times(0);
  SpecializedSetup<%(name)s, 0>(false);
  %(data_type)s temp[%(data_count)s * 2] = { 0, };
  cmd.Init(%(all_but_last_args)s, &temp[0]);
  EXPECT_EQ(error::%(parse_result)s,
            ExecuteImmediateCmd(cmd, sizeof(temp)));%(gl_error_test)s
}
"""
    self.WriteInvalidUnitTest(func, file, invalid_test, extra)

  def WriteGetDataSizeCode(self, func, file):
    """Overrriden from TypeHandler."""
    code = """  uint32 data_size;
  if (!ComputeDataSize(count, sizeof(%s), %d, &data_size)) {
    return error::kOutOfBounds;
  }
"""
    file.Write(code % (func.info.data_type, func.info.count))
    if func.is_immediate:
      file.Write("  if (data_size > immediate_data_size) {\n")
      file.Write("    return error::kOutOfBounds;\n")
      file.Write("  }\n")

  def WriteGLES2ImplementationHeader(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("%s %s(%s) {\n" %
               (func.return_type, func.original_name,
                func.MakeTypedOriginalArgString("")))
    file.Write("  GPU_CLIENT_SINGLE_THREAD_CHECK();\n")
    func.WriteDestinationInitalizationValidation(file)
    self.WriteClientGLCallLog(func, file)
    last_arg_name = func.GetLastOriginalArg().name
    file.Write("""  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < count; ++i) {
""")
    values_str = ' << ", " << '.join(
        ["%s[%d + i * %d]" % (
            last_arg_name, ndx, func.info.count) for ndx in range(
                0, func.info.count)])
    file.Write('       GPU_CLIENT_LOG("  " << i << ": " << %s);\n' % values_str)
    file.Write("    }\n  });\n")
    for arg in func.GetOriginalArgs():
      arg.WriteClientSideValidationCode(file, func)
    file.Write("  helper_->%sImmediate(%s);\n" %
               (func.name, func.MakeOriginalArgString("")))
    file.Write("}\n")
    file.Write("\n")

  def WriteGLES2ImplementationUnitTest(self, func, file):
    """Writes the GLES2 Implemention unit test."""
    code = """
TEST_F(GLES2ImplementationTest, %(name)s) {
  struct Cmds {
    %(name)sImmediate cmd;
    %(type)s data[2][%(count)d];
  };

  Cmds expected;
  for (int ii = 0; ii < 2; ++ii) {
    for (int jj = 0; jj < %(count)d; ++jj) {
      expected.data[ii][jj] = static_cast<%(type)s>(ii * %(count)d + jj);
    }
  }
  expected.cmd.Init(%(cmd_args)s, &expected.data[0][0]);
  gl_->%(name)s(%(args)s, &expected.data[0][0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}
"""
    cmd_arg_strings = []
    count = 0
    for arg in func.GetCmdArgs()[0:-2]:
      cmd_arg_strings.append(arg.GetValidClientSideCmdArg(func, count, 0))
      count += 1
    gl_arg_strings = []
    count = 0
    for arg in func.GetOriginalArgs()[0:-1]:
      gl_arg_strings.append(arg.GetValidClientSideArg(func, count, 0))
      count += 1
    file.Write(code % {
          'name': func.name,
          'type': func.GetInfo('data_type'),
          'count': func.GetInfo('count'),
          'args': ", ".join(gl_arg_strings),
          'cmd_args': ", ".join(cmd_arg_strings),
        })

  def WriteImmediateCmdComputeSize(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  static uint32 ComputeDataSize(GLsizei count) {\n")
    file.Write("    return static_cast<uint32>(\n")
    file.Write("        sizeof(%s) * %d * count);  // NOLINT\n" %
               (func.info.data_type, func.info.count))
    file.Write("  }\n")
    file.Write("\n")
    file.Write("  static uint32 ComputeSize(GLsizei count) {\n")
    file.Write("    return static_cast<uint32>(\n")
    file.Write(
        "        sizeof(ValueType) + ComputeDataSize(count));  // NOLINT\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdSetHeader(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  void SetHeader(GLsizei count) {\n")
    file.Write(
        "    header.SetCmdByTotalSize<ValueType>(ComputeSize(count));\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdInit(self, func, file):
    """Overrriden from TypeHandler."""
    last_arg = func.GetLastOriginalArg()
    file.Write("  void Init(%s, %s _%s) {\n" %
               (func.MakeTypedCmdArgString("_"),
                last_arg.type, last_arg.name))
    file.Write("    SetHeader(_count);\n")
    args = func.GetCmdArgs()
    for arg in args:
      file.Write("    %s = _%s;\n" % (arg.name, arg.name))
    file.Write("    memcpy(ImmediateDataAddress(this),\n")
    file.Write("           _%s, ComputeDataSize(_count));\n" % last_arg.name)
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdSet(self, func, file):
    """Overrriden from TypeHandler."""
    last_arg = func.GetLastOriginalArg()
    copy_args = func.MakeCmdArgString("_", False)
    file.Write("  void* Set(void* cmd%s, %s _%s) {\n" %
               (func.MakeTypedCmdArgString("_", True),
                last_arg.type, last_arg.name))
    file.Write("    static_cast<ValueType*>(cmd)->Init(%s, _%s);\n" %
               (copy_args, last_arg.name))
    file.Write("    const uint32 size = ComputeSize(_count);\n")
    file.Write("    return NextImmediateCmdAddressTotalSize<ValueType>("
               "cmd, size);\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdHelper(self, func, file):
    """Overrriden from TypeHandler."""
    code = """  void %(name)s(%(typed_args)s) {
    const uint32 size = gles2::%(name)s::ComputeSize(count);
    gles2::%(name)s* c =
        GetImmediateCmdSpaceTotalSize<gles2::%(name)s>(size);
    if (c) {
      c->Init(%(args)s);
    }
  }

"""
    file.Write(code % {
          "name": func.name,
          "typed_args": func.MakeTypedOriginalArgString(""),
          "args": func.MakeOriginalArgString(""),
        })

  def WriteImmediateFormatTest(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("TEST_F(GLES2FormatTest, %s) {\n" % func.name)
    file.Write("  const int kSomeBaseValueToTestWith = 51;\n")
    file.Write("  static %s data[] = {\n" % func.info.data_type)
    for v in range(0, func.info.count * 2):
      file.Write("    static_cast<%s>(kSomeBaseValueToTestWith + %d),\n" %
                 (func.info.data_type, v))
    file.Write("  };\n")
    file.Write("  %s& cmd = *GetBufferAs<%s>();\n" % (func.name, func.name))
    file.Write("  const GLsizei kNumElements = 2;\n")
    file.Write("  const size_t kExpectedCmdSize =\n")
    file.Write("      sizeof(cmd) + kNumElements * sizeof(%s) * %d;\n" %
               (func.info.data_type, func.info.count))
    file.Write("  void* next_cmd = cmd.Set(\n")
    file.Write("      &cmd")
    args = func.GetCmdArgs()
    value = 1
    for arg in args:
      file.Write(",\n      static_cast<%s>(%d)" % (arg.type, value))
      value += 1
    file.Write(",\n      data);\n")
    args = func.GetCmdArgs()
    value = 1
    file.Write("  EXPECT_EQ(static_cast<uint32>(%s::kCmdId),\n" % func.name)
    file.Write("            cmd.header.command);\n")
    file.Write("  EXPECT_EQ(kExpectedCmdSize, cmd.header.size * 4u);\n")
    for arg in args:
      file.Write("  EXPECT_EQ(static_cast<%s>(%d), cmd.%s);\n" %
                 (arg.type, value, arg.name))
      value += 1
    file.Write("  CheckBytesWrittenMatchesExpectedSize(\n")
    file.Write("      next_cmd, sizeof(cmd) +\n")
    file.Write("      RoundSizeToMultipleOfEntries(sizeof(data)));\n")
    file.Write("  // TODO(gman): Check that data was inserted;\n")
    file.Write("}\n")
    file.Write("\n")


class PUTXnHandler(TypeHandler):
  """Handler for glUniform?f functions."""
  def __init__(self):
    TypeHandler.__init__(self)

  def WriteHandlerImplementation(self, func, file):
    """Overrriden from TypeHandler."""
    code = """  %(type)s temp[%(count)s] = { %(values)s};
  Do%(name)sv(%(location)s, 1, &temp[0]);
"""
    values = ""
    args = func.GetOriginalArgs()
    count = int(func.GetInfo('count'))
    num_args = len(args)
    for ii in range(count):
      values += "%s, " % args[len(args) - count + ii].name

    file.Write(code % {
        'name': func.name,
        'count': func.GetInfo('count'),
        'type': func.GetInfo('data_type'),
        'location': args[0].name,
        'args': func.MakeOriginalArgString(""),
        'values': values,
      })

  def WriteServiceUnitTest(self, func, file):
    """Overrriden from TypeHandler."""
    valid_test = """
TEST_F(%(test_name)s, %(name)sValidArgs) {
  EXPECT_CALL(*gl_, %(name)sv(%(local_args)s));
  SpecializedSetup<%(name)s, 0>(true);
  %(name)s cmd;
  cmd.Init(%(args)s);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}
"""
    args = func.GetOriginalArgs()
    local_args = "%s, 1, _" % args[0].GetValidGLArg(func, 0, 0)
    self.WriteValidUnitTest(func, file, valid_test, {
        'name': func.name,
        'count': func.GetInfo('count'),
        'local_args': local_args,
      })

    invalid_test = """
TEST_F(%(test_name)s, %(name)sInvalidArgs%(arg_index)d_%(value_index)d) {
  EXPECT_CALL(*gl_, %(name)sv(_, _, _).Times(0);
  SpecializedSetup<%(name)s, 0>(false);
  %(name)s cmd;
  cmd.Init(%(args)s);
  EXPECT_EQ(error::%(parse_result)s, ExecuteCmd(cmd));%(gl_error_test)s
}
"""
    self.WriteInvalidUnitTest(func, file, invalid_test, {
        'name': func.GetInfo('name'),
        'count': func.GetInfo('count'),
      })


class GLcharHandler(CustomHandler):
  """Handler for functions that pass a single string ."""

  def __init__(self):
    CustomHandler.__init__(self)

  def WriteImmediateCmdComputeSize(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  static uint32 ComputeSize(uint32 data_size) {\n")
    file.Write("    return static_cast<uint32>(\n")
    file.Write("        sizeof(ValueType) + data_size);  // NOLINT\n")
    file.Write("  }\n")

  def WriteImmediateCmdSetHeader(self, func, file):
    """Overrriden from TypeHandler."""
    code = """
  void SetHeader(uint32 data_size) {
    header.SetCmdBySize<ValueType>(data_size);
  }
"""
    file.Write(code)

  def WriteImmediateCmdInit(self, func, file):
    """Overrriden from TypeHandler."""
    last_arg = func.GetLastOriginalArg()
    args = func.GetCmdArgs()
    set_code = []
    for arg in args:
      set_code.append("    %s = _%s;" % (arg.name, arg.name))
    code = """
  void Init(%(typed_args)s, uint32 _data_size) {
    SetHeader(_data_size);
%(set_code)s
    memcpy(ImmediateDataAddress(this), _%(last_arg)s, _data_size);
  }

"""
    file.Write(code % {
          "typed_args": func.MakeTypedOriginalArgString("_"),
          "set_code": "\n".join(set_code),
          "last_arg": last_arg.name
        })

  def WriteImmediateCmdSet(self, func, file):
    """Overrriden from TypeHandler."""
    last_arg = func.GetLastOriginalArg()
    file.Write("  void* Set(void* cmd%s, uint32 _data_size) {\n" %
               func.MakeTypedOriginalArgString("_", True))
    file.Write("    static_cast<ValueType*>(cmd)->Init(%s, _data_size);\n" %
               func.MakeOriginalArgString("_"))
    file.Write("    return NextImmediateCmdAddress<ValueType>("
               "cmd, _data_size);\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdHelper(self, func, file):
    """Overrriden from TypeHandler."""
    code = """  void %(name)s(%(typed_args)s) {
    const uint32 data_size = strlen(name);
    gles2::%(name)s* c = GetImmediateCmdSpace<gles2::%(name)s>(data_size);
    if (c) {
      c->Init(%(args)s, data_size);
    }
  }

"""
    file.Write(code % {
          "name": func.name,
          "typed_args": func.MakeTypedOriginalArgString(""),
          "args": func.MakeOriginalArgString(""),
        })


  def WriteImmediateFormatTest(self, func, file):
    """Overrriden from TypeHandler."""
    init_code = []
    check_code = []
    all_but_last_arg = func.GetCmdArgs()[:-1]
    value = 11
    for arg in all_but_last_arg:
      init_code.append("      static_cast<%s>(%d)," % (arg.type, value))
      value += 1
    value = 11
    for arg in all_but_last_arg:
      check_code.append("  EXPECT_EQ(static_cast<%s>(%d), cmd.%s);" %
                        (arg.type, value, arg.name))
      value += 1
    code = """
TEST_F(GLES2FormatTest, %(func_name)s) {
  %(func_name)s& cmd = *GetBufferAs<%(func_name)s>();
  static const char* const test_str = \"test string\";
  void* next_cmd = cmd.Set(
      &cmd,
%(init_code)s
      test_str,
      strlen(test_str));
  EXPECT_EQ(static_cast<uint32>(%(func_name)s::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(strlen(test_str)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd) +
                RoundSizeToMultipleOfEntries(strlen(test_str)));
%(check_code)s
  EXPECT_EQ(static_cast<uint32>(strlen(test_str)), cmd.data_size);
  EXPECT_EQ(0, memcmp(test_str, ImmediateDataAddress(&cmd), strlen(test_str)));
  CheckBytesWritten(
      next_cmd,
      sizeof(cmd) + RoundSizeToMultipleOfEntries(strlen(test_str)),
      sizeof(cmd) + strlen(test_str));
}

"""
    file.Write(code % {
          'func_name': func.name,
          'init_code': "\n".join(init_code),
          'check_code': "\n".join(check_code),
        })


class IsHandler(TypeHandler):
  """Handler for glIs____ type and glGetError functions."""

  def __init__(self):
    TypeHandler.__init__(self)

  def InitFunction(self, func):
    """Overrriden from TypeHandler."""
    func.AddCmdArg(Argument("result_shm_id", 'uint32'))
    func.AddCmdArg(Argument("result_shm_offset", 'uint32'))
    if func.GetInfo('result') == None:
      func.AddInfo('result', ['uint32'])

  def WriteServiceUnitTest(self, func, file):
    """Overrriden from TypeHandler."""
    valid_test = """
TEST_F(%(test_name)s, %(name)sValidArgs) {
  EXPECT_CALL(*gl_, %(gl_func_name)s(%(gl_args)s));
  SpecializedSetup<%(name)s, 0>(true);
  %(name)s cmd;
  cmd.Init(%(args)s%(comma)sshared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}
"""
    comma = ""
    if len(func.GetOriginalArgs()):
      comma =", "
    self.WriteValidUnitTest(func, file, valid_test, {
          'comma': comma,
        })

    invalid_test = """
TEST_F(%(test_name)s, %(name)sInvalidArgs%(arg_index)d_%(value_index)d) {
  EXPECT_CALL(*gl_, %(gl_func_name)s(%(gl_args)s)).Times(0);
  SpecializedSetup<%(name)s, 0>(false);
  %(name)s cmd;
  cmd.Init(%(args)s%(comma)sshared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::%(parse_result)s, ExecuteCmd(cmd));%(gl_error_test)s
}
"""
    self.WriteInvalidUnitTest(func, file, invalid_test, {
          'comma': comma,
        })

    invalid_test = """
TEST_F(%(test_name)s, %(name)sInvalidArgsBadSharedMemoryId) {
  EXPECT_CALL(*gl_, %(gl_func_name)s(%(gl_args)s)).Times(0);
  SpecializedSetup<%(name)s, 0>(false);
  %(name)s cmd;
  cmd.Init(%(args)s%(comma)skInvalidSharedMemoryId, shared_memory_offset_);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  cmd.Init(%(args)s%(comma)sshared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}
"""
    self.WriteValidUnitTest(func, file, invalid_test, {
          'comma': comma,
        })

  def WriteServiceImplementation(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write(
        "error::Error GLES2DecoderImpl::Handle%s(\n" % func.name)
    file.Write(
        "    uint32 immediate_data_size, const gles2::%s& c) {\n" % func.name)
    args = func.GetOriginalArgs()
    for arg in args:
      arg.WriteGetCode(file)

    code = """  typedef %(func_name)s::Result Result;
  Result* result_dst = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  if (!result_dst) {
    return error::kOutOfBounds;
  }
"""
    file.Write(code % {'func_name': func.name})
    func.WriteHandlerValidation(file)
    file.Write("  *result_dst = %s(%s);\n" %
               (func.GetGLFunctionName(), func.MakeOriginalArgString("")))
    file.Write("  return error::kNoError;\n")
    file.Write("}\n")
    file.Write("\n")

  def WriteGLES2ImplementationHeader(self, func, file):
    """Overrriden from TypeHandler."""
    impl_func = func.GetInfo('impl_func')
    if impl_func == None or impl_func == True:
      error_value = func.GetInfo("error_value") or "GL_FALSE"
      file.Write("%s %s(%s) {\n" %
                 (func.return_type, func.original_name,
                  func.MakeTypedOriginalArgString("")))
      file.Write("  GPU_CLIENT_SINGLE_THREAD_CHECK();\n")
      func.WriteDestinationInitalizationValidation(file)
      self.WriteClientGLCallLog(func, file)
      file.Write("  typedef %s::Result Result;\n" % func.name)
      file.Write("  Result* result = GetResultAs<Result*>();\n")
      file.Write("  if (!result) {\n")
      file.Write("    return %s;\n" % error_value)
      file.Write("  }\n")
      file.Write("  *result = 0;\n")
      arg_string = func.MakeOriginalArgString("")
      comma = ""
      if len(arg_string) > 0:
        comma = ", "
      file.Write(
          "  helper_->%s(%s%sGetResultShmId(), GetResultShmOffset());\n" %
                 (func.name, arg_string, comma))
      file.Write("  WaitForCmd();\n")
      file.Write('  GPU_CLIENT_LOG("returned " << *result);\n')
      file.Write("  return *result;\n")
      file.Write("}\n")
      file.Write("\n")
    else:
      self.WriteGLES2ImplementationDeclaration(func, file)

  def WriteGLES2ImplementationUnitTest(self, func, file):
    """Overrriden from TypeHandler."""
    client_test = func.GetInfo('client_test')
    if client_test == None or client_test == True:
      code = """
TEST_F(GLES2ImplementationTest, %(name)s) {
  struct Cmds {
    %(name)s cmd;
  };

  typedef %(name)s::Result Result;
  Cmds expected;
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(%(name)s::Result));
  expected.cmd.Init(1, result1.id, result1.offset);

  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, uint32(1)))
      .RetiresOnSaturation();

  GLboolean result = gl_->%(name)s(1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_TRUE(result);
}
"""
      file.Write(code % {
            'name': func.name,
          })


class STRnHandler(TypeHandler):
  """Handler for GetProgramInfoLog, GetShaderInfoLog, GetShaderSource, and
  GetTranslatedShaderSourceANGLE."""

  def __init__(self):
    TypeHandler.__init__(self)

  def InitFunction(self, func):
    """Overrriden from TypeHandler."""
    # remove all but the first cmd args.
    cmd_args = func.GetCmdArgs()
    func.ClearCmdArgs()
    func.AddCmdArg(cmd_args[0])
    # add on a bucket id.
    func.AddCmdArg(Argument('bucket_id', 'uint32'))

  def WriteGLES2ImplementationHeader(self, func, file):
    """Overrriden from TypeHandler."""
    code_1 = """%(return_type)s %(func_name)s(%(args)s) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
"""
    code_2 = """  GPU_CLIENT_LOG("[" << this << "] gl%(func_name)s" << "("
      << %(arg0)s << ", "
      << %(arg1)s << ", "
      << static_cast<void*>(%(arg2)s) << ", "
      << static_cast<void*>(%(arg3)s) << ")");
  helper_->SetBucketSize(kResultBucketId, 0);
  helper_->%(func_name)s(%(id_name)s, kResultBucketId);
  std::string str;
  GLsizei max_size = 0;
  if (GetBucketAsString(kResultBucketId, &str)) {
    if (bufsize > 0) {
      max_size =
          std::min(static_cast<size_t>(%(bufsize_name)s) - 1, str.size());
      memcpy(%(dest_name)s, str.c_str(), max_size);
      %(dest_name)s[max_size] = '\\0';
      GPU_CLIENT_LOG("------\\n" << %(dest_name)s << "\\n------");
    }
  }
  if (%(length_name)s != NULL) {
    *%(length_name)s = max_size;
  }
}
"""
    args = func.GetOriginalArgs()
    str_args = {
      'return_type': func.return_type,
      'func_name': func.original_name,
      'args': func.MakeTypedOriginalArgString(""),
      'id_name': args[0].name,
      'bufsize_name': args[1].name,
      'length_name': args[2].name,
      'dest_name': args[3].name,
      'arg0': args[0].name,
      'arg1': args[1].name,
      'arg2': args[2].name,
      'arg3': args[3].name,
    }
    file.Write(code_1 % str_args)
    func.WriteDestinationInitalizationValidation(file)
    file.Write(code_2 % str_args)

  def WriteServiceUnitTest(self, func, file):
    """Overrriden from TypeHandler."""
    valid_test = """
TEST_F(%(test_name)s, %(name)sValidArgs) {
  const char* kInfo = "hello";
  const uint32 kBucketId = 123;
  SpecializedSetup<%(name)s, 0>(true);
%(expect_len_code)s
  EXPECT_CALL(*gl_, %(gl_func_name)s(%(gl_args)s))
      .WillOnce(DoAll(SetArgumentPointee<2>(strlen(kInfo)),
                      SetArrayArgument<3>(kInfo, kInfo + strlen(kInfo) + 1)));
  %(name)s cmd;
  cmd.Init(%(args)s);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  CommonDecoder::Bucket* bucket = decoder_->GetBucket(kBucketId);
  ASSERT_TRUE(bucket != NULL);
  EXPECT_EQ(strlen(kInfo) + 1, bucket->size());
  EXPECT_EQ(0, memcmp(bucket->GetData(0, bucket->size()), kInfo,
                      bucket->size()));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}
"""
    args = func.GetOriginalArgs()
    id_name = args[0].GetValidGLArg(func, 0, 0)
    get_len_func = func.GetInfo('get_len_func')
    get_len_enum = func.GetInfo('get_len_enum')
    sub = {
        'id_name': id_name,
        'get_len_func': get_len_func,
        'get_len_enum': get_len_enum,
        'gl_args': '%s, strlen(kInfo) + 1, _, _' %
             args[0].GetValidGLArg(func, 0, 0),
        'args': '%s, kBucketId' % args[0].GetValidArg(func, 0, 0),
        'expect_len_code': '',
    }
    if get_len_func and get_len_func[0:2] == 'gl':
      sub['expect_len_code'] = (
        "  EXPECT_CALL(*gl_, %s(%s, %s, _))\n"
        "      .WillOnce(SetArgumentPointee<2>(strlen(kInfo) + 1));") % (
            get_len_func[2:], id_name, get_len_enum)
    self.WriteValidUnitTest(func, file, valid_test, sub)

    invalid_test = """
TEST_F(%(test_name)s, %(name)sInvalidArgs) {
  const uint32 kBucketId = 123;
  EXPECT_CALL(*gl_, %(gl_func_name)s(_, _, _, _))
      .Times(0);
  %(name)s cmd;
  cmd.Init(kInvalidClientId, kBucketId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}
"""
    self.WriteValidUnitTest(func, file, invalid_test)

  def WriteServiceImplementation(self, func, file):
    """Overrriden from TypeHandler."""
    pass


class FunctionInfo(object):
  """Holds info about a function."""

  def __init__(self, info, type_handler):
    for key in info:
      setattr(self, key, info[key])
    self.type_handler = type_handler
    if not 'type' in info:
      self.type = ''


class Argument(object):
  """A class that represents a function argument."""

  cmd_type_map_ = {
    'GLenum': 'uint32',
    'GLint': 'int32',
    'GLintptr': 'int32',
    'GLsizei': 'int32',
    'GLsizeiptr': 'int32',
    'GLfloat': 'float',
    'GLclampf': 'float',
  }
  need_validation_ = ['GLsizei*', 'GLboolean*', 'GLenum*', 'GLint*']

  def __init__(self, name, type):
    self.name = name
    self.type = type

    if type in self.cmd_type_map_:
      self.cmd_type = self.cmd_type_map_[type]
    else:
      self.cmd_type = 'uint32'

  def IsPointer(self):
    """Returns true if argument is a pointer."""
    return False

  def AddCmdArgs(self, args):
    """Adds command arguments for this argument to the given list."""
    return args.append(self)

  def AddInitArgs(self, args):
    """Adds init arguments for this argument to the given list."""
    return args.append(self)

  def GetValidArg(self, func, offset, index):
    """Gets a valid value for this argument."""
    valid_arg = func.GetValidArg(offset)
    if valid_arg != None:
      return valid_arg
    return str(offset + 1)

  def GetValidClientSideArg(self, func, offset, index):
    """Gets a valid value for this argument."""
    return str(offset + 1)

  def GetValidClientSideCmdArg(self, func, offset, index):
    """Gets a valid value for this argument."""
    return str(offset + 1)

  def GetValidGLArg(self, func, offset, index):
    """Gets a valid GL value for this argument."""
    valid_arg = func.GetValidArg(offset)
    if valid_arg != None:
      return valid_arg
    return str(offset + 1)

  def GetNumInvalidValues(self, func):
    """returns the number of invalid values to be tested."""
    return 0

  def GetInvalidArg(self, offset, index):
    """returns an invalid value and expected parse result by index."""
    return ("---ERROR0---", "---ERROR2---", None)

  def GetLogArg(self):
    """Get argument appropriate for LOG macro."""
    if self.type == 'GLboolean':
      return 'GLES2Util::GetStringBool(%s)' % self.name
    if self.type == 'GLenum':
      return 'GLES2Util::GetStringEnum(%s)' % self.name
    return self.name

  def WriteGetCode(self, file):
    """Writes the code to get an argument from a command structure."""
    file.Write("  %s %s = static_cast<%s>(c.%s);\n" %
               (self.type, self.name, self.type, self.name))

  def WriteValidationCode(self, file, func):
    """Writes the validation code for an argument."""
    pass

  def WriteClientSideValidationCode(self, file, func):
    """Writes the validation code for an argument."""
    pass

  def WriteDestinationInitalizationValidation(self, file, func):
    """Writes the client side destintion initialization validation."""
    pass

  def WriteDestinationInitalizationValidatationIfNeeded(self, file, func):
    """Writes the client side destintion initialization validation if needed."""
    parts = self.type.split(" ")
    if len(parts) > 1:
      return
    if parts[0] in self.need_validation_:
      file.Write("  GPU_CLIENT_VALIDATE_DESTINATION_INITALIZATION(%s, %s);\n" %
          (self.type[:-1], self.name))


  def WriteGetAddress(self, file):
    """Writes the code to get the address this argument refers to."""
    pass

  def GetImmediateVersion(self):
    """Gets the immediate version of this argument."""
    return self

  def GetBucketVersion(self):
    """Gets the bucket version of this argument."""
    return self


class BoolArgument(Argument):
  """class for GLboolean"""

  def __init__(self, name, type):
    Argument.__init__(self, name, 'GLboolean')

  def GetValidArg(self, func, offset, index):
    """Gets a valid value for this argument."""
    return 'true'

  def GetValidClientSideArg(self, func, offset, index):
    """Gets a valid value for this argument."""
    return 'true'

  def GetValidClientSideCmdArg(self, func, offset, index):
    """Gets a valid value for this argument."""
    return 'true'

  def GetValidGLArg(self, func, offset, index):
    """Gets a valid GL value for this argument."""
    return 'true'


class UniformLocationArgument(Argument):
  """class for uniform locations."""

  def __init__(self, name):
    Argument.__init__(self, name, "GLint")

  def WriteGetCode(self, file):
    """Writes the code to get an argument from a command structure."""
    code = """  %s %s = program_manager()->UnswizzleLocation(
      static_cast<%s>(c.%s));
"""
    file.Write(code % (self.type, self.name, self.type, self.name))

  def GetValidArg(self, func, offset, index):
    """Gets a valid value for this argument."""
    return "program_manager()->SwizzleLocation(%d)" % (offset + 1)


class DataSizeArgument(Argument):
  """class for data_size which Bucket commands do not need."""

  def __init__(self, name):
    Argument.__init__(self, name, "uint32")

  def GetBucketVersion(self):
    return None


class SizeArgument(Argument):
  """class for GLsizei and GLsizeiptr."""

  def __init__(self, name, type):
    Argument.__init__(self, name, type)

  def GetNumInvalidValues(self, func):
    """overridden from Argument."""
    if func.is_immediate:
      return 0
    return 1

  def GetInvalidArg(self, offset, index):
    """overridden from Argument."""
    return ("-1", "kNoError", "GL_INVALID_VALUE")

  def WriteValidationCode(self, file, func):
    """overridden from Argument."""
    file.Write("  if (%s < 0) {\n" % self.name)
    file.Write("    SetGLError(GL_INVALID_VALUE, \"gl%s: %s < 0\");\n" %
               (func.original_name, self.name))
    file.Write("    return error::kNoError;\n")
    file.Write("  }\n")

  def WriteClientSideValidationCode(self, file, func):
    """overridden from Argument."""
    file.Write("  if (%s < 0) {\n" % self.name)
    file.Write("    SetGLError(GL_INVALID_VALUE, \"gl%s: %s < 0\");\n" %
               (func.original_name, self.name))
    file.Write("    return;\n")
    file.Write("  }\n")


class SizeNotNegativeArgument(SizeArgument):
  """class for GLsizeiNotNegative. It's NEVER allowed to be negative"""

  def __init__(self, name, type, gl_type):
    SizeArgument.__init__(self, name, gl_type)

  def GetInvalidArg(self, offset, index):
    """overridden from SizeArgument."""
    return ("-1", "kOutOfBounds", "GL_NO_ERROR")

  def WriteValidationCode(self, file, func):
    """overridden from SizeArgument."""
    pass
    #file.Write("  if (%s < 0) {\n" % self.name)
    #file.Write("    SetGLError(GL_INVALID_VALUE, \"gl%s: %s < 0\");\n" %
    #           (func.original_name, self.name))
    #file.Write("    return error::kNoError;\n")
    #file.Write("  }\n")


class EnumBaseArgument(Argument):
  """Base class for EnumArgument, IntArgument and ValidatedBoolArgument"""

  def __init__(self, name, gl_type, type, gl_error):
    Argument.__init__(self, name, gl_type)

    self.local_type = type
    self.gl_error = gl_error
    name = type[len(gl_type):]
    self.type_name = name
    self.enum_info = _ENUM_LISTS[name]

  def WriteValidationCode(self, file, func):
    file.Write("  if (!validators_->%s.IsValid(%s)) {\n" %
        (ToUnderscore(self.type_name), self.name))
    file.Write("    SetGLError(%s, \"gl%s: %s %s\");\n" %
               (self.gl_error, func.original_name, self.name, self.gl_error))
    file.Write("    return error::kNoError;\n")
    file.Write("  }\n")

  def GetValidArg(self, func, offset, index):
    valid_arg = func.GetValidArg(offset)
    if valid_arg != None:
      return valid_arg
    if 'valid' in self.enum_info:
      valid = self.enum_info['valid']
      num_valid = len(valid)
      if index >= num_valid:
        index = num_valid - 1
      return valid[index]
    return str(offset + 1)

  def GetValidClientSideArg(self, func, offset, index):
    """Gets a valid value for this argument."""
    return self.GetValidArg(func, offset, index)

  def GetValidClientSideCmdArg(self, func, offset, index):
    """Gets a valid value for this argument."""
    return self.GetValidArg(func, offset, index)

  def GetValidGLArg(self, func, offset, index):
    """Gets a valid value for this argument."""
    return self.GetValidArg(func, offset, index)

  def GetNumInvalidValues(self, func):
    """returns the number of invalid values to be tested."""
    if 'invalid' in self.enum_info:
      invalid = self.enum_info['invalid']
      return len(invalid)
    return 0

  def GetInvalidArg(self, offset, index):
    """returns an invalid value by index."""
    if 'invalid' in self.enum_info:
      invalid = self.enum_info['invalid']
      num_invalid = len(invalid)
      if index >= num_invalid:
        index = num_invalid - 1
      return (invalid[index], "kNoError", self.gl_error)
    return ("---ERROR1---", "kNoError", self.gl_error)


class EnumArgument(EnumBaseArgument):
  """A class that represents a GLenum argument"""

  def __init__(self, name, type):
    EnumBaseArgument.__init__(self, name, "GLenum", type, "GL_INVALID_ENUM")

  def GetLogArg(self):
    """Overridden from Argument."""
    return ("GLES2Util::GetString%s(%s)" %
            (self.type_name, self.name))


class IntArgument(EnumBaseArgument):
  """A class for a GLint argument that can only except specific values.

  For example glTexImage2D takes a GLint for its internalformat
  argument instead of a GLenum.
  """

  def __init__(self, name, type):
    EnumBaseArgument.__init__(self, name, "GLint", type, "GL_INVALID_VALUE")


class ValidatedBoolArgument(EnumBaseArgument):
  """A class for a GLboolean argument that can only except specific values.

  For example glUniformMatrix takes a GLboolean for it's transpose but it
  must be false.
  """

  def __init__(self, name, type):
    EnumBaseArgument.__init__(self, name, "GLboolean", type, "GL_INVALID_VALUE")

  def GetLogArg(self):
    """Overridden from Argument."""
    return 'GLES2Util::GetStringBool(%s)' % self.name


class ImmediatePointerArgument(Argument):
  """A class that represents an immediate argument to a function.

  An immediate argument is one where the data follows the command.
  """

  def __init__(self, name, type):
    Argument.__init__(self, name, type)

  def AddCmdArgs(self, args):
    """Overridden from Argument."""
    pass

  def WriteGetCode(self, file):
    """Overridden from Argument."""
    file.Write(
      "  %s %s = GetImmediateDataAs<%s>(\n" %
      (self.type, self.name, self.type))
    file.Write("      c, data_size, immediate_data_size);\n")

  def WriteValidationCode(self, file, func):
    """Overridden from Argument."""
    file.Write("  if (%s == NULL) {\n" % self.name)
    file.Write("    return error::kOutOfBounds;\n")
    file.Write("  }\n")

  def GetImmediateVersion(self):
    """Overridden from Argument."""
    return None

  def WriteDestinationInitalizationValidation(self, file, func):
    """Overridden from Argument."""
    self.WriteDestinationInitalizationValidatationIfNeeded(file, func)

  def GetLogArg(self):
    """Overridden from Argument."""
    return "static_cast<const void*>(%s)" % self.name


class BucketPointerArgument(Argument):
  """A class that represents an bucket argument to a function."""

  def __init__(self, name, type):
    Argument.__init__(self, name, type)

  def AddCmdArgs(self, args):
    """Overridden from Argument."""
    pass

  def WriteGetCode(self, file):
    """Overridden from Argument."""
    file.Write(
      "  %s %s = bucket->GetData(0, data_size);\n" %
      (self.type, self.name))

  def WriteValidationCode(self, file, func):
    """Overridden from Argument."""
    pass

  def GetImmediateVersion(self):
    """Overridden from Argument."""
    return None

  def WriteDestinationInitalizationValidation(self, file, func):
    """Overridden from Argument."""
    self.WriteDestinationInitalizationValidatationIfNeeded(file, func)

  def GetLogArg(self):
    """Overridden from Argument."""
    return "static_cast<const void*>(%s)" % self.name


class PointerArgument(Argument):
  """A class that represents a pointer argument to a function."""

  def __init__(self, name, type):
    Argument.__init__(self, name, type)

  def IsPointer(self):
    """Returns true if argument is a pointer."""
    return True

  def GetValidArg(self, func, offset, index):
    """Overridden from Argument."""
    return "shared_memory_id_, shared_memory_offset_"

  def GetValidGLArg(self, func, offset, index):
    """Overridden from Argument."""
    return "reinterpret_cast<%s>(shared_memory_address_)" % self.type

  def GetNumInvalidValues(self, func):
    """Overridden from Argument."""
    return 2

  def GetInvalidArg(self, offset, index):
    """Overridden from Argument."""
    if index == 0:
      return ("kInvalidSharedMemoryId, 0", "kOutOfBounds", None)
    else:
      return ("shared_memory_id_, kInvalidSharedMemoryOffset",
              "kOutOfBounds", None)

  def GetLogArg(self):
    """Overridden from Argument."""
    return "static_cast<const void*>(%s)" % self.name

  def AddCmdArgs(self, args):
    """Overridden from Argument."""
    args.append(Argument("%s_shm_id" % self.name, 'uint32'))
    args.append(Argument("%s_shm_offset" % self.name, 'uint32'))

  def WriteGetCode(self, file):
    """Overridden from Argument."""
    file.Write(
        "  %s %s = GetSharedMemoryAs<%s>(\n" %
        (self.type, self.name, self.type))
    file.Write(
        "      c.%s_shm_id, c.%s_shm_offset, data_size);\n" %
        (self.name, self.name))

  def WriteGetAddress(self, file):
    """Overridden from Argument."""
    file.Write(
        "  %s %s = GetSharedMemoryAs<%s>(\n" %
        (self.type, self.name, self.type))
    file.Write(
        "      %s_shm_id, %s_shm_offset, %s_size);\n" %
        (self.name, self.name, self.name))

  def WriteValidationCode(self, file, func):
    """Overridden from Argument."""
    file.Write("  if (%s == NULL) {\n" % self.name)
    file.Write("    return error::kOutOfBounds;\n")
    file.Write("  }\n")

  def GetImmediateVersion(self):
    """Overridden from Argument."""
    return ImmediatePointerArgument(self.name, self.type)

  def GetBucketVersion(self):
    """Overridden from Argument."""
    if self.type == "const char*":
      return InputStringBucketArgument(self.name, self.type)
    return BucketPointerArgument(self.name, self.type)

  def WriteDestinationInitalizationValidation(self, file, func):
    """Overridden from Argument."""
    self.WriteDestinationInitalizationValidatationIfNeeded(file, func)


class InputStringBucketArgument(Argument):
  """An string input argument where the string is passed in a bucket."""

  def __init__(self, name, type):
    Argument.__init__(self, name + "_bucket_id", "uint32")

  def WriteGetCode(self, file):
    """Overridden from Argument."""
    code = """
  Bucket* %(name)s_bucket = GetBucket(c.%(name)s);
  if (!%(name)s_bucket) {
    return error::kInvalidArguments;
  }
  std::string %(name)s_str;
  if (!%(name)s_bucket->GetAsString(&%(name)s_str)) {
    return error::kInvalidArguments;
  }
  const char* %(name)s = %(name)s_str.c_str();
"""
    file.Write(code % {
        'name': self.name,
      })

  def GetValidArg(self, func, offset, index):
    return "kNameBucketId"

  def GetValidGLArg(self, func, offset, index):
    return "_"


class NonImmediatePointerArgument(PointerArgument):
  """A pointer argument that stays a pointer even in an immediate cmd."""

  def __init__(self, name, type):
    PointerArgument.__init__(self, name, type)

  def IsPointer(self):
    """Returns true if argument is a pointer."""
    return False

  def GetImmediateVersion(self):
    """Overridden from Argument."""
    return self


class ResourceIdArgument(Argument):
  """A class that represents a resource id argument to a function."""

  def __init__(self, name, type):
    match = re.match("(GLid\w+)", type)
    self.resource_type = match.group(1)[4:]
    type = type.replace(match.group(1), "GLuint")
    Argument.__init__(self, name, type)

  def WriteGetCode(self, file):
    """Overridden from Argument."""
    file.Write("  %s %s = c.%s;\n" % (self.type, self.name, self.name))

  def GetValidArg(self, func, offset, index):
    return "client_%s_id_" % self.resource_type.lower()

  def GetValidGLArg(self, func, offset, index):
    return "kService%sId" % self.resource_type


class ResourceIdBindArgument(Argument):
  """Represents a resource id argument to a bind function."""

  def __init__(self, name, type):
    match = re.match("(GLidBind\w+)", type)
    self.resource_type = match.group(1)[8:]
    type = type.replace(match.group(1), "GLuint")
    Argument.__init__(self, name, type)

  def WriteGetCode(self, file):
    """Overridden from Argument."""
    code = """  %(type)s %(name)s = c.%(name)s;
"""
    file.Write(code % {'type': self.type, 'name': self.name})

  def GetValidArg(self, func, offset, index):
    return "client_%s_id_" % self.resource_type.lower()

  def GetValidGLArg(self, func, offset, index):
    return "kService%sId" % self.resource_type


class ResourceIdZeroArgument(Argument):
  """Represents a resource id argument to a function that can be zero."""

  def __init__(self, name, type):
    match = re.match("(GLidZero\w+)", type)
    self.resource_type = match.group(1)[8:]
    type = type.replace(match.group(1), "GLuint")
    Argument.__init__(self, name, type)

  def WriteGetCode(self, file):
    """Overridden from Argument."""
    file.Write("  %s %s = c.%s;\n" % (self.type, self.name, self.name))

  def GetValidArg(self, func, offset, index):
    return "client_%s_id_" % self.resource_type.lower()

  def GetValidGLArg(self, func, offset, index):
    return "kService%sId" % self.resource_type

  def GetNumInvalidValues(self, func):
    """returns the number of invalid values to be tested."""
    return 1

  def GetInvalidArg(self, offset, index):
    """returns an invalid value by index."""
    return ("kInvalidClientId", "kNoError", "GL_INVALID_VALUE")


class Function(object):
  """A class that represents a function."""

  def __init__(self, original_name, name, info, return_type, original_args,
               args_for_cmds, cmd_args, init_args, num_pointer_args):
    self.name = name
    self.original_name = original_name
    self.info = info
    self.type_handler = info.type_handler
    self.return_type = return_type
    self.original_args = original_args
    self.num_pointer_args = num_pointer_args
    self.can_auto_generate = num_pointer_args == 0 and return_type == "void"
    self.cmd_args = cmd_args
    self.init_args = init_args
    self.InitFunction()
    self.args_for_cmds = args_for_cmds
    self.is_immediate = False

  def IsType(self, type_name):
    """Returns true if function is a certain type."""
    return self.info.type == type_name

  def InitFunction(self):
    """Calls the init function for the type handler."""
    self.type_handler.InitFunction(self)

  def GetInfo(self, name):
    """Returns a value from the function info for this function."""
    if hasattr(self.info, name):
      return getattr(self.info, name)
    return None

  def GetValidArg(self, index):
    """Gets a valid arg from the function info if one exists."""
    valid_args = self.GetInfo('valid_args')
    if valid_args and str(index) in valid_args:
      return valid_args[str(index)]
    return None

  def AddInfo(self, name, value):
    """Adds an info."""
    setattr(self.info, name, value)

  def IsCoreGLFunction(self):
    return (not self.GetInfo('extension') and
            not self.GetInfo('pepper_interface'))

  def InPepperInterface(self, interface):
    ext = self.GetInfo('pepper_interface')
    if not interface.GetName():
      return self.IsCoreGLFunction()
    return ext == interface.GetName()

  def InAnyPepperExtension(self):
    return self.IsCoreGLFunction() or self.GetInfo('pepper_interface')

  def GetGLFunctionName(self):
    """Gets the function to call to execute GL for this command."""
    if self.GetInfo('decoder_func'):
      return self.GetInfo('decoder_func')
    return "gl%s" % self.original_name

  def GetGLTestFunctionName(self):
    gl_func_name = self.GetInfo('gl_test_func')
    if gl_func_name == None:
      gl_func_name = self.GetGLFunctionName()
    if gl_func_name.startswith("gl"):
      gl_func_name = gl_func_name[2:]
    else:
      gl_func_name = self.original_name
    return gl_func_name

  def AddCmdArg(self, arg):
    """Adds a cmd argument to this function."""
    self.cmd_args.append(arg)

  def GetCmdArgs(self):
    """Gets the command args for this function."""
    return self.cmd_args

  def ClearCmdArgs(self):
    """Clears the command args for this function."""
    self.cmd_args = []

  def GetInitArgs(self):
    """Gets the init args for this function."""
    return self.init_args

  def GetOriginalArgs(self):
    """Gets the original arguments to this function."""
    return self.original_args

  def GetLastOriginalArg(self):
    """Gets the last original argument to this function."""
    return self.original_args[len(self.original_args) - 1]

  def __GetArgList(self, arg_string, add_comma):
    """Adds a comma if arg_string is not empty and add_comma is true."""
    comma = ""
    if add_comma and len(arg_string):
      comma = ", "
    return "%s%s" % (comma, arg_string)

  def MakeTypedOriginalArgString(self, prefix, add_comma = False):
    """Gets a list of arguments as they arg in GL."""
    args = self.GetOriginalArgs()
    arg_string = ", ".join(
        ["%s %s%s" % (arg.type, prefix, arg.name) for arg in args])
    return self.__GetArgList(arg_string, add_comma)

  def MakeOriginalArgString(self, prefix, add_comma = False, separator = ", "):
    """Gets the list of arguments as they are in GL."""
    args = self.GetOriginalArgs()
    arg_string = separator.join(
        ["%s%s" % (prefix, arg.name) for arg in args])
    return self.__GetArgList(arg_string, add_comma)

  def MakeTypedCmdArgString(self, prefix, add_comma = False):
    """Gets a typed list of arguments as they need to be for command buffers."""
    args = self.GetCmdArgs()
    arg_string = ", ".join(
        ["%s %s%s" % (arg.type, prefix, arg.name) for arg in args])
    return self.__GetArgList(arg_string, add_comma)

  def MakeCmdArgString(self, prefix, add_comma = False):
    """Gets the list of arguments as they need to be for command buffers."""
    args = self.GetCmdArgs()
    arg_string = ", ".join(
        ["%s%s" % (prefix, arg.name) for arg in args])
    return self.__GetArgList(arg_string, add_comma)

  def MakeTypedInitString(self, prefix, add_comma = False):
    """Gets a typed list of arguments as they need to be for cmd Init/Set."""
    args = self.GetInitArgs()
    arg_string = ", ".join(
        ["%s %s%s" % (arg.type, prefix, arg.name) for arg in args])
    return self.__GetArgList(arg_string, add_comma)

  def MakeInitString(self, prefix, add_comma = False):
    """Gets the list of arguments as they need to be for cmd Init/Set."""
    args = self.GetInitArgs()
    arg_string = ", ".join(
        ["%s%s" % (prefix, arg.name) for arg in args])
    return self.__GetArgList(arg_string, add_comma)

  def MakeLogArgString(self):
    """Makes a string of the arguments for the LOG macros"""
    args = self.GetOriginalArgs()
    return ' << ", " << '.join([arg.GetLogArg() for arg in args])

  def WriteCommandDescription(self, file):
    """Writes a description of the command."""
    file.Write("//! Command that corresponds to gl%s.\n" % self.original_name)

  def WriteHandlerValidation(self, file):
    """Writes validation code for the function."""
    for arg in self.GetOriginalArgs():
      arg.WriteValidationCode(file, self)
    self.WriteValidationCode(file)

  def WriteHandlerImplementation(self, file):
    """Writes the handler implementation for this command."""
    self.type_handler.WriteHandlerImplementation(self, file)

  def WriteValidationCode(self, file):
    """Writes the validation code for a command."""
    pass

  def WriteCmdArgFlag(self, file):
    """Writes the cmd kArgFlags constant."""
    file.Write("  static const cmd::ArgFlags kArgFlags = cmd::kFixed;\n")

  def WriteCmdComputeSize(self, file):
    """Writes the ComputeSize function for the command."""
    file.Write("  static uint32 ComputeSize() {\n")
    file.Write(
        "    return static_cast<uint32>(sizeof(ValueType));  // NOLINT\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteCmdSetHeader(self, file):
    """Writes the cmd's SetHeader function."""
    file.Write("  void SetHeader() {\n")
    file.Write("    header.SetCmd<ValueType>();\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteCmdInit(self, file):
    """Writes the cmd's Init function."""
    file.Write("  void Init(%s) {\n" % self.MakeTypedCmdArgString("_"))
    file.Write("    SetHeader();\n")
    args = self.GetCmdArgs()
    for arg in args:
      file.Write("    %s = _%s;\n" % (arg.name, arg.name))
    file.Write("  }\n")
    file.Write("\n")

  def WriteCmdSet(self, file):
    """Writes the cmd's Set function."""
    copy_args = self.MakeCmdArgString("_", False)
    file.Write("  void* Set(void* cmd%s) {\n" %
               self.MakeTypedCmdArgString("_", True))
    file.Write("    static_cast<ValueType*>(cmd)->Init(%s);\n" % copy_args)
    file.Write("    return NextCmdAddress<ValueType>(cmd);\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteStruct(self, file):
    self.type_handler.WriteStruct(self, file)

  def WriteDocs(self, file):
    self.type_handler.WriteDocs(self, file)

  def WriteCmdHelper(self, file):
    """Writes the cmd's helper."""
    self.type_handler.WriteCmdHelper(self, file)

  def WriteServiceImplementation(self, file):
    """Writes the service implementation for a command."""
    self.type_handler.WriteServiceImplementation(self, file)

  def WriteServiceUnitTest(self, file):
    """Writes the service implementation for a command."""
    self.type_handler.WriteServiceUnitTest(self, file)

  def WriteGLES2CLibImplementation(self, file):
    """Writes the GLES2 C Lib Implemention."""
    self.type_handler.WriteGLES2CLibImplementation(self, file)

  def WriteGLES2ImplementationHeader(self, file):
    """Writes the GLES2 Implemention declaration."""
    self.type_handler.WriteGLES2ImplementationHeader(self, file)

  def WriteGLES2ImplementationUnitTest(self, file):
    """Writes the GLES2 Implemention unit test."""
    self.type_handler.WriteGLES2ImplementationUnitTest(self, file)

  def WriteDestinationInitalizationValidation(self, file):
    """Writes the client side destintion initialization validation."""
    self.type_handler.WriteDestinationInitalizationValidation(self, file)

  def WriteFormatTest(self, file):
    """Writes the cmd's format test."""
    self.type_handler.WriteFormatTest(self, file)


class PepperInterface(object):
  """A class that represents a function."""

  def __init__(self, info):
    self.name = info["name"]
    self.dev = info["dev"]

  def GetName(self):
    return self.name

  def GetInterfaceName(self):
    upperint = ""
    dev = ""
    if self.name:
      upperint = "_" + self.name.upper()
    if self.dev:
      dev = "_DEV"
    return "PPB_OPENGLES2%s%s_INTERFACE" % (upperint, dev)

  def GetInterfaceString(self):
    dev = ""
    if self.dev:
      dev = "(Dev)"
    return "PPB_OpenGLES2%s%s" % (self.name, dev)

  def GetStructName(self):
    dev = ""
    if self.dev:
      dev = "_Dev"
    return "PPB_OpenGLES2%s%s" % (self.name, dev)


class ImmediateFunction(Function):
  """A class that represnets an immediate function command."""

  def __init__(self, func):
    new_args = []
    for arg in func.GetOriginalArgs():
      new_arg = arg.GetImmediateVersion()
      if new_arg:
        new_args.append(new_arg)

    cmd_args = []
    new_args_for_cmds = []
    for arg in func.args_for_cmds:
      new_arg = arg.GetImmediateVersion()
      if new_arg:
        new_args_for_cmds.append(new_arg)
        new_arg.AddCmdArgs(cmd_args)

    new_init_args = []
    for arg in new_args_for_cmds:
      arg.AddInitArgs(new_init_args)

    Function.__init__(
        self,
        func.original_name,
        "%sImmediate" % func.name,
        func.info,
        func.return_type,
        new_args,
        new_args_for_cmds,
        cmd_args,
        new_init_args,
        0)
    self.is_immediate = True

  def WriteCommandDescription(self, file):
    """Overridden from Function"""
    file.Write("//! Immediate version of command that corresponds to gl%s.\n" %
        self.original_name)

  def WriteServiceImplementation(self, file):
    """Overridden from Function"""
    self.type_handler.WriteImmediateServiceImplementation(self, file)

  def WriteHandlerImplementation(self, file):
    """Overridden from Function"""
    self.type_handler.WriteImmediateHandlerImplementation(self, file)

  def WriteServiceUnitTest(self, file):
    """Writes the service implementation for a command."""
    self.type_handler.WriteImmediateServiceUnitTest(self, file)

  def WriteValidationCode(self, file):
    """Overridden from Function"""
    self.type_handler.WriteImmediateValidationCode(self, file)

  def WriteCmdArgFlag(self, file):
    """Overridden from Function"""
    file.Write("  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;\n")

  def WriteCmdComputeSize(self, file):
    """Overridden from Function"""
    self.type_handler.WriteImmediateCmdComputeSize(self, file)

  def WriteCmdSetHeader(self, file):
    """Overridden from Function"""
    self.type_handler.WriteImmediateCmdSetHeader(self, file)

  def WriteCmdInit(self, file):
    """Overridden from Function"""
    self.type_handler.WriteImmediateCmdInit(self, file)

  def WriteCmdSet(self, file):
    """Overridden from Function"""
    self.type_handler.WriteImmediateCmdSet(self, file)

  def WriteCmdHelper(self, file):
    """Overridden from Function"""
    self.type_handler.WriteImmediateCmdHelper(self, file)

  def WriteFormatTest(self, file):
    """Overridden from Function"""
    self.type_handler.WriteImmediateFormatTest(self, file)


class BucketFunction(Function):
  """A class that represnets a bucket version of a function command."""

  def __init__(self, func):
    new_args = []
    for arg in func.GetOriginalArgs():
      new_arg = arg.GetBucketVersion()
      if new_arg:
        new_args.append(new_arg)

    cmd_args = []
    new_args_for_cmds = []
    for arg in func.args_for_cmds:
      new_arg = arg.GetBucketVersion()
      if new_arg:
        new_args_for_cmds.append(new_arg)
        new_arg.AddCmdArgs(cmd_args)

    new_init_args = []
    for arg in new_args_for_cmds:
      arg.AddInitArgs(new_init_args)

    Function.__init__(
        self,
        func.original_name,
        "%sBucket" % func.name,
        func.info,
        func.return_type,
        new_args,
        new_args_for_cmds,
        cmd_args,
        new_init_args,
        0)

#  def InitFunction(self):
#    """Overridden from Function"""
#    pass

  def WriteCommandDescription(self, file):
    """Overridden from Function"""
    file.Write("//! Bucket version of command that corresponds to gl%s.\n" %
        self.original_name)

  def WriteServiceImplementation(self, file):
    """Overridden from Function"""
    self.type_handler.WriteBucketServiceImplementation(self, file)

  def WriteHandlerImplementation(self, file):
    """Overridden from Function"""
    self.type_handler.WriteBucketHandlerImplementation(self, file)

  def WriteServiceUnitTest(self, file):
    """Writes the service implementation for a command."""
    self.type_handler.WriteBucketServiceUnitTest(self, file)


def CreateArg(arg_string):
  """Creates an Argument."""
  arg_parts = arg_string.split()
  if len(arg_parts) == 1 and arg_parts[0] == 'void':
    return None
  # Is this a pointer argument?
  elif arg_string.find('*') >= 0:
    if arg_parts[0] == 'NonImmediate':
      return NonImmediatePointerArgument(
          arg_parts[-1],
          " ".join(arg_parts[1:-1]))
    else:
      return PointerArgument(
          arg_parts[-1],
          " ".join(arg_parts[0:-1]))
  # Is this a resource argument? Must come after pointer check.
  elif arg_parts[0].startswith('GLidBind'):
    return ResourceIdBindArgument(arg_parts[-1], " ".join(arg_parts[0:-1]))
  elif arg_parts[0].startswith('GLidZero'):
    return ResourceIdZeroArgument(arg_parts[-1], " ".join(arg_parts[0:-1]))
  elif arg_parts[0].startswith('GLid'):
    return ResourceIdArgument(arg_parts[-1], " ".join(arg_parts[0:-1]))
  elif arg_parts[0].startswith('GLenum') and len(arg_parts[0]) > 6:
    return EnumArgument(arg_parts[-1], " ".join(arg_parts[0:-1]))
  elif arg_parts[0].startswith('GLboolean') and len(arg_parts[0]) > 9:
    return ValidatedBoolArgument(arg_parts[-1], " ".join(arg_parts[0:-1]))
  elif arg_parts[0].startswith('GLboolean'):
    return BoolArgument(arg_parts[-1], " ".join(arg_parts[0:-1]))
  elif arg_parts[0].startswith('GLintUniformLocation'):
    return UniformLocationArgument(arg_parts[-1])
  elif (arg_parts[0].startswith('GLint') and len(arg_parts[0]) > 5 and
        not arg_parts[0].startswith('GLintptr')):
    return IntArgument(arg_parts[-1], " ".join(arg_parts[0:-1]))
  elif (arg_parts[0].startswith('GLsizeiNotNegative') or
        arg_parts[0].startswith('GLintptrNotNegative')):
    return SizeNotNegativeArgument(arg_parts[-1],
                                   " ".join(arg_parts[0:-1]),
                                   arg_parts[0][0:-11])
  elif arg_parts[0].startswith('GLsize'):
    return SizeArgument(arg_parts[-1], " ".join(arg_parts[0:-1]))
  else:
    return Argument(arg_parts[-1], " ".join(arg_parts[0:-1]))


class GLGenerator(object):
  """A class to generate GL command buffers."""

  _function_re = re.compile(r'GL_APICALL(.*?)GL_APIENTRY (.*?) \((.*?)\);')

  def __init__(self, verbose):
    self.original_functions = []
    self.functions = []
    self.verbose = verbose
    self.errors = 0
    self._function_info = {}
    self._empty_type_handler = TypeHandler()
    self._empty_function_info = FunctionInfo({}, self._empty_type_handler)
    self.pepper_interfaces = []
    self.interface_info = {}

    self._type_handlers = {
      'Bind': BindHandler(),
      'Create': CreateHandler(),
      'Custom': CustomHandler(),
      'Data': DataHandler(),
      'Delete': DeleteHandler(),
      'DELn': DELnHandler(),
      'GENn': GENnHandler(),
      'GETn': GETnHandler(),
      'GLchar': GLcharHandler(),
      'HandWritten': HandWrittenHandler(),
      'Is': IsHandler(),
      'Manual': ManualHandler(),
      'PUT': PUTHandler(),
      'PUTn': PUTnHandler(),
      'PUTXn': PUTXnHandler(),
      'STRn': STRnHandler(),
      'Todo': TodoHandler(),
    }

    for func_name in _FUNCTION_INFO:
      info = _FUNCTION_INFO[func_name]
      type = ''
      if 'type' in info:
        type = info['type']
      self._function_info[func_name] = FunctionInfo(info,
                                                    self.GetTypeHandler(type))
    for interface in _PEPPER_INTERFACES:
      interface = PepperInterface(interface)
      self.pepper_interfaces.append(interface)
      self.interface_info[interface.GetName()] = interface

  def AddFunction(self, func):
    """Adds a function."""
    self.functions.append(func)

  def GetTypeHandler(self, name):
    """Gets a type info for the given type."""
    if name in self._type_handlers:
      return self._type_handlers[name]
    return self._empty_type_handler

  def GetFunctionInfo(self, name):
    """Gets a type info for the given function name."""
    if name in self._function_info:
      return self._function_info[name]
    return self._empty_function_info

  def Log(self, msg):
    """Prints something if verbose is true."""
    if self.verbose:
      print msg

  def Error(self, msg):
    """Prints an error."""
    print "Error: %s" % msg
    self.errors += 1

  def WriteLicense(self, file):
    """Writes the license."""
    file.Write(_LICENSE)

  def WriteNamespaceOpen(self, file):
    """Writes the code for the namespace."""
    file.Write("namespace gpu {\n")
    file.Write("namespace gles2 {\n")
    file.Write("\n")

  def WriteNamespaceClose(self, file):
    """Writes the code to close the namespace."""
    file.Write("}  // namespace gles2\n")
    file.Write("}  // namespace gpu\n")
    file.Write("\n")

  def ParseArgs(self, arg_string):
    """Parses a function arg string."""
    args = []
    num_pointer_args = 0
    parts = arg_string.split(',')
    is_gl_enum = False
    for arg_string in parts:
      if arg_string.startswith('GLenum '):
        is_gl_enum = True
      arg = CreateArg(arg_string)
      if arg:
        args.append(arg)
        if arg.IsPointer():
          num_pointer_args += 1
    return (args, num_pointer_args, is_gl_enum)

  def ParseGLH(self, filename):
    """Parses the cmd_buffer_functions.txt file and extracts the functions"""
    f = open("gpu/command_buffer/cmd_buffer_functions.txt", "r")
    functions = f.read()
    f.close()
    for line in functions.splitlines():
      match = self._function_re.match(line)
      if match:
        func_name = match.group(2)[2:]
        func_info = self.GetFunctionInfo(func_name)
        if func_info.type != 'Noop':
          return_type = match.group(1).strip()
          arg_string = match.group(3)
          (args, num_pointer_args, is_gl_enum) = self.ParseArgs(arg_string)
          # comment in to find out which functions use bare enums.
          # if is_gl_enum:
          #   self.Log("%s uses bare GLenum" % func_name)
          args_for_cmds = args
          if hasattr(func_info, 'cmd_args'):
            (args_for_cmds, num_pointer_args, is_gl_enum) = (
                self.ParseArgs(getattr(func_info, 'cmd_args')))
          cmd_args = []
          for arg in args_for_cmds:
            arg.AddCmdArgs(cmd_args)
          init_args = []
          for arg in args_for_cmds:
            arg.AddInitArgs(init_args)
          return_arg = CreateArg(return_type + " result")
          if return_arg:
            init_args.append(return_arg)
          f = Function(func_name, func_name, func_info, return_type, args,
                       args_for_cmds, cmd_args, init_args, num_pointer_args)
          self.original_functions.append(f)
          gen_cmd = f.GetInfo('gen_cmd')
          if gen_cmd == True or gen_cmd == None:
            self.AddFunction(f)
            f.type_handler.AddImmediateFunction(self, f)
            f.type_handler.AddBucketFunction(self, f)

    self.Log("Auto Generated Functions    : %d" %
             len([f for f in self.functions if f.can_auto_generate or
                  (not f.IsType('') and not f.IsType('Custom') and
                   not f.IsType('Todo'))]))

    funcs = [f for f in self.functions if not f.can_auto_generate and
             (f.IsType('') or f.IsType('Custom') or f.IsType('Todo'))]
    self.Log("Non Auto Generated Functions: %d" % len(funcs))

    for f in funcs:
      self.Log("  %-10s %-20s gl%s" % (f.info.type, f.return_type, f.name))

  def WriteCommandIds(self, filename):
    """Writes the command buffer format"""
    file = CHeaderWriter(filename)
    file.Write("#define GLES2_COMMAND_LIST(OP) \\\n")
    id = 256
    for func in self.functions:
      file.Write("  %-60s /* %d */ \\\n" %
                 ("OP(%s)" % func.name, id))
      id += 1
    file.Write("\n")

    file.Write("enum CommandId {\n")
    file.Write("  kStartPoint = cmd::kLastCommonId,  "
               "// All GLES2 commands start after this.\n")
    file.Write("#define GLES2_CMD_OP(name) k ## name,\n")
    file.Write("  GLES2_COMMAND_LIST(GLES2_CMD_OP)\n")
    file.Write("#undef GLES2_CMD_OP\n")
    file.Write("  kNumCommands\n")
    file.Write("};\n")
    file.Write("\n")
    file.Close()

  def WriteFormat(self, filename):
    """Writes the command buffer format"""
    file = CHeaderWriter(filename)
    for func in self.functions:
      if True:
      #gen_cmd = func.GetInfo('gen_cmd')
      #if gen_cmd == True or gen_cmd == None:
        func.WriteStruct(file)
    file.Write("\n")
    file.Close()

  def WriteDocs(self, filename):
    """Writes the command buffer doc version of the commands"""
    file = CWriter(filename)
    for func in self.functions:
      if True:
      #gen_cmd = func.GetInfo('gen_cmd')
      #if gen_cmd == True or gen_cmd == None:
        func.WriteDocs(file)
    file.Write("\n")
    file.Close()

  def WriteFormatTest(self, filename):
    """Writes the command buffer format test."""
    file = CHeaderWriter(
      filename,
      "// This file contains unit tests for gles2 commmands\n"
      "// It is included by gles2_cmd_format_test.cc\n"
      "\n")

    for func in self.functions:
      if True:
      #gen_cmd = func.GetInfo('gen_cmd')
      #if gen_cmd == True or gen_cmd == None:
        func.WriteFormatTest(file)

    file.Close()

  def WriteCmdHelperHeader(self, filename):
    """Writes the gles2 command helper."""
    file = CHeaderWriter(filename)

    for func in self.functions:
      if True:
      #gen_cmd = func.GetInfo('gen_cmd')
      #if gen_cmd == True or gen_cmd == None:
        func.WriteCmdHelper(file)

    file.Close()

  def WriteServiceImplementation(self, filename):
    """Writes the service decorder implementation."""
    file = CHeaderWriter(
        filename,
        "// It is included by gles2_cmd_decoder.cc\n")

    for func in self.functions:
      if True:
      #gen_cmd = func.GetInfo('gen_cmd')
      #if gen_cmd == True or gen_cmd == None:
        func.WriteServiceImplementation(file)

    file.Close()

  def WriteServiceUnitTests(self, filename):
    """Writes the service decorder unit tests."""
    num_tests = len(self.functions)
    FUNCTIONS_PER_FILE = 98  # hard code this so it doesn't change.
    count = 0
    for test_num in range(0, num_tests, FUNCTIONS_PER_FILE):
      count += 1
      name = filename % count
      file = CHeaderWriter(
          name,
          "// It is included by gles2_cmd_decoder_unittest_%d.cc\n" % count)
      file.SetFileNum(count)
      end = test_num + FUNCTIONS_PER_FILE
      if end > num_tests:
        end = num_tests
      for idx in range(test_num, end):
        func = self.functions[idx]
        if True:
        #gen_cmd = func.GetInfo('gen_cmd')
        #if gen_cmd == True or gen_cmd == None:
          if func.GetInfo('unit_test') == False:
            file.Write("// TODO(gman): %s\n" % func.name)
          else:
            func.WriteServiceUnitTest(file)

      file.Close()


  def WriteGLES2CLibImplementation(self, filename):
    """Writes the GLES2 c lib implementation."""
    file = CHeaderWriter(
        filename,
        "// These functions emulate GLES2 over command buffers.\n")

    for func in self.original_functions:
      func.WriteGLES2CLibImplementation(file)
    file.Write("\n")

    file.Close()

  def WriteGLES2ImplementationHeader(self, filename):
    """Writes the GLES2 helper header."""
    file = CHeaderWriter(
        filename,
        "// This file is included by gles2_implementation.h to declare the\n"
        "// GL api functions.\n")
    for func in self.original_functions:
      func.WriteGLES2ImplementationHeader(file)
    file.Close()

  def WriteGLES2ImplementationUnitTests(self, filename):
    """Writes the GLES2 helper header."""
    file = CHeaderWriter(
        filename,
        "// This file is included by gles2_implementation.h to declare the\n"
        "// GL api functions.\n")
    for func in self.original_functions:
      func.WriteGLES2ImplementationUnitTest(file)
    file.Close()

  def WriteServiceUtilsHeader(self, filename):
    """Writes the gles2 auto generated utility header."""
    file = CHeaderWriter(filename)
    for enum in sorted(_ENUM_LISTS.keys()):
      file.Write("ValueValidator<%s> %s;\n" %
                 (_ENUM_LISTS[enum]['type'], ToUnderscore(enum)))
    file.Write("\n")
    file.Close()

  def WriteServiceUtilsImplementation(self, filename):
    """Writes the gles2 auto generated utility implementation."""
    file = CHeaderWriter(filename)
    enums = sorted(_ENUM_LISTS.keys())
    for enum in enums:
      if len(_ENUM_LISTS[enum]['valid']) > 0:
        file.Write("static %s valid_%s_table[] = {\n" %
                   (_ENUM_LISTS[enum]['type'], ToUnderscore(enum)))
        for value in _ENUM_LISTS[enum]['valid']:
          file.Write("  %s,\n" % value)
        file.Write("};\n")
        file.Write("\n")
    file.Write("Validators::Validators()\n")
    pre = ': '
    post = ','
    count = 0
    for enum in enums:
      count += 1
      if count == len(enums):
        post = ' {'
      if len(_ENUM_LISTS[enum]['valid']) > 0:
        code = """    %(pre)s%(name)s(
          valid_%(name)s_table, arraysize(valid_%(name)s_table))%(post)s
"""
      else:
        code = """    %(pre)s%(name)s()%(post)s
"""
      file.Write(code % {
          'name': ToUnderscore(enum),
          'pre': pre,
          'post': post,
        })
      pre = '  '
    file.Write("}\n\n");
    file.Close()

  def WriteCommonUtilsHeader(self, filename):
    """Writes the gles2 common utility header."""
    file = CHeaderWriter(filename)
    enums = sorted(_ENUM_LISTS.keys())
    for enum in enums:
      if _ENUM_LISTS[enum]['type'] == 'GLenum':
        file.Write("static std::string GetString%s(uint32 value);\n" % enum)
    file.Write("\n")
    file.Close()

  def WriteCommonUtilsImpl(self, filename):
    """Writes the gles2 common utility header."""
    enum_re = re.compile(r'\#define\s+(GL_[a-zA-Z0-9_]+)\s+([0-9A-Fa-fx]+)')
    dict = {}
    for fname in ['../../third_party/khronos/GLES2/gl2.h',
                  '../../third_party/khronos/GLES2/gl2ext.h']:
      lines = open(fname).readlines()
      for line in lines:
        m = enum_re.match(line)
        if m:
          name = m.group(1)
          value = m.group(2)
          if not value in dict:
            dict[value] = name

    file = CHeaderWriter(filename)
    file.Write("static GLES2Util::EnumToString enum_to_string_table[] = {\n")
    for value in dict:
      file.Write('  { %s, "%s", },\n' % (value, dict[value]))
    file.Write("""};

const GLES2Util::EnumToString* GLES2Util::enum_to_string_table_ =
    enum_to_string_table;
const size_t GLES2Util::enum_to_string_table_len_ =
    sizeof(enum_to_string_table) / sizeof(enum_to_string_table[0]);

""")

    enums = sorted(_ENUM_LISTS.keys())
    for enum in enums:
      if _ENUM_LISTS[enum]['type'] == 'GLenum':
        file.Write("std::string GLES2Util::GetString%s(uint32 value) {\n" %
                   enum)
        if len(_ENUM_LISTS[enum]['valid']) > 0:
          file.Write("  static EnumToString string_table[] = {\n")
          for value in _ENUM_LISTS[enum]['valid']:
            file.Write('    { %s, "%s" },\n' % (value, value))
          file.Write("""  };
  return GLES2Util::GetQualifiedEnumString(
      string_table, arraysize(string_table), value);
}

""")
        else:
          file.Write("""  return GLES2Util::GetQualifiedEnumString(
      NULL, 0, value);
}

""")
    file.Close()

  def WritePepperGLES2Interface(self, filename, dev):
    """Writes the Pepper OpenGLES interface definition."""
    file = CHeaderWriter(
        filename,
        "// OpenGL ES interface.\n",
        2)

    file.Write("#include \"ppapi/c/pp_resource.h\"\n")
    if dev:
      file.Write("#include \"ppapi/c/ppb_opengles2.h\"\n\n")
    else:
      file.Write("\n#ifndef __gl2_h_\n")
      for (k, v) in _GL_TYPES.iteritems():
        file.Write("typedef %s %s;\n" % (v, k))
      file.Write("#endif  // __gl2_h_\n\n")

    for interface in self.pepper_interfaces:
      if interface.dev != dev:
        continue
      file.Write("#define %s_1_0 \"%s;1.0\"\n" %
                 (interface.GetInterfaceName(), interface.GetInterfaceString()))
      file.Write("#define %s %s_1_0\n" %
                 (interface.GetInterfaceName(), interface.GetInterfaceName()))

      file.Write("\nstruct %s {\n" % interface.GetStructName())
      for func in self.original_functions:
        if not func.InPepperInterface(interface):
          continue

        original_arg = func.MakeTypedOriginalArgString("")
        context_arg = "PP_Resource context"
        if len(original_arg):
          arg = context_arg + ", " + original_arg
        else:
          arg = context_arg
        file.Write("  %s (*%s)(%s);\n" % (func.return_type, func.name, arg))
      file.Write("};\n\n")


    file.Close()

  def WritePepperGLES2Implementation(self, filename):
    """Writes the Pepper OpenGLES interface implementation."""

    file = CWriter(filename)
    file.Write(_LICENSE)
    file.Write(_DO_NOT_EDIT_WARNING)

    file.Write("#include \"ppapi/shared_impl/ppb_opengles2_shared.h\"\n\n")
    file.Write("#include \"base/logging.h\"\n")
    file.Write("#include \"gpu/command_buffer/client/gles2_implementation.h\"\n")
    file.Write("#include \"ppapi/shared_impl/ppb_graphics_3d_shared.h\"\n")
    file.Write("#include \"ppapi/thunk/enter.h\"\n\n")

    file.Write("namespace ppapi {\n\n")
    file.Write("namespace {\n\n")

    file.Write("gpu::gles2::GLES2Implementation*"
               " GetGLES(PP_Resource context) {\n")
    file.Write("  thunk::EnterResource<thunk::PPB_Graphics3D_API>"
               " enter_g3d(context, false);\n")
    file.Write("  DCHECK(enter_g3d.succeeded());\n")
    file.Write("  return static_cast<PPB_Graphics3D_Shared*>"
               "(enter_g3d.object())->gles2_impl();\n")
    file.Write("}\n\n")

    for func in self.original_functions:
      if not func.InAnyPepperExtension():
        continue

      original_arg = func.MakeTypedOriginalArgString("")
      context_arg = "PP_Resource context_id"
      if len(original_arg):
        arg = context_arg + ", " + original_arg
      else:
        arg = context_arg
      file.Write("%s %s(%s) {\n" % (func.return_type, func.name, arg))

      return_str = "" if func.return_type == "void" else "return "
      file.Write("  %sGetGLES(context_id)->%s(%s);\n" %
                 (return_str, func.original_name,
                  func.MakeOriginalArgString("")))
      file.Write("}\n\n")

    file.Write("}  // namespace\n")

    for interface in self.pepper_interfaces:
      file.Write("const %s* PPB_OpenGLES2_Shared::Get%sInterface() {\n" %
                 (interface.GetStructName(), interface.GetName()))
      file.Write("  static const struct %s "
                 "ppb_opengles2 = {\n" % interface.GetStructName())
      file.Write("    &")
      file.Write(",\n    &".join(
        f.name for f in self.original_functions
          if f.InPepperInterface(interface)))
      file.Write("\n")

      file.Write("  };\n")
      file.Write("  return &ppb_opengles2;\n")
      file.Write("}\n")

    file.Write("}  // namespace ppapi\n")
    file.Close()

  def WriteGLES2ToPPAPIBridge(self, filename):
    """Connects GLES2 helper library to PPB_OpenGLES2 interface"""

    file = CWriter(filename)
    file.Write(_LICENSE)
    file.Write(_DO_NOT_EDIT_WARNING)

    file.Write("#include <GLES2/gl2.h>\n")
    file.Write("#include <GLES2/gl2ext.h>\n")
    file.Write("#include \"ppapi/lib/gl/gles2/gl2ext_ppapi.h\"\n\n")

    for func in self.original_functions:
      if not func.InAnyPepperExtension():
        continue

      interface = self.interface_info[func.GetInfo('pepper_interface') or '']

      file.Write("%s GL_APIENTRY gl%s(%s) {\n" %
                 (func.return_type, func.name,
                  func.MakeTypedOriginalArgString("")))
      return_str = "" if func.return_type == "void" else "return "
      interface_str = "glGet%sInterfacePPAPI()" % interface.GetName()
      original_arg = func.MakeOriginalArgString("")
      context_arg = "glGetCurrentContextPPAPI()"
      if len(original_arg):
        arg = context_arg + ", " + original_arg
      else:
        arg = context_arg
      if interface.GetName():
        file.Write("  const struct %s* ext = %s;\n" %
                   (interface.GetStructName(), interface_str))
        file.Write("  if (ext)\n")
        file.Write("    %sext->%s(%s);\n" %
                   (return_str, func.name, arg))
        if return_str:
          file.Write("  %s0;\n" % return_str)
      else:
        file.Write("  %s%s->%s(%s);\n" %
                   (return_str, interface_str, func.name, arg))
      file.Write("}\n\n")
    file.Close()

  def WritePepperGLES2NaClProxy(self, filename):
    """Writes the Pepper OpenGLES interface implementation for NaCl."""
    file = CWriter(filename)
    file.Write(_LICENSE)
    file.Write(_DO_NOT_EDIT_WARNING)

    file.Write("#include \"native_client/src/shared/ppapi_proxy"
        "/plugin_ppb_graphics_3d.h\"\n\n")

    file.Write("#include \"gpu/command_buffer/client/gles2_implementation.h\"")
    file.Write("\n#include \"ppapi/c/ppb_opengles2.h\"\n\n")

    file.Write("using ppapi_proxy::PluginGraphics3D;\n")
    file.Write("using ppapi_proxy::PluginResource;\n\n")
    file.Write("namespace {\n\n")

    for func in self.original_functions:
      if not func.InAnyPepperExtension():
        continue
      args = func.MakeTypedOriginalArgString("")
      if len(args) != 0:
        args = ", " + args
      file.Write("%s %s(PP_Resource context%s) {\n" %
                 (func.return_type, func.name, args))
      return_string = "return "
      if func.return_type == "void":
        return_string = ""
      file.Write("  %sPluginGraphics3D::implFromResource(context)->"
                 "%s(%s);\n" %
                 (return_string,
                  func.original_name,
                  func.MakeOriginalArgString("")))
      file.Write("}\n")

    file.Write("\n} // namespace\n\n")

    for interface in self.pepper_interfaces:
      file.Write("const %s* "
                 "PluginGraphics3D::GetOpenGLES%sInterface() {\n" %
                 (interface.GetStructName(), interface.GetName()))

      file.Write("  const static struct %s ppb_opengles = {\n" %
                 interface.GetStructName())
      file.Write("    &")
      file.Write(",\n    &".join(
        f.name for f in self.original_functions
          if f.InPepperInterface(interface)))
      file.Write("\n")
      file.Write("  };\n")
      file.Write("  return &ppb_opengles;\n")
      file.Write("}\n")
    file.Close()


def main(argv):
  """This is the main function."""
  parser = OptionParser()
  parser.add_option(
      "-g", "--generate-implementation-templates", action="store_true",
      help="generates files that are generally hand edited..")
  parser.add_option(
      "--alternate-mode", type="choice",
      choices=("ppapi", "chrome_ppapi", "chrome_ppapi_proxy", "nacl_ppapi"),
      help="generate files for other projects. \"ppapi\" will generate ppapi "
      "bindings. \"chrome_ppapi\" generate chrome implementation for ppapi. "
      "\"chrome_ppapi_proxy\" will generate the glue for the chrome IPC ppapi"
      "proxy. \"nacl_ppapi\" will generate NaCl implementation for ppapi")
  parser.add_option(
      "--output-dir",
      help="base directory for resulting files, under chrome/src. default is "
      "empty. Use this if you want the result stored under gen.")
  parser.add_option(
      "-v", "--verbose", action="store_true",
      help="prints more output.")

  (options, args) = parser.parse_args(args=argv)

  # This script lives under gpu/command_buffer, cd to base directory.
  os.chdir(os.path.dirname(__file__) + "/../..")

  gen = GLGenerator(options.verbose)
  gen.ParseGLH("common/GLES2/gl2.h")

  # Support generating files under gen/
  if options.output_dir != None:
    os.chdir(options.output_dir)

  if options.alternate_mode == "ppapi":
    # To trigger this action, do "make ppapi_gles_bindings"
    os.chdir("ppapi");
    gen.WritePepperGLES2Interface("c/ppb_opengles2.h", False)
    gen.WritePepperGLES2Interface("c/dev/ppb_opengles2ext_dev.h", True)
    gen.WriteGLES2ToPPAPIBridge("lib/gl/gles2/gles2.c")

  elif options.alternate_mode == "chrome_ppapi":
    # To trigger this action, do "make ppapi_gles_implementation"
    gen.WritePepperGLES2Implementation(
        "ppapi/shared_impl/ppb_opengles2_shared.cc")

  elif options.alternate_mode == "nacl_ppapi":
    os.chdir("ppapi")
    gen.WritePepperGLES2NaClProxy(
        "native_client/src/shared/ppapi_proxy/plugin_opengles.cc")

  else:
    os.chdir("gpu/command_buffer")
    gen.WriteCommandIds("common/gles2_cmd_ids_autogen.h")
    gen.WriteFormat("common/gles2_cmd_format_autogen.h")
    gen.WriteFormatTest("common/gles2_cmd_format_test_autogen.h")
    gen.WriteGLES2ImplementationHeader("client/gles2_implementation_autogen.h")
    gen.WriteGLES2ImplementationUnitTests(
        "client/gles2_implementation_unittest_autogen.h")
    gen.WriteGLES2CLibImplementation("client/gles2_c_lib_autogen.h")
    gen.WriteCmdHelperHeader("client/gles2_cmd_helper_autogen.h")
    gen.WriteServiceImplementation("service/gles2_cmd_decoder_autogen.h")
    gen.WriteServiceUnitTests("service/gles2_cmd_decoder_unittest_%d_autogen.h")
    gen.WriteServiceUtilsHeader("service/gles2_cmd_validation_autogen.h")
    gen.WriteServiceUtilsImplementation(
        "service/gles2_cmd_validation_implementation_autogen.h")
    gen.WriteCommonUtilsHeader("common/gles2_cmd_utils_autogen.h")
    gen.WriteCommonUtilsImpl("common/gles2_cmd_utils_implementation_autogen.h")

  if gen.errors > 0:
    print "%d errors" % gen.errors
    return 1
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
