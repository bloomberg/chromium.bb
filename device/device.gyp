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
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        'bluetooth/bluetooth.gyp:device_bluetooth',
        'bluetooth/bluetooth.gyp:device_bluetooth_mocks',
        'usb/usb.gyp:device_usb',
      ],
      'sources': [
        'bluetooth/bluetooth_adapter_mac_unittest.mm',
        'bluetooth/bluetooth_adapter_win_unittest.cc',
        'bluetooth/bluetooth_device_win_unittest.cc',
        'bluetooth/bluetooth_experimental_chromeos_unittest.cc',
        'bluetooth/bluetooth_profile_chromeos_unittest.cc',
        'bluetooth/bluetooth_service_record_mac_unittest.mm',
        'bluetooth/bluetooth_service_record_win_unittest.cc',
        'bluetooth/bluetooth_task_manager_win_unittest.cc',
        'bluetooth/bluetooth_utils_unittest.cc',
        'test/run_all_unittests.cc',
        'usb/usb_ids_unittest.cc',
      ],
      'conditions': [
        ['chromeos==1', {
          'dependencies': [
            '../build/linux/system.gyp:dbus',
            '../chromeos/chromeos.gyp:chromeos_test_support',
            '../chromeos/chromeos.gyp:chromeos_test_support_without_gmock',
            '../dbus/dbus.gyp:dbus',
          ]
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
            ['linux_use_tcmalloc == 1', {
              'dependencies': [
                '../base/allocator/allocator.gyp:allocator',
              ],
            }],
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['OS=="linux"', {
      'targets': [
        {
          # Protobuf compiler / generator for the MtpFileEntry and
          # MtpFileEntries protocol buffers.
          'target_name': 'mtp_file_entry_proto',
          'type': 'static_library',
          'sources': [
            '../third_party/cros_system_api/dbus/mtp_file_entry.proto',
          ],
          'variables': {
            'proto_in_dir': '../third_party/cros_system_api/dbus',
            'proto_out_dir': 'device/media_transfer_protocol',
          },
          'includes': ['../build/protoc.gypi'],
        },
        {
          # Protobuf compiler / generator for the MtpStorageInfo protocol
          # buffer.
          'target_name': 'mtp_storage_info_proto',
          'type': 'static_library',
          'sources': [
            '../third_party/cros_system_api/dbus/mtp_storage_info.proto',
          ],
          'variables': {
            'proto_in_dir': '../third_party/cros_system_api/dbus',
            'proto_out_dir': 'device/media_transfer_protocol',
          },
          'includes': ['../build/protoc.gypi'],
        },
        {
          'target_name': 'device_media_transfer_protocol',
          'type': 'static_library',
          'dependencies': [
            '../build/linux/system.gyp:dbus',
            'mtp_file_entry_proto',
            'mtp_storage_info_proto',
          ],
          'sources': [
            'media_transfer_protocol/media_transfer_protocol_daemon_client.cc',
            'media_transfer_protocol/media_transfer_protocol_daemon_client.h',
            'media_transfer_protocol/media_transfer_protocol_manager.cc',
            'media_transfer_protocol/media_transfer_protocol_manager.h',
          ],
        },
      ],
    }],
  ],
}
