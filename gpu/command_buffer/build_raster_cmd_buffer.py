#!/usr/bin/env python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""code generator for raster command buffers."""

import os
import os.path
import sys
from optparse import OptionParser

import build_cmd_buffer_lib

# Empty flags because raster interface does not support glEnable
_CAPABILITY_FLAGS = []

_STATE_INFO = {}

# TODO(backer): Figure out which of these enums are actually valid.
#
# Named type info object represents a named type that is used in OpenGL call
# arguments.  Each named type defines a set of valid OpenGL call arguments.  The
# named types are used in 'raster_cmd_buffer_functions.txt'.
# type: The actual GL type of the named type.
# valid: The list of values that are valid for both the client and the service.
# valid_es3: The list of values that are valid in OpenGL ES 3, but not ES 2.
# invalid: Examples of invalid values for the type. At least these values
#          should be tested to be invalid.
# deprecated_es3: The list of values that are valid in OpenGL ES 2, but
#                 deprecated in ES 3.
# is_complete: The list of valid values of type are final and will not be
#              modified during runtime.
# validator: If set to False will prevent creation of a ValueValidator. Values
#            are still expected to be checked for validity and will be tested.
_NAMED_TYPE_INFO = {
  'CompressedTextureFormat': {
    'type': 'GLenum',
    'valid': [
    ],
    'valid_es3': [
    ],
  },
  'GLState': {
    'type': 'GLenum',
    'valid': [
      # NOTE: State an Capability entries added later.
      'GL_ACTIVE_TEXTURE',
      'GL_ALIASED_LINE_WIDTH_RANGE',
      'GL_ALIASED_POINT_SIZE_RANGE',
      'GL_ALPHA_BITS',
      'GL_ARRAY_BUFFER_BINDING',
      'GL_BLUE_BITS',
      'GL_COMPRESSED_TEXTURE_FORMATS',
      'GL_CURRENT_PROGRAM',
      'GL_DEPTH_BITS',
      'GL_DEPTH_RANGE',
      'GL_ELEMENT_ARRAY_BUFFER_BINDING',
      'GL_FRAMEBUFFER_BINDING',
      'GL_GENERATE_MIPMAP_HINT',
      'GL_GREEN_BITS',
      'GL_IMPLEMENTATION_COLOR_READ_FORMAT',
      'GL_IMPLEMENTATION_COLOR_READ_TYPE',
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
      'GL_RED_BITS',
      'GL_RENDERBUFFER_BINDING',
      'GL_SAMPLE_BUFFERS',
      'GL_SAMPLE_COVERAGE_INVERT',
      'GL_SAMPLE_COVERAGE_VALUE',
      'GL_SAMPLES',
      'GL_SCISSOR_BOX',
      'GL_SHADER_BINARY_FORMATS',
      'GL_SHADER_COMPILER',
      'GL_SUBPIXEL_BITS',
      'GL_STENCIL_BITS',
      'GL_TEXTURE_BINDING_2D',
      'GL_TEXTURE_BINDING_CUBE_MAP',
      'GL_UNPACK_ALIGNMENT',
      'GL_BIND_GENERATES_RESOURCE_CHROMIUM',
      # we can add this because we emulate it if the driver does not support it.
      'GL_VERTEX_ARRAY_BINDING_OES',
      'GL_VIEWPORT',
    ],
    'valid_es3': [
      'GL_COPY_READ_BUFFER_BINDING',
      'GL_COPY_WRITE_BUFFER_BINDING',
      'GL_DRAW_BUFFER0',
      'GL_DRAW_BUFFER1',
      'GL_DRAW_BUFFER2',
      'GL_DRAW_BUFFER3',
      'GL_DRAW_BUFFER4',
      'GL_DRAW_BUFFER5',
      'GL_DRAW_BUFFER6',
      'GL_DRAW_BUFFER7',
      'GL_DRAW_BUFFER8',
      'GL_DRAW_BUFFER9',
      'GL_DRAW_BUFFER10',
      'GL_DRAW_BUFFER11',
      'GL_DRAW_BUFFER12',
      'GL_DRAW_BUFFER13',
      'GL_DRAW_BUFFER14',
      'GL_DRAW_BUFFER15',
      'GL_DRAW_FRAMEBUFFER_BINDING',
      'GL_FRAGMENT_SHADER_DERIVATIVE_HINT',
      'GL_GPU_DISJOINT_EXT',
      'GL_MAJOR_VERSION',
      'GL_MAX_3D_TEXTURE_SIZE',
      'GL_MAX_ARRAY_TEXTURE_LAYERS',
      'GL_MAX_COLOR_ATTACHMENTS',
      'GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS',
      'GL_MAX_COMBINED_UNIFORM_BLOCKS',
      'GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS',
      'GL_MAX_DRAW_BUFFERS',
      'GL_MAX_ELEMENT_INDEX',
      'GL_MAX_ELEMENTS_INDICES',
      'GL_MAX_ELEMENTS_VERTICES',
      'GL_MAX_FRAGMENT_INPUT_COMPONENTS',
      'GL_MAX_FRAGMENT_UNIFORM_BLOCKS',
      'GL_MAX_FRAGMENT_UNIFORM_COMPONENTS',
      'GL_MAX_PROGRAM_TEXEL_OFFSET',
      'GL_MAX_SAMPLES',
      'GL_MAX_SERVER_WAIT_TIMEOUT',
      'GL_MAX_TEXTURE_LOD_BIAS',
      'GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS',
      'GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS',
      'GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS',
      'GL_MAX_UNIFORM_BLOCK_SIZE',
      'GL_MAX_UNIFORM_BUFFER_BINDINGS',
      'GL_MAX_VARYING_COMPONENTS',
      'GL_MAX_VERTEX_OUTPUT_COMPONENTS',
      'GL_MAX_VERTEX_UNIFORM_BLOCKS',
      'GL_MAX_VERTEX_UNIFORM_COMPONENTS',
      'GL_MIN_PROGRAM_TEXEL_OFFSET',
      'GL_MINOR_VERSION',
      'GL_NUM_EXTENSIONS',
      'GL_NUM_PROGRAM_BINARY_FORMATS',
      'GL_PACK_ROW_LENGTH',
      'GL_PACK_SKIP_PIXELS',
      'GL_PACK_SKIP_ROWS',
      'GL_PIXEL_PACK_BUFFER_BINDING',
      'GL_PIXEL_UNPACK_BUFFER_BINDING',
      'GL_PROGRAM_BINARY_FORMATS',
      'GL_READ_BUFFER',
      'GL_READ_FRAMEBUFFER_BINDING',
      'GL_SAMPLER_BINDING',
      'GL_TIMESTAMP_EXT',
      'GL_TEXTURE_BINDING_2D_ARRAY',
      'GL_TEXTURE_BINDING_3D',
      'GL_TRANSFORM_FEEDBACK_BINDING',
      'GL_TRANSFORM_FEEDBACK_ACTIVE',
      'GL_TRANSFORM_FEEDBACK_BUFFER_BINDING',
      'GL_TRANSFORM_FEEDBACK_PAUSED',
      'GL_TRANSFORM_FEEDBACK_BUFFER_SIZE',
      'GL_TRANSFORM_FEEDBACK_BUFFER_START',
      'GL_UNIFORM_BUFFER_BINDING',
      'GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT',
      'GL_UNIFORM_BUFFER_SIZE',
      'GL_UNIFORM_BUFFER_START',
      'GL_UNPACK_IMAGE_HEIGHT',
      'GL_UNPACK_ROW_LENGTH',
      'GL_UNPACK_SKIP_IMAGES',
      'GL_UNPACK_SKIP_PIXELS',
      'GL_UNPACK_SKIP_ROWS',
      # GL_VERTEX_ARRAY_BINDING is the same as GL_VERTEX_ARRAY_BINDING_OES
      # 'GL_VERTEX_ARRAY_BINDING',
    ],
    'invalid': [
      'GL_FOG_HINT',
    ],
  },
  'BufferUsage': {
    'type': 'GLenum',
    'is_complete': True,
    'valid': [
      'GL_STREAM_DRAW',
      'GL_STATIC_DRAW',
      'GL_DYNAMIC_DRAW',
    ],
    'valid_es3': [
      'GL_STREAM_READ',
      'GL_STREAM_COPY',
      'GL_STATIC_READ',
      'GL_STATIC_COPY',
      'GL_DYNAMIC_READ',
      'GL_DYNAMIC_COPY',
    ],
    'invalid': [
      'GL_NONE',
    ],
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
    'valid_es3': [
      'GL_TEXTURE_3D',
      'GL_TEXTURE_2D_ARRAY',
    ],
    'invalid': [
      'GL_TEXTURE_1D',
      'GL_TEXTURE_3D',
    ],
  },
  'QueryObjectParameter': {
    'type': 'GLenum',
    'is_complete': True,
    'valid': [
      'GL_QUERY_RESULT_EXT',
      'GL_QUERY_RESULT_AVAILABLE_EXT',
    ],
  },
  'QueryTarget': {
    'type': 'GLenum',
    'is_complete': True,
    'valid': [
      'GL_SAMPLES_PASSED_ARB',
      'GL_ANY_SAMPLES_PASSED_EXT',
      'GL_ANY_SAMPLES_PASSED_CONSERVATIVE_EXT',
      'GL_COMMANDS_ISSUED_CHROMIUM',
      'GL_LATENCY_QUERY_CHROMIUM',
      'GL_ASYNC_PIXEL_PACK_COMPLETED_CHROMIUM',
      'GL_COMMANDS_COMPLETED_CHROMIUM',
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
    'valid_es3': [
      'GL_TEXTURE_BASE_LEVEL',
      'GL_TEXTURE_COMPARE_FUNC',
      'GL_TEXTURE_COMPARE_MODE',
      'GL_TEXTURE_IMMUTABLE_FORMAT',
      'GL_TEXTURE_IMMUTABLE_LEVELS',
      'GL_TEXTURE_MAX_LEVEL',
      'GL_TEXTURE_MAX_LOD',
      'GL_TEXTURE_MIN_LOD',
      'GL_TEXTURE_WRAP_R',
    ],
    'invalid': [
      'GL_GENERATE_MIPMAP',
    ],
  },
  'PixelStore': {
    'type': 'GLenum',
    'valid': [
      'GL_PACK_ALIGNMENT',
      'GL_UNPACK_ALIGNMENT',
    ],
    'valid_es3': [
      'GL_PACK_ROW_LENGTH',
      'GL_PACK_SKIP_PIXELS',
      'GL_PACK_SKIP_ROWS',
      'GL_UNPACK_ROW_LENGTH',
      'GL_UNPACK_IMAGE_HEIGHT',
      'GL_UNPACK_SKIP_PIXELS',
      'GL_UNPACK_SKIP_ROWS',
      'GL_UNPACK_SKIP_IMAGES',
    ],
    'invalid': [
      'GL_PACK_SWAP_BYTES',
      'GL_UNPACK_SWAP_BYTES',
    ],
  },
  'PixelStoreAlignment': {
    'type': 'GLint',
    'is_complete': True,
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
  'PixelType': {
    'type': 'GLenum',
    'valid': [
      'GL_UNSIGNED_BYTE',
      'GL_UNSIGNED_SHORT_5_6_5',
      'GL_UNSIGNED_SHORT_4_4_4_4',
      'GL_UNSIGNED_SHORT_5_5_5_1',
    ],
    'valid_es3': [
      'GL_BYTE',
      'GL_UNSIGNED_SHORT',
      'GL_SHORT',
      'GL_UNSIGNED_INT',
      'GL_INT',
      'GL_HALF_FLOAT',
      'GL_FLOAT',
      'GL_UNSIGNED_INT_2_10_10_10_REV',
      'GL_UNSIGNED_INT_10F_11F_11F_REV',
      'GL_UNSIGNED_INT_5_9_9_9_REV',
      'GL_UNSIGNED_INT_24_8',
      'GL_FLOAT_32_UNSIGNED_INT_24_8_REV',
    ],
    'invalid': [
      'GL_UNSIGNED_BYTE_3_3_2',
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
    'valid_es3': [
      'GL_RED',
      'GL_RED_INTEGER',
      'GL_RG',
      'GL_RG_INTEGER',
      'GL_RGB_INTEGER',
      'GL_RGBA_INTEGER',
      'GL_DEPTH_COMPONENT',
      'GL_DEPTH_STENCIL',
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
    'valid_es3': [
      'GL_R8',
      'GL_R8_SNORM',
      'GL_R16F',
      'GL_R32F',
      'GL_R8UI',
      'GL_R8I',
      'GL_R16UI',
      'GL_R16I',
      'GL_R32UI',
      'GL_R32I',
      'GL_RG8',
      'GL_RG8_SNORM',
      'GL_RG16F',
      'GL_RG32F',
      'GL_RG8UI',
      'GL_RG8I',
      'GL_RG16UI',
      'GL_RG16I',
      'GL_RG32UI',
      'GL_RG32I',
      'GL_RGB8',
      'GL_SRGB8',
      'GL_RGB565',
      'GL_RGB8_SNORM',
      'GL_R11F_G11F_B10F',
      'GL_RGB9_E5',
      'GL_RGB16F',
      'GL_RGB32F',
      'GL_RGB8UI',
      'GL_RGB8I',
      'GL_RGB16UI',
      'GL_RGB16I',
      'GL_RGB32UI',
      'GL_RGB32I',
      'GL_RGBA8',
      'GL_SRGB8_ALPHA8',
      'GL_RGBA8_SNORM',
      'GL_RGB5_A1',
      'GL_RGBA4',
      'GL_RGB10_A2',
      'GL_RGBA16F',
      'GL_RGBA32F',
      'GL_RGBA8UI',
      'GL_RGBA8I',
      'GL_RGB10_A2UI',
      'GL_RGBA16UI',
      'GL_RGBA16I',
      'GL_RGBA32UI',
      'GL_RGBA32I',
      # The DEPTH/STENCIL formats are not supported in CopyTexImage2D.
      # We will reject them dynamically in GPU command buffer.
      'GL_DEPTH_COMPONENT16',
      'GL_DEPTH_COMPONENT24',
      'GL_DEPTH_COMPONENT32F',
      'GL_DEPTH24_STENCIL8',
      'GL_DEPTH32F_STENCIL8',
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
    'valid_es3': [
      'GL_R8',
      'GL_R8_SNORM',
      'GL_R16F',
      'GL_R32F',
      'GL_R8UI',
      'GL_R8I',
      'GL_R16UI',
      'GL_R16I',
      'GL_R32UI',
      'GL_R32I',
      'GL_RG8',
      'GL_RG8_SNORM',
      'GL_RG16F',
      'GL_RG32F',
      'GL_RG8UI',
      'GL_RG8I',
      'GL_RG16UI',
      'GL_RG16I',
      'GL_RG32UI',
      'GL_RG32I',
      'GL_RGB8',
      'GL_SRGB8',
      'GL_RGB8_SNORM',
      'GL_R11F_G11F_B10F',
      'GL_RGB9_E5',
      'GL_RGB16F',
      'GL_RGB32F',
      'GL_RGB8UI',
      'GL_RGB8I',
      'GL_RGB16UI',
      'GL_RGB16I',
      'GL_RGB32UI',
      'GL_RGB32I',
      'GL_RGBA8',
      'GL_SRGB8_ALPHA8',
      'GL_RGBA8_SNORM',
      'GL_RGB10_A2',
      'GL_RGBA16F',
      'GL_RGBA32F',
      'GL_RGBA8UI',
      'GL_RGBA8I',
      'GL_RGB10_A2UI',
      'GL_RGBA16UI',
      'GL_RGBA16I',
      'GL_RGBA32UI',
      'GL_RGBA32I',
      'GL_DEPTH_COMPONENT16',
      'GL_DEPTH_COMPONENT24',
      'GL_DEPTH_COMPONENT32F',
      'GL_DEPTH24_STENCIL8',
      'GL_DEPTH32F_STENCIL8',
    ],
    'deprecated_es3': [
      'GL_ALPHA8_EXT',
      'GL_LUMINANCE8_EXT',
      'GL_LUMINANCE8_ALPHA8_EXT',
      'GL_ALPHA16F_EXT',
      'GL_LUMINANCE16F_EXT',
      'GL_LUMINANCE_ALPHA16F_EXT',
      'GL_ALPHA32F_EXT',
      'GL_LUMINANCE32F_EXT',
      'GL_LUMINANCE_ALPHA32F_EXT',
    ],
  },
  'TextureBorder': {
    'type': 'GLint',
    'is_complete': True,
    'valid': [
      '0',
    ],
    'invalid': [
      '1',
    ],
  },
  'ResetStatus': {
    'type': 'GLenum',
    'is_complete': True,
    'valid': [
      'GL_GUILTY_CONTEXT_RESET_ARB',
      'GL_INNOCENT_CONTEXT_RESET_ARB',
      'GL_UNKNOWN_CONTEXT_RESET_ARB',
    ],
  },
  'ClientBufferUsage': {
    'type': 'GLenum',
    'is_complete': True,
    'valid': [
      'GL_SCANOUT_CHROMIUM',
    ],
    'invalid': [
      'GL_NONE',
    ],
  },
}

# A function info object specifies the type and other special data for the
# command that will be generated. A base function info object is generated by
# parsing the "raster_cmd_buffer_functions.txt", one for each function in the
# file. These function info objects can be augmented and their values can be
# overridden by adding an object to the table below.
#
# Must match function names specified in "raster_cmd_buffer_functions.txt".
#
# cmd_comment:  A comment added to the cmd format.
# type:         defines which handler will be used to generate code.
# decoder_func: defines which function to call in the decoder to execute the
#               corresponding GL command. If not specified the GL command will
#               be called directly.
# gl_test_func: GL function that is expected to be called when testing.
# cmd_args:     The arguments to use for the command. This overrides generating
#               them based on the GL function arguments.
# data_transfer_methods: Array of methods that are used for transfering the
#               pointer data.  Possible values: 'immediate', 'shm', 'bucket'.
#               The default is 'immediate' if the command has one pointer
#               argument, otherwise 'shm'. One command is generated for each
#               transfer method. Affects only commands which are not of type
#               'GETn' or 'GLcharN'.
#               Note: the command arguments that affect this are the final args,
#               taking cmd_args override into consideration.
# impl_func:    Whether or not to generate the GLES2Implementation part of this
#               command.
# internal:     If true, this is an internal command only, not exposed to the
#               client.
# needs_size:   If True a data_size field is added to the command.
# count:        The number of units per element. For PUTn or PUT types.
# use_count_func: If True the actual data count needs to be computed; the count
#               argument specifies the maximum count.
# unit_test:    If False no service side unit test will be generated.
# client_test:  If False no client side unit test will be generated.
# expectation:  If False the unit test will have no expected calls.
# gen_func:     Name of function that generates GL resource for corresponding
#               bind function.
# states:       array of states that get set by this function corresponding to
#               the given arguments
# state_flag:   name of flag that is set to true when function is called.
# no_gl:        no GL function is called.
# valid_args:   A dictionary of argument indices to args to use in unit tests
#               when they can not be automatically determined.
# pepper_interface: The pepper interface that is used for this extension
# pepper_name:  The name of the function as exposed to pepper.
# pepper_args:  A string representing the argument list (what would appear in
#               C/C++ between the parentheses for the function declaration)
#               that the Pepper API expects for this function. Use this only if
#               the stable Pepper API differs from the GLES2 argument list.
# invalid_test: False if no invalid test needed.
# shadowed:     True = the value is shadowed so no glGetXXX call will be made.
# first_element_only: For PUT types, True if only the first element of an
#               array is used and we end up calling the single value
#               corresponding function. eg. TexParameteriv -> TexParameteri
# extension:    Function is an extension to GL and should not be exposed to
#               pepper unless pepper_interface is defined.
# extension_flag: Function is an extension and should be enabled only when
#               the corresponding feature info flag is enabled. Implies
#               'extension': True.
# not_shared:   For GENn types, True if objects can't be shared between contexts
# es3:          ES3 API. True if the function requires an ES3 or WebGL2 context.

_FUNCTION_INFO = {
  # TODO(backer): Kept for unittests remove once new raster API implemented.
 'BindTexture': {
    'type': 'Bind',
    'internal' : True,
    'decoder_func': 'DoBindTexture',
    'gen_func': 'GenTextures',
    # TODO: remove this once client side caching works.
    'client_test': False,
    'unit_test': False,
    'trace_level': 2,
  },
  'CompressedTexSubImage2D': {
    'type': 'Custom',
    'data_transfer_methods': ['bucket', 'shm'],
    'trace_level': 1,
  },
  'CopyTexImage2D': {
    'decoder_func': 'DoCopyTexImage2D',
    'unit_test': False,
    'trace_level': 1,
  },
  'CopyTexSubImage2D': {
    'decoder_func': 'DoCopyTexSubImage2D',
    'trace_level': 1,
  },
  'CreateImageCHROMIUM': {
    'type': 'NoCommand',
    'cmd_args':
        'ClientBuffer buffer, GLsizei width, GLsizei height, '
        'GLenum internalformat',
    'result': ['GLuint'],
    'extension': "CHROMIUM_image",
    'trace_level': 1,
  },
  'DestroyImageCHROMIUM': {
    'type': 'NoCommand',
    'extension': "CHROMIUM_image",
    'trace_level': 1,
  },
  'DeleteTextures': {
    'type': 'DELn',
    'resource_type': 'Texture',
    'resource_types': 'Textures',
  },
  'Finish': {
    'impl_func': False,
    'client_test': False,
    'decoder_func': 'DoFinish',
    'trace_level': 1,
  },
  'Flush': {
    'impl_func': False,
    'decoder_func': 'DoFlush',
    'trace_level': 1,
  },
  # TODO(backer): Kept for unittests remove once new raster API implemented.
  'GenTextures': {
    'type': 'GENn',
    'internal' : True,
    'gl_test_func': 'glGenTextures',
    'resource_type': 'Texture',
    'resource_types': 'Textures',
  },
  'GetError': {
    'type': 'Is',
    'decoder_func': 'GetErrorState()->GetGLError',
    'impl_func': False,
    'result': ['GLenum'],
    'client_test': False,
  },
  'GetGraphicsResetStatusKHR': {
    'type': 'NoCommand',
    'extension': True,
    'trace_level': 1,
  },
  'GetIntegerv': {
    'type': 'GETn',
    'result': ['SizedResult<GLint>'],
    'decoder_func': 'DoGetIntegerv',
    'client_test': False,
  },
  'TexParameteri': {
    'decoder_func': 'DoTexParameteri',
    'valid_args': {
      '2': 'GL_NEAREST'
    },
  },
  'WaitSync': {
    'type': 'Custom',
    'cmd_args': 'GLuint sync, GLbitfieldSyncFlushFlags flags, '
                'GLuint64 timeout',
    'impl_func': False,
    'client_test': False,
    'es3': True,
    'trace_level': 1,
  },
  'CompressedCopyTextureCHROMIUM': {
    'decoder_func': 'DoCompressedCopyTextureCHROMIUM',
    'unit_test': False,
    'extension': 'CHROMIUM_copy_compressed_texture',
  },
  'GenQueriesEXT': {
    'type': 'GENn',
    'gl_test_func': 'glGenQueriesARB',
    'resource_type': 'Query',
    'resource_types': 'Queries',
    'unit_test': False,
    'pepper_interface': 'Query',
    'not_shared': 'True',
    'extension': "occlusion_query_EXT",
  },
  'DeleteQueriesEXT': {
    'type': 'DELn',
    'gl_test_func': 'glDeleteQueriesARB',
    'resource_type': 'Query',
    'resource_types': 'Queries',
    'unit_test': False,
    'pepper_interface': 'Query',
    'extension': "occlusion_query_EXT",
  },
  'BeginQueryEXT': {
    'type': 'Custom',
    'impl_func': False,
    'cmd_args': 'GLenumQueryTarget target, GLidQuery id, void* sync_data',
    'data_transfer_methods': ['shm'],
    'gl_test_func': 'glBeginQuery',
    'pepper_interface': 'Query',
    'extension': "occlusion_query_EXT",
  },
  'EndQueryEXT': {
    'type': 'Custom',
    'impl_func': False,
    'cmd_args': 'GLenumQueryTarget target, GLuint submit_count',
    'gl_test_func': 'glEndnQuery',
    'client_test': False,
    'pepper_interface': 'Query',
    'extension': "occlusion_query_EXT",
  },
  'GetQueryObjectuivEXT': {
    'type': 'NoCommand',
    'gl_test_func': 'glGetQueryObjectuiv',
    'pepper_interface': 'Query',
    'extension': "occlusion_query_EXT",
  },
  'ShallowFlushCHROMIUM': {
    'type': 'NoCommand',
    'extension': 'CHROMIUM_ordering_barrier',
  },
  'OrderingBarrierCHROMIUM': {
    'type': 'NoCommand',
    'extension': 'CHROMIUM_ordering_barrier',
  },
  'LoseContextCHROMIUM': {
    'decoder_func': 'DoLoseContextCHROMIUM',
    'unit_test': False,
    'extension': 'CHROMIUM_lose_context',
    'trace_level': 1,
  },
  'GenSyncTokenCHROMIUM': {
    'type': 'NoCommand',
    'extension': "CHROMIUM_sync_point",
  },
  'GenUnverifiedSyncTokenCHROMIUM': {
    'type': 'NoCommand',
    'extension': "CHROMIUM_sync_point",
  },
  'VerifySyncTokensCHROMIUM' : {
    'type': 'NoCommand',
    'extension': "CHROMIUM_sync_point",
  },
  'WaitSyncTokenCHROMIUM': {
    'type': 'Custom',
    'impl_func': False,
    'cmd_args': 'GLint namespace_id, '
                'GLuint64 command_buffer_id, '
                'GLuint64 release_count',
    'client_test': False,
    'extension': "CHROMIUM_sync_point",
  },
  'InitializeDiscardableTextureCHROMIUM': {
    'type': 'Custom',
    'cmd_args': 'GLuint texture_id, uint32_t shm_id, '
                'uint32_t shm_offset',
    'impl_func': False,
    'client_test': False,
    'extension': True,
  },
  'UnlockDiscardableTextureCHROMIUM': {
    'type': 'Custom',
    'cmd_args': 'GLuint texture_id',
    'impl_func': False,
    'client_test': False,
    'extension': True,
  },
  'LockDiscardableTextureCHROMIUM': {
    'type': 'Custom',
    'cmd_args': 'GLuint texture_id',
    'impl_func': False,
    'client_test': False,
    'extension': True,
  },
  'BeginRasterCHROMIUM': {
    'decoder_func': 'DoBeginRasterCHROMIUM',
    'internal': True,
    'impl_func': False,
    'unit_test': False,
    'extension': 'CHROMIUM_raster_transport',
    'extension_flag': 'chromium_raster_transport',
  },
  'RasterCHROMIUM': {
    'type': 'Data',
    'internal': True,
    'decoder_func': 'DoRasterCHROMIUM',
    'data_transfer_methods': ['shm'],
    'extension': 'CHROMIUM_raster_transport',
    'extension_flag': 'chromium_raster_transport',
  },
  'EndRasterCHROMIUM': {
    'decoder_func': 'DoEndRasterCHROMIUM',
    'impl_func': True,
    'unit_test': False,
    'extension': 'CHROMIUM_raster_transport',
    'extension_flag': 'chromium_raster_transport',
  },
  'CreateTransferCacheEntryINTERNAL': {
    'decoder_func': 'DoCreateTransferCacheEntryINTERNAL',
    'cmd_args': 'GLuint entry_type, GLuint entry_id, GLuint handle_shm_id, '
                'GLuint handle_shm_offset, GLuint data_shm_id, '
                'GLuint data_shm_offset, GLuint data_size',
    'internal': True,
    'impl_func': True,
    'client_test': False,
    'unit_test': False,
    'extension': True,
  },
  'DeleteTransferCacheEntryINTERNAL': {
    'decoder_func': 'DoDeleteTransferCacheEntryINTERNAL',
    'cmd_args': 'GLuint entry_type, GLuint entry_id',
    'internal': True,
    'impl_func': True,
    'client_test': False,
    'unit_test': False,
    'extension': True,
  },
  'UnlockTransferCacheEntryINTERNAL': {
    'decoder_func': 'DoUnlockTransferCacheEntryINTERNAL',
    'cmd_args': 'GLuint entry_type, GLuint entry_id',
    'internal': True,
    'impl_func': True,
    'client_test': False,
    'unit_test': False,
    'extension': True,
  },
}


def main(argv):
  """This is the main function."""
  parser = OptionParser()
  parser.add_option(
      "--output-dir",
      help="base directory for resulting files, under chrome/src. default is "
      "empty. Use this if you want the result stored under gen.")
  parser.add_option(
      "-v", "--verbose", action="store_true",
      help="prints more output.")

  (options, _) = parser.parse_args(args=argv)

  # This script lives under gpu/command_buffer, cd to base directory.
  os.chdir(os.path.dirname(__file__) + "/../..")
  base_dir = os.getcwd()
  build_cmd_buffer_lib.InitializePrefix("Raster")
  gen = build_cmd_buffer_lib.GLGenerator(options.verbose, "2018",
                                         _FUNCTION_INFO, _NAMED_TYPE_INFO,
                                         _STATE_INFO, _CAPABILITY_FLAGS)
  gen.ParseGLH("gpu/command_buffer/raster_cmd_buffer_functions.txt")

  # Support generating files under gen/
  if options.output_dir != None:
    os.chdir(options.output_dir)

  os.chdir(base_dir)

  # TODO(backer): Uncomment once the output looks good.
  gen.WriteCommandIds("gpu/command_buffer/common/raster_cmd_ids_autogen.h")
  gen.WriteFormat("gpu/command_buffer/common/raster_cmd_format_autogen.h")
  gen.WriteFormatTest(
    "gpu/command_buffer/common/raster_cmd_format_test_autogen.h")
  gen.WriteGLES2InterfaceHeader(
    "gpu/command_buffer/client/raster_interface_autogen.h")
  # gen.WriteGLES2InterfaceStub(
  #   "gpu/command_buffer/client/raster_interface_stub_autogen.h")
  # gen.WriteGLES2InterfaceStubImpl(
  #     "gpu/command_buffer/client/raster_interface_stub_impl_autogen.h")
  # gen.WriteGLES2ImplementationHeader(
  #   "gpu/command_buffer/client/raster_implementation_autogen.h")
  # gen.WriteGLES2Implementation(
  #   "gpu/command_buffer/client/raster_implementation_impl_autogen.h")
  # gen.WriteGLES2ImplementationUnitTests(
  #     "gpu/command_buffer/client/raster_implementation_unittest_autogen.h")
  # gen.WriteGLES2TraceImplementationHeader(
  #     "gpu/command_buffer/client/raster_trace_implementation_autogen.h")
  # gen.WriteGLES2TraceImplementation(
  #     "gpu/command_buffer/client/raster_trace_implementation_impl_autogen.h")
  # gen.WriteGLES2CLibImplementation(
  #   "gpu/command_buffer/client/raster_c_lib_autogen.h")
  # gen.WriteCmdHelperHeader(
  #   "gpu/command_buffer/client/raster_cmd_helper_autogen.h")
  gen.WriteServiceImplementation(
    "gpu/command_buffer/service/raster_decoder_autogen.h")
  gen.WriteServiceUnitTests(
    "gpu/command_buffer/service/raster_decoder_unittest_%d_autogen.h")
  # gen.WriteServiceUnitTestsForExtensions(
  #   "gpu/command_buffer/service/"
  #   "raster_cmd_decoder_unittest_extensions_autogen.h")
  # gen.WriteServiceUtilsHeader(
  #   "gpu/command_buffer/service/raster_cmd_validation_autogen.h")
  # gen.WriteServiceUtilsImplementation(
  #   "gpu/command_buffer/service/"
  #   "raster_cmd_validation_implementation_autogen.h")

  build_cmd_buffer_lib.Format(gen.generated_cpp_filenames)

  if gen.errors > 0:
    print "%d errors" % gen.errors
    return 1
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
