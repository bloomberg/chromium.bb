# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'devices_app_lib',
      'type': 'static_library',
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'devices_app.cc',
        'devices_app.h',
        'usb/device_impl.cc',
        'usb/device_impl.h',
        'usb/device_manager_impl.cc',
        'usb/device_manager_impl.h',
        'usb/type_converters.cc',
        'usb/type_converters.h',
      ],
      'dependencies': [
        'device_usb_mojo_bindings_lib',
        '<(DEPTH)/device/core/core.gyp:device_core',
        '<(DEPTH)/device/usb/usb.gyp:device_usb',
        '<(DEPTH)/mojo/mojo_base.gyp:mojo_application_base',
        '<(DEPTH)/mojo/mojo_base.gyp:mojo_application_bindings',
        '<(DEPTH)/mojo/mojo_base.gyp:mojo_url_type_converters',
        '<(DEPTH)/third_party/mojo/mojo_public.gyp:mojo_cpp_bindings',
      ],
      'export_dependent_settings': [
        '<(DEPTH)/mojo/mojo_base.gyp:mojo_application_base',
        '<(DEPTH)/mojo/mojo_base.gyp:mojo_application_bindings',
        '<(DEPTH)/third_party/mojo/mojo_public.gyp:mojo_cpp_bindings',
      ],
    },
    {
      'target_name': 'device_usb_mojo_bindings',
      'type': 'none',
      'variables': {
        'mojom_files': [
          'usb/public/interfaces/device.mojom',
          'usb/public/interfaces/device_manager.mojom',
          'usb/public/interfaces/permission_provider.mojom',
        ],
      },
      'includes': [
        '../../third_party/mojo/mojom_bindings_generator_explicit.gypi',
      ],
    },
    {
      'target_name': 'device_usb_mojo_bindings_lib',
      'type': 'static_library',
      'dependencies': [
        'device_usb_mojo_bindings',
      ],
    },
    {
      'target_name': 'devices_app_public_cpp',
      'type': 'static_library',
      'sources': [
        'public/cpp/constants.cc',
        'public/cpp/constants.h',
      ],
      'dependencies': [
        'devices_app_lib',
      ],
    },
    {
      'target_name': 'devices_app_public_cpp_factory',
      'type': 'static_library',
      'sources': [
        'public/cpp/devices_app_factory.cc',
        'public/cpp/devices_app_factory.h',
      ],
      'dependencies': [
        'devices_app_lib',
      ],
    },
  ],
}
