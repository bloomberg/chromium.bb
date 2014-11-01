# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'mojo_variables.gypi',
  ],
  'targets': [
    {
      # GN version: //mojo
      'target_name': 'mojo',
      'type': 'none',
      'dependencies': [
        'edk/mojo_edk.gyp:mojo_edk',
        'mojo_application_manager',
        'mojo_application_manager_unittests',
        'mojo_base.gyp:mojo_base',
        'mojo_geometry_converters.gyp:mojo_geometry_lib',
        'mojo_input_events_converters.gyp:mojo_input_events_lib',
        'mojo_js_unittests',
        'mojo_surface_converters.gyp:mojo_surfaces_lib',
        'mojo_surface_converters.gyp:mojo_surfaces_lib_unittests',
        'services/public/mojo_services_public.gyp:mojo_services_public',
        'public/mojo_public.gyp:mojo_public',
      ],
    },
    {
      # GN version: //mojo/application_manager
      'target_name': 'mojo_application_manager',
      'type': '<(component)',
      'defines': [
        'MOJO_APPLICATION_MANAGER_IMPLEMENTATION',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../url/url.gyp:url_lib',
        'services/public/mojo_services_public.gyp:mojo_content_handler_bindings',
        'services/public/mojo_services_public.gyp:mojo_network_bindings',
        'mojo_base.gyp:mojo_common_lib',
        'mojo_base.gyp:mojo_environment_chromium',
        'public/mojo_public.gyp:mojo_application_bindings',
        '<(mojo_system_for_component)',
      ],
      'sources': [
        'application_manager/application_loader.cc',
        'application_manager/application_loader.h',
        'application_manager/application_manager.cc',
        'application_manager/application_manager.h',
        'application_manager/application_manager_export.h',
        'application_manager/background_shell_application_loader.cc',
        'application_manager/background_shell_application_loader.h',
      ],
      'export_dependent_settings': [
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        'public/mojo_public.gyp:mojo_application_bindings',
        'services/public/mojo_services_public.gyp:mojo_network_bindings',
      ],
    },
    {
      # GN version: //mojo/application_manager:mojo_application_manager_unittests
      'target_name': 'mojo_application_manager_unittests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/gtest.gyp:gtest',
        '../url/url.gyp:url_lib',
        'edk/mojo_edk.gyp:mojo_run_all_unittests',
        'mojo_application_manager',
        'mojo_base.gyp:mojo_application_chromium',
        'mojo_base.gyp:mojo_common_lib',
        'mojo_base.gyp:mojo_environment_chromium',
        'public/mojo_public.gyp:mojo_cpp_bindings',
      ],
      'includes': [ 'public/tools/bindings/mojom_bindings_generator.gypi' ],
      'sources': [
        'application_manager/application_manager_unittest.cc',
        'application_manager/background_shell_application_loader_unittest.cc',
        'application_manager/test.mojom',
      ],
    },
    {
      # GN version: //mojo/bindings/js/tests:mojo_js_unittests
      'target_name': 'mojo_js_unittests',
      'type': 'executable',
      'dependencies': [
        '../gin/gin.gyp:gin_test',
        'edk/mojo_edk.gyp:mojo_common_test_support',
        'edk/mojo_edk.gyp:mojo_run_all_unittests',
        'mojo_base.gyp:mojo_js_bindings_lib',
        'public/mojo_public.gyp:mojo_environment_standalone',
        'public/mojo_public.gyp:mojo_public_test_interfaces',
        'public/mojo_public.gyp:mojo_utility',
      ],
      'sources': [
        'bindings/js/tests/run_js_tests.cc',
      ],
    },
  ],
}
