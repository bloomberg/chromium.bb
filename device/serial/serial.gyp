# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      # GN version: //device/serial
      'target_name': 'device_serial',
      'type': 'static_library',
      'include_dirs': [
        '../..',
      ],
      'conditions': [
        ['OS=="linux"', {
          'dependencies': [
            '../../build/linux/system.gyp:udev',
          ],
        }],
      ],
      'variables': {
        'mojom_base_output_dir': 'device/serial',
      },
      'includes': [
        '../../mojo/public/tools/bindings/mojom_bindings_generator.gypi',
      ],
      'dependencies': [
        '../../mojo/mojo.gyp:mojo_cpp_bindings',
        '../../net/net.gyp:net',
      ],
      'export_dependent_settings': [
        '../../mojo/mojo.gyp:mojo_cpp_bindings',
      ],
      'sources': [
        'serial.mojom',
        'serial_connection.cc',
        'serial_connection.h',
        'serial_connection_factory.cc',
        'serial_connection_factory.h',
        'serial_device_enumerator.cc',
        'serial_device_enumerator.h',
        'serial_device_enumerator_linux.cc',
        'serial_device_enumerator_linux.h',
        'serial_device_enumerator_mac.cc',
        'serial_device_enumerator_mac.h',
        'serial_device_enumerator_win.cc',
        'serial_device_enumerator_win.h',
        'serial_io_handler.cc',
        'serial_io_handler.h',
        'serial_io_handler_posix.cc',
        'serial_io_handler_posix.h',
        'serial_io_handler_win.cc',
        'serial_io_handler_win.h',
        'serial_service_impl.cc',
        'serial_service_impl.h',
      ],
    },
    {
      # GN version: //device/serial:test_util
      'target_name': 'device_serial_test_util',
      'type': 'static_library',
      'dependencies': [
        'device_serial',
      ],
      'sources': [
        'test_serial_io_handler.cc',
        'test_serial_io_handler.h',
      ],
    },
  ],
}
