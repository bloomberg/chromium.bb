# Copyright (c) 2014 Google Inc. All Rights Reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'cast_tests',
      'type': 'none',
      'dependencies': [
        'cast_test_generator',
      ],
      'conditions': [
        ['chromecast_branding=="Chrome"', {
          'dependencies': [
            '<(cast_internal_gyp):cast_tests_internal',
          ],
        }],
      ],
    },
    # This target only depends on targets that generate test binaries.
    {
      'target_name': 'cast_test_generator',
      'type': 'none',
      'dependencies': [
        'media/media.gyp:cast_media_unittests',
        '../base/base.gyp:base_unittests',
        '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation_unittests',
        '../content/content_shell_and_tests.gyp:content_unittests',
        '../crypto/crypto.gyp:crypto_unittests',
        '../ipc/ipc.gyp:ipc_tests',
        '../jingle/jingle.gyp:jingle_unittests',
        '../media/media.gyp:media_unittests',
        '../net/net.gyp:net_unittests',
        '../sandbox/sandbox.gyp:sandbox_linux_unittests',
        '../sql/sql.gyp:sql_unittests',
        '../sync/sync.gyp:sync_unit_tests',
        '../ui/base/ui_base_tests.gyp:ui_base_unittests',
        '../url/url.gyp:url_unittests',
      ],
      'variables': {
        'filters': [
          # Disable OutOfMemoryDeathTest.ViaSharedLibraries due to gTrusty eglibc incompatibility
          # See: crbug/428211
          'base_unittests --gtest_filter=-OutOfMemoryDeathTest.ViaSharedLibraries',
        ],
      },
      'conditions': [
        ['disable_display==0', {
          'dependencies': [
            '../gpu/gpu.gyp:gpu_unittests',
          ],
        }],
        ['OS!="android"', {
          'dependencies': [
            'cast_shell_browser_test',
          ],
          'variables': {
            'filters': [
              'cast_shell_browser_test --no-sandbox --disable-gpu',
            ],
          },
        }],
      ],
      'includes': ['build/tests/test_list.gypi'],
    },
    # Builds all tests and the output lists of build/run targets for those tests.
    # Note: producing a predetermined list of dependent inputs on which to
    # regenerate this output is difficult with GYP. This file is not
    # guaranteed to be regenerated outside of a clean build.
    {
      'target_name': 'cast_test_lists',
      'type': 'none',
      'dependencies': [
        'cast_tests',
      ],
      'variables': {
        'test_generator_py': '<(DEPTH)/chromecast/tools/build/generate_test_lists.py',
        'test_inputs_dir': '<(SHARED_INTERMEDIATE_DIR)/chromecast/tests',
        'test_additional_options': '--ozone-platform=test'
      },
      'actions': [
        {
          'action_name': 'generate_combined_test_build_list',
          'message': 'Generating combined test build list',
          'inputs': ['<(test_generator_py)'],
          'outputs': ['<(PRODUCT_DIR)/tests/build_test_list.txt'],
          'action': [
            'python', '<(test_generator_py)',
            '-t', '<(test_inputs_dir)',
            '-o', '<@(_outputs)',
            'pack_build',
          ],
        },
        {
          'action_name': 'generate_combined_test_run_list',
          'message': 'Generating combined test run list',
          'inputs': ['<(test_generator_py)'],
          'outputs': ['<(PRODUCT_DIR)/tests/run_test_list.txt'],
          'action': [
            'python', '<(test_generator_py)',
            '-t', '<(test_inputs_dir)',
            '-o', '<@(_outputs)',
            '-a', '<(test_additional_options)',
            'pack_run',
          ],
        }
      ],
    },
    {
      'target_name': 'cast_metrics_test_support',
      'type': '<(component)',
      'dependencies': [
        'cast_base',
      ],
      'sources': [
        'base/metrics/cast_metrics_test_helper.cc',
        'base/metrics/cast_metrics_test_helper.h',
      ],
    },  # end of target 'cast_metrics_test_support'
  ],  # end of targets
  'conditions': [
    ['OS=="android"', {
      'targets': [
        {
          'target_name': 'cast_android_tests',
          'type': 'none',
          'dependencies': [
            '../base/base.gyp:base_unittests_apk',
            '../cc/cc_tests.gyp:cc_unittests_apk',
            '../ipc/ipc.gyp:ipc_tests_apk',
            '../media/media.gyp:media_unittests_apk',
            '../net/net.gyp:net_unittests_apk',
            '../sandbox/sandbox.gyp:sandbox_linux_jni_unittests_apk',
            '../sql/sql.gyp:sql_unittests_apk',
            '../sync/sync.gyp:sync_unit_tests_apk',
            '../ui/events/events.gyp:events_unittests_apk',
            '../ui/gfx/gfx_tests.gyp:gfx_unittests_apk',
          ],
          'includes': ['build/tests/test_list.gypi'],
        },
        {
          'target_name': 'cast_android_test_lists',
          'type': 'none',
          'dependencies': [
            'cast_android_tests',
          ],
          'variables': {
            'test_generator_py': '<(DEPTH)/chromecast/tools/build/generate_test_lists.py',
            'test_inputs_dir': '<(SHARED_INTERMEDIATE_DIR)/chromecast/tests',
          },
          'actions': [
            {
              'action_name': 'generate_combined_test_build_list',
              'message': 'Generating combined test build list',
              'inputs': ['<(test_generator_py)'],
              'outputs': ['<(PRODUCT_DIR)/tests/build_test_list_android.txt'],
              'action': [
                'python', '<(test_generator_py)',
                '-t', '<(test_inputs_dir)',
                '-o', '<@(_outputs)',
                'pack_build',
              ],
            },
          ],
        },
      ],  # end of targets
    }, {  # OS!="android"
      'targets': [
        {
          'target_name': 'cast_shell_test_support',
          'type': '<(component)',
          'defines': [
            'HAS_OUT_OF_PROC_TEST_RUNNER',
          ],
          'dependencies': [
            'cast_shell_core',
            '../content/content_shell_and_tests.gyp:content_browser_test_support',
            '../testing/gtest.gyp:gtest',
            '../third_party/mojo/mojo_public.gyp:mojo_cpp_bindings',
          ],
          'sources': [
            'browser/test/chromecast_browser_test.cc',
            'browser/test/chromecast_browser_test.h',
            'browser/test/chromecast_browser_test_runner.cc',
          ],
        },  # end of target 'cast_shell_test_support'
        {
          'target_name': 'cast_shell_browser_test',
          'type': '<(gtest_target_type)',
          'dependencies': [
            'cast_shell_test_support',
            '../testing/gtest.gyp:gtest',
          ],
          'defines': [
            'HAS_OUT_OF_PROC_TEST_RUNNER',
          ],
          'sources': [
            'browser/test/chromecast_shell_browser_test.cc',
          ],
        },
      ],  # end of targets
    }],
  ],  # end of conditions
}
