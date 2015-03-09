# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'device_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        '../base/base.gyp:test_support_base',
        '../mojo/mojo_base.gyp:mojo_environment_chromium',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/mojo/mojo_edk.gyp:mojo_system_impl',
        '../third_party/mojo/mojo_public.gyp:mojo_cpp_bindings',
        '../tools/usb_gadget/usb_gadget.gyp:usb_gadget',
        'battery/battery.gyp:device_battery',
        'battery/battery.gyp:device_battery_mojo_bindings',
        'bluetooth/bluetooth.gyp:device_bluetooth',
        'bluetooth/bluetooth.gyp:device_bluetooth_mocks',
        'nfc/nfc.gyp:device_nfc',
        'usb/usb.gyp:device_usb',
        'hid/hid.gyp:device_hid',
        'serial/serial.gyp:device_serial',
        'serial/serial.gyp:device_serial_test_util',
      ],
      'sources': [
        'battery/battery_status_manager_linux_unittest.cc',
        'battery/battery_status_manager_win_unittest.cc',
        'battery/battery_status_service_unittest.cc',
        'bluetooth/bluetooth_adapter_mac_unittest.mm',
        'bluetooth/bluetooth_adapter_profile_chromeos_unittest.cc',
        'bluetooth/bluetooth_adapter_unittest.cc',
        'bluetooth/bluetooth_adapter_win_unittest.cc',
        'bluetooth/bluetooth_audio_sink_chromeos_unittest.cc',
        'bluetooth/bluetooth_chromeos_unittest.cc',
        'bluetooth/bluetooth_device_unittest.cc',
        'bluetooth/bluetooth_device_win_unittest.cc',
        'bluetooth/bluetooth_gatt_chromeos_unittest.cc',
        'bluetooth/bluetooth_low_energy_win_unittest.cc',
        'bluetooth/bluetooth_service_record_win_unittest.cc',
        'bluetooth/bluetooth_socket_chromeos_unittest.cc',
        'bluetooth/bluetooth_task_manager_win_unittest.cc',
        'bluetooth/bluetooth_uuid_unittest.cc',
        'hid/hid_connection_unittest.cc',
        'hid/hid_device_filter_unittest.cc',
        'hid/hid_report_descriptor_unittest.cc',
        'hid/input_service_linux_unittest.cc',
        'hid/test_report_descriptors.cc',
        'hid/test_report_descriptors.h',
        'nfc/nfc_chromeos_unittest.cc',
        'nfc/nfc_ndef_record_unittest.cc',
        'serial/data_sink_unittest.cc',
        'serial/data_source_unittest.cc',
        'serial/serial_connection_unittest.cc',
        'serial/serial_service_unittest.cc',
        'test/run_all_unittests.cc',
        'test/usb_test_gadget_impl.cc',
        'usb/usb_context_unittest.cc',
        'usb/usb_device_filter_unittest.cc',
        'usb/usb_device_handle_unittest.cc',
        'usb/usb_ids_unittest.cc',
        'usb/usb_service_unittest.cc',
      ],
      'conditions': [
        ['chromeos==1', {
          'dependencies': [
            '../build/linux/system.gyp:dbus',
            '../chromeos/chromeos.gyp:chromeos_test_support',
            '../chromeos/chromeos.gyp:chromeos_test_support_without_gmock',
            '../dbus/dbus.gyp:dbus',
          ],
          'sources!': [
            'battery/battery_status_manager_linux_unittest.cc',
          ],
        }],
        ['OS=="mac"', {
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/IOBluetooth.framework',
            ],
          },
        }],
        ['os_posix == 1 and OS != "mac" and OS != "android" and OS != "ios"', {
          'conditions': [
            ['use_allocator!="none"', {
              'dependencies': [
                '../base/allocator/allocator.gyp:allocator',
              ],
            }],
          ],
        }],
        ['use_udev==1', {
          'dependencies': [
            'udev_linux/udev.gyp:udev_linux',
          ],
          'sources': [
            'udev_linux/udev_unittest.cc',
          ],
        }],
        ['OS=="linux" and use_udev==0', {
          # Udev is the only Linux implementation. If we're compiling without
          # Udev, disable these unittests.
          'dependencies!': [
            'hid/hid.gyp:device_hid',
            'serial/serial.gyp:device_serial',
            'serial/serial.gyp:device_serial_test_util',
          ],
          'sources/': [
            ['exclude', '^serial/'],
            ['exclude', '^hid/'],
          ],
        }],
        ['use_dbus==0', {
          'sources!': [
            'battery/battery_status_manager_linux_unittest.cc',
          ],
        }],
      ],
    },
  ],
}
