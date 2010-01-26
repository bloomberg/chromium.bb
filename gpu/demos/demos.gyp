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
    {
      'target_name': 'mip_map_2d_exe',
      'type': 'executable',
      'dependencies': [
        'gpu_demo_framework_exe',
        '../../third_party/gles2_book/gles2_book.gyp:mip_map_2d',
      ],
      'sources': [
        'gles2_book/example.h',
        'gles2_book/mip_map_2d.cc',
      ],
    },
    {
      'target_name': 'mip_map_2d_pepper',
      'type': 'shared_library',
      'dependencies': [
        'gpu_demo_framework_pepper',
        '../../third_party/gles2_book/gles2_book.gyp:mip_map_2d',
      ],
      'sources': [
        'gles2_book/example.h',
        'gles2_book/mip_map_2d.cc',
      ],
    },
    {
      'target_name': 'simple_texture_2d_exe',
      'type': 'executable',
      'dependencies': [
        'gpu_demo_framework_exe',
        '../../third_party/gles2_book/gles2_book.gyp:simple_texture_2d',
      ],
      'sources': [
        'gles2_book/example.h',
        'gles2_book/simple_texture_2d.cc',
      ],
    },
    {
      'target_name': 'simple_texture_2d_pepper',
      'type': 'shared_library',
      'dependencies': [
        'gpu_demo_framework_pepper',
        '../../third_party/gles2_book/gles2_book.gyp:simple_texture_2d',
      ],
      'sources': [
        'gles2_book/example.h',
        'gles2_book/simple_texture_2d.cc',
      ],
    },
    {
      'target_name': 'simple_texture_cubemap_exe',
      'type': 'executable',
      'dependencies': [
        'gpu_demo_framework_exe',
        '../../third_party/gles2_book/gles2_book.gyp:simple_texture_cubemap',
      ],
      'sources': [
        'gles2_book/example.h',
        'gles2_book/simple_texture_cubemap.cc',
      ],
    },
    {
      'target_name': 'simple_texture_cubemap_pepper',
      'type': 'shared_library',
      'dependencies': [
        'gpu_demo_framework_pepper',
        '../../third_party/gles2_book/gles2_book.gyp:simple_texture_cubemap',
      ],
      'sources': [
        'gles2_book/example.h',
        'gles2_book/simple_texture_cubemap.cc',
      ],
    },
    {
      'target_name': 'simple_vertex_shader_exe',
      'type': 'executable',
      'dependencies': [
        'gpu_demo_framework_exe',
        '../../third_party/gles2_book/gles2_book.gyp:simple_vertex_shader',
      ],
      'sources': [
        'gles2_book/example.h',
        'gles2_book/simple_vertex_shader.cc',
      ],
    },
    {
      'target_name': 'simple_vertex_shader_pepper',
      'type': 'shared_library',
      'dependencies': [
        'gpu_demo_framework_pepper',
        '../../third_party/gles2_book/gles2_book.gyp:simple_vertex_shader',
      ],
      'sources': [
        'gles2_book/example.h',
        'gles2_book/simple_vertex_shader.cc',
      ],
    },
    {
      'target_name': 'stencil_test_exe',
      'type': 'executable',
      'dependencies': [
        'gpu_demo_framework_exe',
        '../../third_party/gles2_book/gles2_book.gyp:stencil_test',
      ],
      'sources': [
        'gles2_book/example.h',
        'gles2_book/stencil_test.cc',
      ],
    },
    {
      'target_name': 'stencil_test_pepper',
      'type': 'shared_library',
      'dependencies': [
        'gpu_demo_framework_pepper',
        '../../third_party/gles2_book/gles2_book.gyp:stencil_test',
      ],
      'sources': [
        'gles2_book/example.h',
        'gles2_book/stencil_test.cc',
      ],
    },
    {
      'target_name': 'texture_wrap_exe',
      'type': 'executable',
      'dependencies': [
        'gpu_demo_framework_exe',
        '../../third_party/gles2_book/gles2_book.gyp:texture_wrap',
      ],
      'sources': [
        'gles2_book/example.h',
        'gles2_book/texture_wrap.cc',
      ],
    },
    {
      'target_name': 'texture_wrap_pepper',
      'type': 'shared_library',
      'dependencies': [
        'gpu_demo_framework_pepper',
        '../../third_party/gles2_book/gles2_book.gyp:texture_wrap',
      ],
      'sources': [
        'gles2_book/example.h',
        'gles2_book/texture_wrap.cc',
      ],
    },
  ]
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
