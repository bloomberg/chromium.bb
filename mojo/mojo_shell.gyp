# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [{
    'target_name': 'mojo_shell_lib',
    'type': 'static_library',
    'sources': [
      'services/catalog/catalog.cc',
      'services/catalog/catalog.h',
      'services/catalog/entry.cc',
      'services/catalog/entry.h',
      'services/catalog/factory.cc',
      'services/catalog/factory.h',
      'services/catalog/store.cc',
      'services/catalog/store.h',
      'services/catalog/types.h',
      'shell/loader.h',
      'shell/connect_params.cc',
      'shell/connect_params.h',
      'shell/connect_util.cc',
      'shell/connect_util.h',
      'shell/native_runner.h',
      'shell/native_runner_delegate.h',
      'shell/shell.cc',
      'shell/shell.h',
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
      '<(DEPTH)/mojo/mojo_base.gyp:mojo_url_type_converters',
      '<(DEPTH)/net/net.gyp:net',
      '<(DEPTH)/url/url.gyp:url_lib',
    ],
    'export_dependent_settings': [
      '<(DEPTH)/mojo/mojo_base.gyp:mojo_application_bindings',
    ],
  }, {
    'target_name': 'mojo_shell_unittests',
    'type': 'executable',
    'sources': [
      'shell/tests/loader_unittest.cc',
    ],
    'dependencies': [
      'mojo_shell_lib',
      'mojo_shell_test_bindings',
      '<(DEPTH)/base/base.gyp:base',
      '<(DEPTH)/mojo/mojo_base.gyp:mojo_application_base',
      '<(DEPTH)/mojo/mojo_base.gyp:mojo_common_lib',
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
        'shell/tests/test.mojom',
      ],
    },
    'includes': [
      'mojom_bindings_generator_explicit.gypi',
    ],
  }, {
    'target_name': 'mojo_runner_common_lib',
    'type': 'static_library',
    'sources': [
      'shell/runner/common/client_util.cc',
      'shell/runner/common/client_util.h',
      'shell/runner/common/switches.cc',
      'shell/runner/common/switches.h',
    ],
    'include_dirs': [
      '..',
    ],
    'dependencies': [
      '<(DEPTH)/base/base.gyp:base',
      '<(DEPTH)/mojo/mojo_base.gyp:mojo_application_bindings',
      '<(DEPTH)/mojo/mojo_edk.gyp:mojo_system_impl',
      '<(DEPTH)/mojo/mojo_public.gyp:mojo_cpp_bindings',
      '<(DEPTH)/mojo/mojo_public.gyp:mojo_cpp_system',
    ],
    'export_dependent_settings': [
      '<(DEPTH)/mojo/mojo_base.gyp:mojo_application_bindings',
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
      'mojo_shell_lib',
      '<(DEPTH)/base/base.gyp:base',
      '<(DEPTH)/base/base.gyp:base_i18n',
      '<(DEPTH)/base/base.gyp:base_static',
      '<(DEPTH)/mojo/mojo_base.gyp:mojo_application_bindings',
      '<(DEPTH)/mojo/mojo_edk.gyp:mojo_system_impl',
      '<(DEPTH)/mojo/mojo_platform_handle.gyp:platform_handle',
      '<(DEPTH)/mojo/mojo_public.gyp:mojo_message_pump_lib',
    ],
    'export_dependent_settings': [
      '<(DEPTH)/mojo/mojo_base.gyp:mojo_application_bindings',
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
      ['OS=="mac"', {
        'sources': [
          'shell/runner/host/mach_broker.cc',
          'shell/runner/host/mach_broker.h',
        ],
      }],
    ],
  }],
}
