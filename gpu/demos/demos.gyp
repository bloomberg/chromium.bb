# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../ppapi/ppapi.gypi',
  ],
  'variables': {
    'chromium_code': 1,
    'conditions': [
      # Pepper demos that are compiled as shared libraries need to be compiled
      # with -fPIC flag. All static libraries that these demos depend on must
      # also be compiled with -fPIC flag. Setting GYP_DEFINES="linux_fpic=1"
      # compiles everything with -fPIC. Disable pepper demos on linux/x64
      # unless linux_fpic is 1.
      ['os_posix == 1 and OS != "mac" and (target_arch=="x64" or target_arch=="arm") and linux_fpic!=1', {
        'enable_pepper_demos%': 0,
      }, {
        'enable_pepper_demos%': 1,
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'gpu_demo_framework',
      'type': 'static_library',
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'framework/demo.cc',
        'framework/demo.h',
        'framework/demo_factory.h',
      ],
    },
    {
      'target_name': 'gpu_demo_framework_exe',
      'type': 'static_library',
      'dependencies': [
        'gpu_demo_framework',
        '../gpu.gyp:command_buffer_client',
        '../gpu.gyp:command_buffer_service',
        '../gpu.gyp:gles2_implementation',
        '../../base/base.gyp:base',
        '../../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../../ui/gl/gl.gyp:gl',
        '../../ui/ui.gyp:ui',
      ],
      'sources': [
        'framework/main_exe.cc',
        'framework/window.cc',
        'framework/window_linux.cc',
        'framework/window_mac.mm',
        'framework/window_win.cc',
        'framework/window.h',
      ],
      'conditions': [
        ['toolkit_uses_gtk == 1', {
          'dependencies': ['../../build/linux/system.gyp:gtk'],
        }],
      ],
    },
    {
      'target_name': 'gpu_demo_framework_ppapi',
      'suppress_wildcard': 1,  # So that 'all' doesn't end up being a bundle.
      'type': 'static_library',
      'dependencies': [
        'gpu_demo_framework',
        '../../ppapi/ppapi.gyp:ppapi_cpp_objects',
        '../../ppapi/ppapi.gyp:ppapi_gles2',
      ],
      'include_dirs': [
        '../..',
        '../../ppapi/lib/gl/include',
      ],
      'sources': [
        'framework/pepper.cc',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../..',
          '../../ppapi/lib/gl/include',
        ],
        'run_as': {
          'conditions': [
            ['OS=="mac"', {
              'action': [
                '<(PRODUCT_DIR)/Chromium.app/Contents/MacOS/Chromium',
                '--register-pepper-plugins='
                  '<(PRODUCT_DIR)/$(PRODUCT_NAME).plugin;'
                  'pepper-application/x-gpu-demo',
                'file://$(SOURCE_ROOT)/pepper_gpu_demo.html',
              ],
             }, { # OS != "mac"
              'action': [
                '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)chrome<(EXECUTABLE_SUFFIX)',
                '--register-pepper-plugins=$(TargetPath);'
                  'pepper-application/x-gpu-demo',
                'file://$(ProjectDir)pepper_gpu_demo.html',
              ],
            }],
          ],
        },
        'conditions': [
          ['os_posix == 1 and OS != "mac"', {
            # -gstabs, used in the official builds, causes an ICE. Remove it.
            'cflags!': ['-gstabs'],
          }],
          ['OS=="mac"', {
            'mac_bundle': 1,
            'product_extension': 'plugin',
          }],
        ],
      },
    },
  ],
}
