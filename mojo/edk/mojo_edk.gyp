# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../mojo_variables.gypi',
  ],
  'targets': [
    {
      # GN version: //mojo/edk/system
      'target_name': 'mojo_system_impl',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
      ],
      'includes': [
        'mojo_edk_system_impl.gypi',
      ],
    },
    {
      # GN version: //mojo/edk/js
      'target_name': 'mojo_js_lib',
      'type': 'static_library',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../gin/gin.gyp:gin',
        '../../v8/tools/gyp/v8.gyp:v8',
      ],
      'export_dependent_settings': [
        '../../base/base.gyp:base',
        '../../gin/gin.gyp:gin',
      ],
      'sources': [
        # Sources list duplicated in GN build.
        'js/core.cc',
        'js/core.h',
        'js/drain_data.cc',
        'js/drain_data.h',
        'js/handle.cc',
        'js/handle.h',
        'js/handle_close_observer.h',
        'js/mojo_runner_delegate.cc',
        'js/mojo_runner_delegate.h',
        'js/support.cc',
        'js/support.h',
        'js/threading.cc',
        'js/threading.h',
        'js/waiting_callback.cc',
        'js/waiting_callback.h',
      ],
    },
    {
      # GN version: //mojo/edk/test:test_support_impl
      'target_name': 'mojo_test_support_impl',
      'type': 'static_library',
      'dependencies': [
        '../../base/base.gyp:base',
      ],
      'sources': [
        'test/test_support_impl.cc',
        'test/test_support_impl.h',
      ],
    },
    {
      # GN version: //mojo/edk/test:test_support
      'target_name': 'mojo_common_test_support',
      'type': 'static_library',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:test_support_base',
        '../../testing/gtest.gyp:gtest',
        'mojo_system_impl',
      ],
      'sources': [
        'test/multiprocess_test_helper.cc',
        'test/multiprocess_test_helper.h',
        'test/test_utils.h',
        'test/test_utils_posix.cc',
        'test/test_utils_win.cc',
      ],
      'conditions': [
        ['OS=="ios"', {
          'sources!': [
            'test/multiprocess_test_helper.cc',
          ],
        }],
      ],
    },
    {
      # GN version: //mojo/edk/test:run_all_unittests
      'target_name': 'mojo_run_all_unittests',
      'type': 'static_library',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:test_support_base',
        '../../testing/gtest.gyp:gtest',
        'mojo_system_impl',
        'mojo_test_support_impl',
        '../public/mojo_public.gyp:mojo_test_support',
      ],
      'sources': [
        'test/run_all_unittests.cc',
      ],
    },
    {
      # GN version: //mojo/edk/test:run_all_perftests
      'target_name': 'mojo_run_all_perftests',
      'type': 'static_library',
      'dependencies': [
        '../../base/base.gyp:test_support_base',
        'mojo_edk.gyp:mojo_system_impl',
        'mojo_test_support_impl',
        '../public/mojo_public.gyp:mojo_test_support',
      ],
      'sources': [
        'test/run_all_perftests.cc',
      ],
    },
  ],
  'conditions': [
    ['OS=="win" and target_arch=="ia32"', {
      'targets': [
        {
          'target_name': 'mojo_system_impl_win64',
          'type': '<(component)',
          'dependencies': [
            '../../base/base.gyp:base_win64',
            '../../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations_win64',
          ],
          'includes': [
            'mojo_edk_system_impl.gypi',
          ],
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
        },
      ],
    }],
  ],
}
