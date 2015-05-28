# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'device_usb',
      'type': 'static_library',
      'dependencies': [
        'device_usb_mojo_bindings_lib',
        '../../components/components.gyp:device_event_log_component',
        '../../net/net.gyp:net',
        '../../third_party/libusb/libusb.gyp:libusb',
        '../../third_party/mojo/mojo_public.gyp:mojo_cpp_bindings',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'device_impl.cc',
        'device_impl.h',
        'device_manager_impl.cc',
        'device_manager_impl.h',
        # TODO(rockot/reillyg): Split out public sources into their own target
        # once all device/usb consumers have transitioned to the new public API.
        'public/cpp/device_manager_delegate.h',
        'public/cpp/device_manager_factory.h',
        'type_converters.cc',
        'type_converters.h',
        'usb_context.cc',
        'usb_context.h',
        'usb_descriptors.cc',
        'usb_descriptors.h',
        'usb_device_impl.cc',
        'usb_device_impl.h',
        'usb_device.cc',
        'usb_device.h',
        'usb_device_filter.cc',
        'usb_device_filter.h',
        'usb_device_handle_impl.cc',
        'usb_device_handle_impl.h',
        'usb_device_handle.h',
        'usb_error.cc',
        'usb_error.h',
        'usb_ids.cc',
        'usb_ids.h',
        'usb_service.cc',
        'usb_service.h',
        'usb_service_impl.cc',
        'usb_service_impl.h',
      ],
      'actions': [
        {
          'action_name': 'generate_usb_ids',
          'variables': {
            'usb_ids_path%': '../../third_party/usb_ids/usb.ids',
            'usb_ids_py_path': 'tools/usb_ids.py',
          },
          'inputs': [
            '<(usb_ids_path)',
            '<(usb_ids_py_path)',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/device/usb/usb_ids_gen.cc',
          ],
          'action': [
            'python',
            '<(usb_ids_py_path)',
            '-i', '<(usb_ids_path)',
            '-o', '<@(_outputs)',
          ],
          'process_outputs_as_sources': 1,
        },
      ],
      'conditions': [
        ['use_udev == 1', {
          'dependencies': [
            '../udev_linux/udev.gyp:udev_linux',
          ],
        }],
        ['chromeos==1', {
          'dependencies': [
            '../../chromeos/chromeos.gyp:chromeos',
          ],
        }],
      ]
    },
    {
      'target_name': 'device_usb_mojo_bindings',
      'type': 'none',
      'variables': {
        'mojom_files': [
          'public/interfaces/device.mojom',
          'public/interfaces/device_manager.mojom',
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
      'target_name': 'device_usb_mocks',
      'type': 'static_library',
      'include_dirs': [
        '../..',
      ],
      'dependencies': [
        '../../testing/gmock.gyp:gmock',
        'device_usb',
      ],
      'sources': [
        'mock_usb_device.cc',
        'mock_usb_device.h',
        'mock_usb_device_handle.cc',
        'mock_usb_device_handle.h',
        'mock_usb_service.cc',
        'mock_usb_service.h',
      ],
    },
  ],
}
