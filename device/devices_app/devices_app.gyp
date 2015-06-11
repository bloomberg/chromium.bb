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
      ],
      'dependencies': [
        '<(DEPTH)/device/usb/usb.gyp:device_usb',
        '<(DEPTH)/device/usb/usb.gyp:device_usb_mojo_bindings_lib',
        '<(DEPTH)/mojo/mojo_base.gyp:mojo_application_base',
        '<(DEPTH)/mojo/mojo_base.gyp:mojo_application_bindings',
        '<(DEPTH)/third_party/mojo/mojo_public.gyp:mojo_cpp_bindings',
      ],
      'export_dependent_settings': [
        '<(DEPTH)/mojo/mojo_base.gyp:mojo_application_base',
        '<(DEPTH)/mojo/mojo_base.gyp:mojo_application_bindings',
        '<(DEPTH)/third_party/mojo/mojo_public.gyp:mojo_cpp_bindings',
      ],
    },
  ],
}
