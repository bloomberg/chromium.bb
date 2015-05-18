# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [{
    'target_name': 'mojo_shell_lib',
    'type': 'static_library',
    'sources': [
      'shell/application_loader.h',
      'shell/application_manager.cc',
      'shell/application_manager.h',
      'shell/data_pipe_peek.cc',
      'shell/data_pipe_peek.h',
      'shell/fetcher.cc',
      'shell/fetcher.h',
      'shell/identity.cc',
      'shell/identity.h',
      'shell/local_fetcher.cc',
      'shell/local_fetcher.h',
      'shell/native_runner.h',
      'shell/network_fetcher.cc',
      'shell/network_fetcher.h',
      'shell/query_util.cc',
      'shell/query_util.h',
      'shell/shell_impl.cc',
      'shell/shell_impl.h',
      'shell/switches.cc',
      'shell/switches.h',
      'util/filename_util.cc',
      'util/filename_util.h',
    ],
    'dependencies': [
      '<(DEPTH)/base/base.gyp:base',
      '<(DEPTH)/base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
      '<(DEPTH)/crypto/crypto.gyp:crypto',
      '<(DEPTH)/mojo/mojo_base.gyp:mojo_common_lib',
      '<(DEPTH)/mojo/mojo_base.gyp:mojo_environment_chromium',
      '<(DEPTH)/mojo/mojo_base.gyp:mojo_url_type_converters',
      '<(DEPTH)/mojo/mojo_services.gyp:content_handler_bindings_mojom',
      '<(DEPTH)/mojo/mojo_services.gyp:network_service_bindings_mojom',
      '<(DEPTH)/third_party/mojo/mojo_public.gyp:mojo_application_bindings_mojom',
      '<(DEPTH)/third_party/mojo/mojo_edk.gyp:mojo_system_impl',
      '<(DEPTH)/url/url.gyp:url_lib',
    ],
  }, {
    'target_name': 'mojo_shell_unittests',
    'type': 'executable',
    'sources': [
      'shell/application_manager_unittest.cc',
      'shell/query_util_unittest.cc',
    ],
    'dependencies': [
      '<(DEPTH)/mojo/mojo_shell.gyp:mojo_shell_lib',
      '<(DEPTH)/mojo/mojo_shell.gyp:mojo_shell_test_bindings',
      '<(DEPTH)/base/base.gyp:base',
      '<(DEPTH)/mojo/mojo_base.gyp:mojo_application_chromium',
      '<(DEPTH)/mojo/mojo_base.gyp:mojo_common_lib',
      '<(DEPTH)/mojo/mojo_base.gyp:mojo_environment_chromium',
      '<(DEPTH)/mojo/mojo_base.gyp:mojo_url_type_converters',
      '<(DEPTH)/third_party/mojo/mojo_edk.gyp:mojo_run_all_unittests',
      '<(DEPTH)/third_party/mojo/mojo_public.gyp:mojo_application_base',
      '<(DEPTH)/third_party/mojo/mojo_public.gyp:mojo_cpp_bindings',
      '<(DEPTH)/testing/gtest.gyp:gtest',
      '<(DEPTH)/url/url.gyp:url_lib',
    ]
  }, {
    'target_name': 'mojo_shell_test_bindings',
    'type': 'static_library',
    'variables': {
      'mojom_files': [
        'shell/test.mojom',
      ],
    },
    'includes': [
      '../third_party/mojo/mojom_bindings_generator_explicit.gypi',
    ],
  }],
}
