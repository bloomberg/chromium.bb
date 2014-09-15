# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //mojo/apps/js
      #             //mojo/apps/js/bindings
      #             //mojo/apps/js/bindings/gl
      'target_name': 'mojo_js_lib',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../gin/gin.gyp:gin',
        '../ui/gl/gl.gyp:gl',
        '../v8/tools/gyp/v8.gyp:v8',
        'mojo_base.gyp:mojo_common_lib',
        'mojo_base.gyp:mojo_environment_chromium',
        'mojo_base.gyp:mojo_gles2_bindings',
        'mojo_base.gyp:mojo_js_bindings_lib',
        'mojo_native_viewport_bindings',
      ],
      'includes': [
        'mojo_public_gles2_for_loadable_module.gypi',
      ],
      'export_dependent_settings': [
        '../base/base.gyp:base',
        '../gin/gin.gyp:gin',
        'mojo_base.gyp:mojo_common_lib',
        'mojo_base.gyp:mojo_gles2_bindings',
        'mojo_native_viewport_bindings',
      ],
      'sources': [
        'apps/js/mojo_runner_delegate.cc',
        'apps/js/mojo_runner_delegate.h',
        'apps/js/bindings/threading.cc',
        'apps/js/bindings/threading.h',
        'apps/js/bindings/gl/context.cc',
        'apps/js/bindings/gl/context.h',
        'apps/js/bindings/gl/module.cc',
        'apps/js/bindings/gl/module.h',
        'apps/js/bindings/monotonic_clock.cc',
        'apps/js/bindings/monotonic_clock.h',
      ],
    },
    {
      # GN version: //mojo/apps/js/test:js_to_cpp_bindings
      'target_name': 'mojo_apps_js_bindings',
      'type': 'static_library',
      'sources': [
        'apps/js/test/js_to_cpp.mojom',
      ],
      'includes': [ 'public/tools/bindings/mojom_bindings_generator.gypi' ],
      'export_dependent_settings': [
        'mojo_base.gyp:mojo_cpp_bindings',
      ],
      'dependencies': [
        'mojo_base.gyp:mojo_cpp_bindings',
      ],
    },
    {
      # GN version: //mojo/apps/js/test/mojo_apps_js_unittests
      'target_name': 'mojo_apps_js_unittests',
      'type': 'executable',
      'dependencies': [
        '../gin/gin.gyp:gin_test',
        'mojo_base.gyp:mojo_common_lib',
        'mojo_base.gyp:mojo_common_test_support',
        'mojo_base.gyp:mojo_public_test_interfaces',
        'mojo_base.gyp:mojo_run_all_unittests',
        'mojo_apps_js_bindings',
        'mojo_js_lib',
      ],
      'sources': [
        'apps/js/test/handle_unittest.cc',
        'apps/js/test/js_to_cpp_unittest.cc',
        'apps/js/test/run_apps_js_tests.cc',
      ],
    },
    {
      # GN version: //mojo/apps/js:mojo_js
      'target_name': 'mojo_js',
      'type': 'loadable_module',
      'dependencies': [
        'mojo_base.gyp:mojo_application_chromium',
        'mojo_base.gyp:mojo_cpp_bindings',
        'mojo_base.gyp:mojo_utility',
        'mojo_content_handler_bindings',
        'mojo_js_lib',
        '<(mojo_system_for_loadable_module)',
      ],
      'sources': [
        'apps/js/application_delegate_impl.cc',
        'apps/js/js_app.cc',
        'apps/js/mojo_module.cc',
        'apps/js/main.cc',
      ],
    },
  ],
  'conditions': [
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'mojo_apps_js_unittests_run',
          'type': 'none',
          'dependencies': [
            'mojo_apps_js_unittests',
          ],
          'includes': [
            '../build/isolate.gypi',
            'mojo_apps_js_unittests.isolate',
          ],
          'sources': [
            'mojo_apps_js_unittests.isolate',
          ],
        },
      ],
    }],
  ],
}
