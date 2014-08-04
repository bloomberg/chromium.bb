# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      # GN version: //device/serial:serial_mojo
      'target_name': 'device_serial_mojo',
      # The type of this target must be none. This is so that resources can
      # depend upon this target for generating the js bindings files. Any
      # generated cpp files must be listed explicitly in device_serial
      'type': 'none',
      'includes': [
        '../../mojo/public/tools/bindings/mojom_bindings_generator.gypi',
      ],
      'sources': [
        'serial.mojom',
      ],
    },
    {
      # GN version: //device/serial
      'target_name': 'device_serial',
      'type': 'static_library',
      'conditions': [
        ['OS=="linux"', {
          'dependencies': [
            '../../build/linux/system.gyp:udev',
          ],
        }],
      ],
      'dependencies': [
        'device_serial_mojo',
        '../../mojo/mojo_base.gyp:mojo_cpp_bindings',
        '../../net/net.gyp:net',
      ],
      'export_dependent_settings': [
        'device_serial_mojo',
        '../../mojo/mojo_base.gyp:mojo_cpp_bindings',
      ],
      'sources': [
        '<(SHARED_INTERMEDIATE_DIR)/device/serial/serial.mojom.cc',
        '<(SHARED_INTERMEDIATE_DIR)/device/serial/serial.mojom.h',
        'buffer.cc',
        'buffer.h',
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
        'device_serial_mojo',
      ],
      'sources': [
        'test_serial_io_handler.cc',
        'test_serial_io_handler.h',
      ],
    },
  ],
}
