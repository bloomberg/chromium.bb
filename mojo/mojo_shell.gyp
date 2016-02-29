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
      'shell/loader.h',
      'shell/application_manager.cc',
      'shell/application_manager.h',
      'shell/connect_params.cc',
      'shell/connect_params.h',
      'shell/connect_util.cc',
      'shell/connect_util.h',
      'shell/identity.cc',
      'shell/identity.h',
      'shell/native_runner.h',
      'shell/native_runner_delegate.h',
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
      'mojo_shell_lib',
      'mojo_shell_test_bindings',
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
  }, {
    'target_name': 'mojo_runner_connection_lib',
    'type': 'static_library',
    'sources': [
      'shell/runner/child/runner_connection.cc',
      'shell/runner/child/runner_connection.h',
    ],
    'dependencies': [
      'mojo_runner_common_lib',
      'mojo_runner_connection_bindings_lib',
      '<(DEPTH)/base/base.gyp:base',
      '<(DEPTH)/mojo/mojo_base.gyp:mojo_application_base',
      '<(DEPTH)/mojo/mojo_edk.gyp:mojo_system_impl',
      '<(DEPTH)/mojo/mojo_platform_handle.gyp:platform_handle',
      '<(DEPTH)/mojo/mojo_public.gyp:mojo_message_pump_lib',
    ],
  }, {
    'target_name': 'mojo_runner_common_lib',
    'type': 'static_library',
    'sources': [
      'shell/runner/common/switches.cc',
      'shell/runner/common/switches.h',
    ],
    'include_dirs': [
      '..',
    ],
  }, {
    'target_name': 'mojo_runner_connection_bindings_lib',
    'type': 'static_library',
    'dependencies': [
      'mojo_runner_connection_mojom',
    ],
  }, {
    'target_name': 'mojo_runner_connection_mojom',
    'type': 'none',
    'variables': {
      'mojom_files': [
        'shell/runner/child/child_controller.mojom',
      ],
    },
    'includes': [
      'mojom_bindings_generator_explicit.gypi',
    ],
    'dependencies': [
      '<(DEPTH)/mojo/mojo_base.gyp:mojo_application_base',
    ],
  }, {
    'target_name': 'mojo_runner_host_lib',
    'type': 'static_library',
    'sources': [
      'shell/runner/host/child_process.cc',
      'shell/runner/host/child_process.h',
      'shell/runner/host/child_process_base.cc',
      'shell/runner/host/child_process_base.h',
      'shell/runner/host/child_process_host.cc',
      'shell/runner/host/child_process_host.h',
      'shell/runner/host/in_process_native_runner.cc',
      'shell/runner/host/in_process_native_runner.h',
      'shell/runner/host/out_of_process_native_runner.cc',
      'shell/runner/host/out_of_process_native_runner.h',
      'shell/runner/host/native_application_support.cc',
      'shell/runner/host/native_application_support.h',
      'shell/runner/init.cc',
      'shell/runner/init.h',
    ],
    'dependencies': [
      'mojo_runner_common_lib',
      'mojo_runner_connection_bindings_lib',
      'mojo_shell_lib',
      '<(DEPTH)/base/base.gyp:base',
      '<(DEPTH)/base/base.gyp:base_i18n',
      '<(DEPTH)/base/base.gyp:base_static',
      '<(DEPTH)/mojo/mojo_edk.gyp:mojo_system_impl',
      '<(DEPTH)/mojo/mojo_platform_handle.gyp:platform_handle',
      '<(DEPTH)/mojo/mojo_public.gyp:mojo_message_pump_lib',
    ],
    'conditions': [
      ['OS=="linux"', {
        'sources': [
          'shell/runner/host/linux_sandbox.cc',
          'shell/runner/host/linux_sandbox.h',
        ],
        'dependencies': [
          '<(DEPTH)/sandbox/sandbox.gyp:sandbox',
          '<(DEPTH)/sandbox/sandbox.gyp:sandbox_services',
          '<(DEPTH)/sandbox/sandbox.gyp:seccomp_bpf',
          '<(DEPTH)/sandbox/sandbox.gyp:seccomp_bpf_helpers',
        ],
      }],
    ],
  }],
}
