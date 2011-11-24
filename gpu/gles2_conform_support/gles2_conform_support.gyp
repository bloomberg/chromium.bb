# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # These are defined here because we want to be able to compile them on
    # the buildbots without needed the OpenGL ES 2.0 conformance tests
    # which are not open source.
    'bootstrap_sources_native': [
      'native/main.cc',
    ],
    'conditions': [
      ['OS=="linux"', {
        'bootstrap_sources_native': [
          'native/egl_native.cc',
          'native/egl_native_linux.cc',
        ],
      }],
      ['OS=="win"', {
        'bootstrap_sources_native': [
          'native/egl_native.cc',
          'native/egl_native_win.cc',
        ],
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'egl_native',
      'type': 'static_library',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/gpu/gpu.gyp:command_buffer_service',
      ],
      'include_dirs': ['<(DEPTH)/third_party/khronos'],
      'sources': [
        'egl/config.cc',
        'egl/config.h',
        'egl/display.cc',
        'egl/display.h',
        'egl/egl.cc',
        'egl/surface.cc',
        'egl/surface.h',
      ],
      'direct_dependent_settings': {
        'include_dirs': ['<(DEPTH)/third_party/khronos'],
      },
      'defines': [
        'EGLAPI=',
        'EGLAPIENTRY=',
      ],
    },
    {
      'target_name': 'egl_main_native',
      'type': 'static_library',
      'dependencies': [
        'egl_native',
      ],
      'conditions': [
        ['toolkit_uses_gtk == 1', {
          'dependencies': ['../../build/linux/system.gyp:gtk'],
        }],
      ],
      'include_dirs': ['<(DEPTH)/third_party/khronos'],
      'sources': [
        '<@(bootstrap_sources_native)',
      ],
      'direct_dependent_settings': {
        'include_dirs': ['<(DEPTH)/third_party/khronos'],
      },
      'defines': [
        'GTF_GLES20',
        'EGLAPI=',
        'EGLAPIENTRY=',
      ],
    },
    {
      'target_name': 'gles2_conform_support',
      'type': 'executable',
      'dependencies': [
        'egl_native',
        '<(DEPTH)/base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '<(DEPTH)/gpu/gpu.gyp:gles2_c_lib_nocheck',
        '<(DEPTH)/third_party/expat/expat.gyp:expat',
      ],
      'conditions': [
        ['toolkit_uses_gtk == 1', {
          'dependencies': ['../../build/linux/system.gyp:gtk'],
        }],
      ],
      'defines': [
        'GLES2_CONFORM_SUPPORT_ONLY',
        'GTF_GLES20',
        'EGLAPI=',
        'EGLAPIENTRY=',
      ],
      'sources': [
        '<@(bootstrap_sources_native)',
        'gles2_conform_support.c'
      ],
    },
  ],
}
