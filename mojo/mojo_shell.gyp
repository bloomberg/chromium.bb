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
      '../services/shell/loader.h',
      '../services/shell/connect_params.cc',
      '../services/shell/connect_params.h',
      '../services/shell/connect_util.cc',
      '../services/shell/connect_util.h',
      '../services/shell/native_runner.h',
      '../services/shell/native_runner_delegate.h',
      '../services/shell/shell.cc',
      '../services/shell/shell.h',
      '../services/shell/switches.cc',
      '../services/shell/switches.cc',
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
      '../services/shell/tests/loader_unittest.cc',
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
        '../services/shell/tests/test.mojom',
      ],
    },
    'includes': [
      'mojom_bindings_generator_explicit.gypi',
    ],
  }, {
    'target_name': 'mojo_runner_common_lib',
    'type': 'static_library',
    'sources': [
      '../services/shell/runner/common/client_util.cc',
      '../services/shell/runner/common/client_util.h',
      '../services/shell/runner/common/switches.cc',
      '../services/shell/runner/common/switches.h',
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
      '../services/shell/runner/host/child_process.cc',
      '../services/shell/runner/host/child_process.h',
      '../services/shell/runner/host/child_process_base.cc',
      '../services/shell/runner/host/child_process_base.h',
      '../services/shell/runner/host/child_process_host.cc',
      '../services/shell/runner/host/child_process_host.h',
      '../services/shell/runner/host/in_process_native_runner.cc',
      '../services/shell/runner/host/in_process_native_runner.h',
      '../services/shell/runner/host/out_of_process_native_runner.cc',
      '../services/shell/runner/host/out_of_process_native_runner.h',
      '../services/shell/runner/host/native_application_support.cc',
      '../services/shell/runner/host/native_application_support.h',
      '../services/shell/runner/init.cc',
      '../services/shell/runner/init.h',
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
          '../services/shell/runner/host/linux_sandbox.cc',
          '../services/shell/runner/host/linux_sandbox.h',
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
          '../services/shell/runner/host/mach_broker.cc',
          '../services/shell/runner/host/mach_broker.h',
        ],
      }],
    ],
  }, {
    # GN version: //mojo/services/catalog:manifest
    'target_name': 'mojo_catalog_manifest',
    'type': 'none',
    'variables': {
      'application_type': 'mojo',
      'application_name': 'catalog',
      'source_manifest': '<(DEPTH)/mojo/services/catalog/manifest.json',
    },
    'includes': [
      '../mojo/public/mojo_application_manifest.gypi',
    ],
    'hard_dependency': 1,
  }],
}
