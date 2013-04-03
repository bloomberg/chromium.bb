# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'device_bluetooth',
      'type': 'static_library',
      'dependencies': [
        'device_bluetooth_strings.gyp:device_bluetooth_strings',
        '../base/base.gyp:base',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../net/net.gyp:net',
        '../third_party/libxml/libxml.gyp:libxml',
        '../ui/ui.gyp:ui'
      ],
      'sources': [
        'bluetooth/bluetooth_adapter.cc',
        'bluetooth/bluetooth_adapter.h',
        'bluetooth/bluetooth_adapter_chromeos.cc',
        'bluetooth/bluetooth_adapter_chromeos.h',
        'bluetooth/bluetooth_adapter_experimental_chromeos.cc',
        'bluetooth/bluetooth_adapter_experimental_chromeos.h',
        'bluetooth/bluetooth_adapter_factory.cc',
        'bluetooth/bluetooth_adapter_factory.h',
        'bluetooth/bluetooth_adapter_mac.h',
        'bluetooth/bluetooth_adapter_mac.mm',
        'bluetooth/bluetooth_adapter_win.cc',
        'bluetooth/bluetooth_adapter_win.h',
        'bluetooth/bluetooth_device.cc',
        'bluetooth/bluetooth_device.h',
        'bluetooth/bluetooth_device_chromeos.cc',
        'bluetooth/bluetooth_device_chromeos.h',
        'bluetooth/bluetooth_device_mac.h',
        'bluetooth/bluetooth_device_mac.mm',
        'bluetooth/bluetooth_device_win.cc',
        'bluetooth/bluetooth_device_win.h',
        'bluetooth/bluetooth_init_win.cc',
        'bluetooth/bluetooth_init_win.h',
        'bluetooth/bluetooth_out_of_band_pairing_data.h',
        'bluetooth/bluetooth_service_record.cc',
        'bluetooth/bluetooth_service_record.h',
        'bluetooth/bluetooth_service_record_chromeos.cc',
        'bluetooth/bluetooth_service_record_chromeos.h',
        'bluetooth/bluetooth_service_record_mac.h',
        'bluetooth/bluetooth_service_record_mac.mm',
        'bluetooth/bluetooth_service_record_win.cc',
        'bluetooth/bluetooth_service_record_win.h',
        'bluetooth/bluetooth_socket.h',
        'bluetooth/bluetooth_socket_chromeos.cc',
        'bluetooth/bluetooth_socket_chromeos.h',
        'bluetooth/bluetooth_socket_mac.h',
        'bluetooth/bluetooth_socket_mac.mm',
        'bluetooth/bluetooth_socket_win.cc',
        'bluetooth/bluetooth_socket_win.h',
        'bluetooth/bluetooth_task_manager_win.cc',
        'bluetooth/bluetooth_task_manager_win.h',
        'bluetooth/bluetooth_utils.cc',
        'bluetooth/bluetooth_utils.h',
      ],
      'conditions': [
        ['chromeos==1', {
          'dependencies': [
            '../build/linux/system.gyp:dbus',
            '../chromeos/chromeos.gyp:chromeos',
            '../dbus/dbus.gyp:dbus',
          ]
        }],
        ['OS=="win"', {
          'all_dependent_settings': {
            'msvs_settings': {
              'VCLinkerTool': {
                'DelayLoadDLLs': [
                  # Despite MSDN stating that Bthprops.dll contains the
                  # symbols declared by bthprops.lib, they actually reside here:
                  'Bthprops.cpl',
                ],
              },
            },
          },
        }],
        ['OS=="mac"', {
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/IOBluetooth.framework',
            ],
          },
        }],
      ],
    },
    {
      'target_name': 'device_bluetooth_mocks',
      'type': 'static_library',
      'dependencies': [
        'device_bluetooth',
        '../testing/gmock.gyp:gmock',
      ],
      'sources': [
        'bluetooth/test/mock_bluetooth_adapter.cc',
        'bluetooth/test/mock_bluetooth_adapter.h',
        'bluetooth/test/mock_bluetooth_device.cc',
        'bluetooth/test/mock_bluetooth_device.h',
        'bluetooth/test/mock_bluetooth_socket.cc',
        'bluetooth/test/mock_bluetooth_socket.h',
      ],
      'include_dirs': [
        '..',
      ],
    },
    {
      'target_name': 'device_usb',
      'type': 'static_library',
      'sources': [
        'usb/usb_ids.cc',
        'usb/usb_ids.h',
      ],
      'include_dirs': [
        '..',
      ],
      'actions': [
        {
          'action_name': 'generate_usb_ids',
          'variables': {
            'usb_ids_path%': '<(DEPTH)/third_party/usb_ids/usb.ids',
            'usb_ids_py_path': '<(DEPTH)/tools/usb_ids/usb_ids.py',
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
    },
    {
      'target_name': 'device_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        'device_bluetooth',
        'device_bluetooth_mocks',
        'device_usb',
        '../base/base.gyp:test_support_base',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
      ],
      'sources': [
        'bluetooth/bluetooth_adapter_chromeos_unittest.cc',
        'bluetooth/bluetooth_adapter_devices_chromeos_unittest.cc',
        'bluetooth/bluetooth_adapter_mac_unittest.mm',
        'bluetooth/bluetooth_adapter_win_unittest.cc',
        'bluetooth/bluetooth_device_win_unittest.cc',
        'bluetooth/bluetooth_service_record_chromeos_unittest.cc',
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
