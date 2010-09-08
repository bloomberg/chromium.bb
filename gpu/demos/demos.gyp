# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    'conditions': [
      # Pepper demos that are compiled as shared libraries need to be compiled
      # with -fPIC flag. All static libraries that these demos depend on must
      # also be compiled with -fPIC flag. Setting GYP_DEFINES="linux_fpic=1"
      # compiles everything with -fPIC. Disable pepper demos on linux/x64
      # unless linux_fpic is 1.
      ['OS=="linux" and (target_arch=="x64" or target_arch=="arm") and linux_fpic!=1', {
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
      'dependencies': [
        '../../base/base.gyp:base',
      ],
      'sources': [
        'framework/demo.cc',
        'framework/demo.h',
        'framework/demo_factory.h',
      ],
      'all_dependent_settings': {
        'include_dirs': [
          '../..',
        ],
      },
    },
    {
      'target_name': 'gpu_demo_framework_exe',
      'type': 'static_library',
      'dependencies': [
        'gpu_demo_framework',
        '../gpu.gyp:command_buffer_client',
        '../gpu.gyp:command_buffer_service',
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
        ['OS=="linux"', {
          'dependencies': ['../../build/linux/system.gyp:gtk'],
        }],
      ],
    },
    {
      'target_name': 'gpu_demo_framework_pepper',
      'type': 'static_library',
      # This target injects a bunch of source files to direct dependents.
      # If the dependent is a none-type target (like all.gyp), gyp will
      # generate error due to these injected source files. Workaround this
      # problem by preventing it from being selected by demos.gyp:*.
      'suppress_wildcard': 1,
      'dependencies': [
        'gpu_demo_framework',
        '../gpu.gyp:pgl',
      ],
      'sources': [
        'framework/plugin.cc',
        'framework/plugin.h',
      ],
      'direct_dependent_settings': {
        'sources': [
          'framework/main_pepper.cc',
        ],
        'run_as': {
          'conditions': [
            ['OS=="mac"', {
              'action': [
                '<(PRODUCT_DIR)/Chromium.app/Contents/MacOS/Chromium',
                '--no-sandbox',
                '--internal-pepper',
                '--enable-gpu-plugin',
                '--load-plugin=<(PRODUCT_DIR)/$(PRODUCT_NAME).plugin',
                'file://$(SOURCE_ROOT)/pepper_gpu_demo.html',
              ],
             }, { # OS != "mac"
              'action': [
                '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)chrome<(EXECUTABLE_SUFFIX)',
                '--no-sandbox',
                '--internal-pepper',
                '--enable-gpu-plugin',
                '--load-plugin=$(TargetPath)',
                'file://$(ProjectDir)pepper_gpu_demo.html',
              ],
            }],
          ],
        },
        'conditions': [
          ['OS=="win"', {
            'sources': [
              'framework/plugin.def',
              'framework/plugin.rc',
            ],
          }],
          ['OS=="mac"', {
            'sources': [
              'framework/Plugin_Info.plist',
            ],
            'xcode_settings': {
              'INFOPLIST_FILE': 'framework/Plugin_Info.plist',
            },
            'mac_bundle': 1,
            'product_extension': 'plugin',
          }],
        ],
      },
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
  ],
  'conditions': [
    ['enable_pepper_demos==1', {
      'targets': [
        {
          'target_name': 'hello_triangle_pepper',
          'type': 'loadable_module',
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
          'target_name': 'mip_map_2d_pepper',
          'type': 'loadable_module',
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
          'target_name': 'simple_texture_2d_pepper',
          'type': 'loadable_module',
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
          'target_name': 'simple_texture_cubemap_pepper',
          'type': 'loadable_module',
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
          'target_name': 'simple_vertex_shader_pepper',
          'type': 'loadable_module',
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
          'target_name': 'stencil_test_pepper',
          'type': 'loadable_module',
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
          'target_name': 'texture_wrap_pepper',
          'type': 'loadable_module',
          'dependencies': [
            'gpu_demo_framework_pepper',
            '../../third_party/gles2_book/gles2_book.gyp:texture_wrap',
          ],
          'sources': [
            'gles2_book/example.h',
            'gles2_book/texture_wrap.cc',
          ],
        },
      ],
    }],
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
