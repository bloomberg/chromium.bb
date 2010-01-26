# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'gpu_demo_framework',
      'type': 'static_library',
      'dependencies': [
        '../../base/base.gyp:base',
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
      ],
      'all_dependent_settings': {
        'sources': [
          'framework/main_exe.cc',
        ],
      },
      'sources': [
        'framework/platform.h',
        'framework/window.cc',
        'framework/window.h',
      ],
    },
    {
      'target_name': 'gpu_demo_framework_pepper',
      'type': 'static_library',
      'dependencies': [
        'gpu_demo_framework',
        '../gpu.gyp:pgl',
      ],
      'all_dependent_settings': {
        'sources': [
          'framework/main_pepper.cc',
          'framework/plugin.def',
          'framework/plugin.rc',
        ],
        'run_as': {
          'action': [
            '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)chrome<(EXECUTABLE_SUFFIX)',
            '--no-sandbox',
            '--internal-pepper',
            '--enable-gpu-plugin',
            '--load-plugin=$(TargetPath)',
            'file://$(ProjectDir)pepper_gpu_demo.html',
          ],
        },
      },
      'sources': [
        'framework/plugin.cc',
        'framework/plugin.h',
      ],
    },
    {
      'target_name': 'hello_triangle_exe',
      'type': 'executable',
      'dependencies': [
        'gpu_demo_framework_exe',
        '../../third_party/gles2_book/gles2_book.gyp:hello_triangle',
      ],
      'sources': [
        'gles2_book/example.h',
        'gles2_book/hello_triangle.cc',
      ],
    },
    {
      'target_name': 'hello_triangle_pepper',
      'type': 'shared_library',
      'dependencies': [
        'gpu_demo_framework_pepper',
        '../../third_party/gles2_book/gles2_book.gyp:hello_triangle',
      ],
      'sources': [
        'gles2_book/example.h',
        'gles2_book/hello_triangle.cc',
      ],
    },
  ]
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
