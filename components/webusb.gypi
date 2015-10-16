# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'webusb',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../device/core/core.gyp:device_core',
        '../device/usb/usb.gyp:device_usb',
        'components_webusb_mojo_bindings_lib',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'webusb/webusb_browser_client.h',
        'webusb/webusb_detector.cc',
        'webusb/webusb_detector.h',
      ],
    },
    {
      'target_name': 'components_webusb_mojo_bindings',
      'type': 'none',
      'variables': {
        'mojom_files': [
          'webusb/public/interfaces/webusb_permission_bubble.mojom',
        ],
        'dependencies': [
          '../device/devices_app/devices_app.gyp:device_usb_mojo_bindings',
        ],
      },
      'includes': [
        '../third_party/mojo/mojom_bindings_generator_explicit.gypi',
      ],
    },
    {
      'target_name': 'components_webusb_mojo_bindings_lib',
      'type': 'static_library',
      'dependencies': [
        'components_webusb_mojo_bindings',
        '../device/devices_app/devices_app.gyp:device_usb_mojo_bindings_lib',
      ],
    },
  ],
}
