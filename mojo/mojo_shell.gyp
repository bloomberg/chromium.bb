# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [{
    'target_name': 'mojo_shell_lib',
    'type': 'static_library',
    'sources': [
      'services/package_manager/loader.cc',
      'services/package_manager/loader.h',
      'services/package_manager/package_manager.cc',
      'services/package_manager/package_manager.h',
      'shell/application_instance.cc',
      'shell/application_instance.h',
      'shell/application_loader.h',
      'shell/application_manager.cc',
      'shell/application_manager.h',
      'shell/capability_filter.cc',
      'shell/capability_filter.h',
      'shell/connect_to_application_params.cc',
      'shell/connect_to_application_params.h',
      'shell/connect_util.cc',
      'shell/connect_util.h',
      'shell/identity.cc',
      'shell/identity.h',
      'shell/native_runner.h',
      'shell/static_application_loader.cc',
      'shell/static_application_loader.h',
      'shell/switches.cc',
      'shell/switches.cc',
      'util/filename_util.cc',
      'util/filename_util.h',
    ],
    'dependencies': [
      '<(DEPTH)/base/base.gyp:base',
      '<(DEPTH)/base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
      '<(DEPTH)/mojo/mojo_base.gyp:mojo_application_base',
      '<(DEPTH)/mojo/mojo_base.gyp:mojo_application_bindings',
      '<(DEPTH)/mojo/mojo_base.gyp:mojo_common_lib',
      '<(DEPTH)/mojo/mojo_base.gyp:mojo_environment_chromium',
      '<(DEPTH)/mojo/mojo_base.gyp:mojo_url_type_converters',
      '<(DEPTH)/url/url.gyp:url_lib',
    ],
  }, {
    'target_name': 'mojo_shell_unittests',
    'type': 'executable',
    'sources': [
      'shell/tests/application_manager_unittest.cc',
      'shell/tests/capability_filter_unittest.cc',
    ],
    'dependencies': [
      '<(DEPTH)/mojo/mojo_shell.gyp:mojo_shell_lib',
      '<(DEPTH)/mojo/mojo_shell.gyp:mojo_shell_test_bindings',
      '<(DEPTH)/base/base.gyp:base',
      '<(DEPTH)/mojo/mojo_base.gyp:mojo_application_base',
      '<(DEPTH)/mojo/mojo_base.gyp:mojo_common_lib',
      '<(DEPTH)/mojo/mojo_base.gyp:mojo_environment_chromium',
      '<(DEPTH)/mojo/mojo_base.gyp:mojo_url_type_converters',
      '<(DEPTH)/mojo/mojo_edk.gyp:mojo_run_all_unittests',
      '<(DEPTH)/mojo/mojo_public.gyp:mojo_cpp_bindings',
      '<(DEPTH)/testing/gtest.gyp:gtest',
      '<(DEPTH)/url/url.gyp:url_lib',
    ]
  }, {
    'target_name': 'mojo_shell_test_bindings',
    'type': 'static_library',
    'variables': {
      'mojom_files': [
        'shell/tests/capability_filter_unittest.mojom',
        'shell/tests/test.mojom',
      ],
    },
    'includes': [
      'mojom_bindings_generator_explicit.gypi',
    ],
  }],
}
