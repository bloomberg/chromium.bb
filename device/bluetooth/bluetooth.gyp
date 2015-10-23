# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      # GN version: //device/bluetooth
      'target_name': 'device_bluetooth',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../crypto/crypto.gyp:crypto',
        '../../net/net.gyp:net',
        '../../ui/base/ui_base.gyp:ui_base',
        'bluetooth_strings.gyp:bluetooth_strings',
        'uribeacon',
      ],
      'defines': [
        'DEVICE_BLUETOOTH_IMPLEMENTATION',
      ],
      'sources': [
        # Note: file list duplicated in GN build.
        'android/bluetooth_jni_registrar.cc',
        'android/bluetooth_jni_registrar.h',
        'android/wrappers.cc',
        'android/wrappers.h',
        'bluetooth_adapter.cc',
        'bluetooth_adapter.h',
        'bluetooth_adapter_android.cc',
        'bluetooth_adapter_android.h',
        'bluetooth_adapter_chromeos.cc',
        'bluetooth_adapter_chromeos.h',
        'bluetooth_adapter_factory.cc',
        'bluetooth_adapter_factory.h',
        'bluetooth_adapter_mac.h',
        'bluetooth_adapter_mac.mm',
        "bluetooth_adapter_profile_chromeos.cc",
        "bluetooth_adapter_profile_chromeos.h",
        'bluetooth_adapter_win.cc',
        'bluetooth_adapter_win.h',
        'bluetooth_advertisement.cc',
        'bluetooth_advertisement.h',
        'bluetooth_advertisement_chromeos.cc',
        'bluetooth_advertisement_chromeos.h',
        'bluetooth_audio_sink.cc',
        'bluetooth_audio_sink.h',
        'bluetooth_audio_sink_chromeos.cc',
        'bluetooth_audio_sink_chromeos.h',
        'bluetooth_channel_mac.mm',
        'bluetooth_channel_mac.h',
        'bluetooth_classic_device_mac.mm',
        'bluetooth_classic_device_mac.h',
        'bluetooth_device.cc',
        'bluetooth_device.h',
        'bluetooth_device_android.h',
        'bluetooth_device_android.cc',
        'bluetooth_device_chromeos.cc',
        'bluetooth_device_chromeos.h',
        'bluetooth_device_mac.mm',
        'bluetooth_device_mac.h',
        'bluetooth_device_win.cc',
        'bluetooth_device_win.h',
        'bluetooth_discovery_filter.cc',
        'bluetooth_discovery_filter.h',
        'bluetooth_discovery_manager_mac.mm',
        'bluetooth_discovery_manager_mac.h',
        'bluetooth_discovery_session.cc',
        'bluetooth_discovery_session.h',
        'bluetooth_discovery_session_outcome.h',
        'bluetooth_gatt_characteristic.cc',
        'bluetooth_gatt_characteristic.h',
        'bluetooth_gatt_connection.cc',
        'bluetooth_gatt_connection.h',
        'bluetooth_gatt_connection_chromeos.cc',
        'bluetooth_gatt_connection_chromeos.h',
        'bluetooth_gatt_descriptor.cc',
        'bluetooth_gatt_descriptor.h',
        'bluetooth_gatt_notify_session.cc',
        'bluetooth_gatt_notify_session.h',
        'bluetooth_gatt_notify_session_chromeos.cc',
        'bluetooth_gatt_notify_session_chromeos.h',
        'bluetooth_gatt_service.cc',
        'bluetooth_gatt_service.h',
        'bluetooth_init_win.cc',
        'bluetooth_init_win.h',
        'bluetooth_l2cap_channel_mac.mm',
        'bluetooth_l2cap_channel_mac.h',
        'bluetooth_low_energy_central_manager_delegate.mm',
        'bluetooth_low_energy_central_manager_delegate.h',
        'bluetooth_low_energy_defs_win.cc',
        'bluetooth_low_energy_defs_win.h',
        'bluetooth_low_energy_device_mac.h',
        'bluetooth_low_energy_device_mac.mm',
        'bluetooth_low_energy_discovery_manager_mac.h',
        'bluetooth_low_energy_discovery_manager_mac.mm',
        'bluetooth_low_energy_win.cc',
        'bluetooth_low_energy_win.h',
        'bluetooth_pairing_chromeos.cc',
        'bluetooth_pairing_chromeos.h',
        'bluetooth_remote_gatt_characteristic_android.cc',
        'bluetooth_remote_gatt_characteristic_android.h',
        'bluetooth_remote_gatt_characteristic_chromeos.cc',
        'bluetooth_remote_gatt_characteristic_chromeos.h',
        'bluetooth_remote_gatt_descriptor_chromeos.cc',
        'bluetooth_remote_gatt_descriptor_chromeos.h',
        'bluetooth_remote_gatt_service_android.cc',
        'bluetooth_remote_gatt_service_android.h',
        'bluetooth_remote_gatt_service_chromeos.cc',
        'bluetooth_remote_gatt_service_chromeos.h',
        'bluetooth_rfcomm_channel_mac.mm',
        'bluetooth_rfcomm_channel_mac.h',
        'bluetooth_service_record_win.cc',
        'bluetooth_service_record_win.h',
        'bluetooth_socket.cc',
        'bluetooth_socket.h',
        'bluetooth_socket_chromeos.cc',
        'bluetooth_socket_chromeos.h',
        'bluetooth_socket_mac.h',
        'bluetooth_socket_mac.mm',
        'bluetooth_socket_net.cc',
        'bluetooth_socket_net.h',
        'bluetooth_socket_thread.cc',
        'bluetooth_socket_thread.h',
        'bluetooth_socket_win.cc',
        'bluetooth_socket_win.h',
        'bluetooth_task_manager_win.cc',
        'bluetooth_task_manager_win.h',
        'bluetooth_uuid.cc',
        'bluetooth_uuid.h',
      ],
      'conditions': [
        ['chromeos==1', {
          'dependencies': [
            '../../build/linux/system.gyp:dbus',
            '../../chromeos/chromeos.gyp:chromeos',
            '../../dbus/dbus.gyp:dbus',
          ],
          'export_dependent_settings': [
            '../../build/linux/system.gyp:dbus'
          ]
        }],
        ['OS == "android"', {
          'dependencies': [
            'device_bluetooth_java',
            'device_bluetooth_jni_headers',
          ],
        }],
        ['OS=="win"', {
          # The following two blocks are duplicated. They apply to static lib
          # and shared lib configurations respectively.
          'all_dependent_settings': {  # For static lib, apply to dependents.
            'msvs_settings': {
              'VCLinkerTool': {
                'DelayLoadDLLs': [
                  'BluetoothApis.dll',
                  # Despite MSDN stating that Bthprops.dll contains the
                  # symbols declared by bthprops.lib, they actually reside here:
                  'Bthprops.cpl',
                  'setupapi.dll',
                ],
              },
            },
          },
          'msvs_settings': {  # For shared lib, apply to self.
            'VCLinkerTool': {
              'DelayLoadDLLs': [
                'BluetoothApis.dll',
                # Despite MSDN stating that Bthprops.dll contains the
                # symbols declared by bthprops.lib, they actually reside here:
                'Bthprops.cpl',
                'setupapi.dll',
              ],
            },
          },
        }],
        ['OS=="mac"', {
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/IOBluetooth.framework',
            ],
            'conditions': [
              ['mac_sdk == "10.10"', {
                'xcode_settings': {
                  # In the OSX 10.10 SDK, CoreBluetooth became a top level
                  # framework. Previously, it was nested in IOBluetooth. In
                  # order for Chrome to run on OSes older than OSX 10.10, the
                  # top level CoreBluetooth framework must be weakly linked.
                  'OTHER_LDFLAGS': [
                    '-weak_framework CoreBluetooth',
                  ],
                },
              }],
            ],
          },
        }],
      ],
    },
    {
      # GN version: //device/bluetooth/uribeacon
      'target_name': 'uribeacon',
      'type': 'static_library',
      'dependencies': [
        '../../base/base.gyp:base',
      ],
      'sources': [
        'uribeacon/uri_encoder.cc',
        'uribeacon/uri_encoder.h'
      ]
    },
    {
      # GN version: //device/bluetooth:mocks
      'target_name': 'device_bluetooth_mocks',
      'type': 'static_library',
      'dependencies': [
        '../../testing/gmock.gyp:gmock',
        'device_bluetooth',
      ],
      'include_dirs': [
        '../../',
      ],
      'sources': [
        # Note: file list duplicated in GN build.
        'test/mock_bluetooth_adapter.cc',
        'test/mock_bluetooth_adapter.h',
        'test/mock_bluetooth_advertisement.cc',
        'test/mock_bluetooth_advertisement.h',
        'test/mock_bluetooth_central_manager_mac.mm',
        'test/mock_bluetooth_central_manager_mac.h',
        'test/mock_bluetooth_device.cc',
        'test/mock_bluetooth_device.h',
        'test/mock_bluetooth_discovery_session.cc',
        'test/mock_bluetooth_discovery_session.h',
        'test/mock_bluetooth_gatt_characteristic.cc',
        'test/mock_bluetooth_gatt_characteristic.h',
        'test/mock_bluetooth_gatt_connection.cc',
        'test/mock_bluetooth_gatt_connection.h',
        'test/mock_bluetooth_gatt_descriptor.cc',
        'test/mock_bluetooth_gatt_descriptor.h',
        'test/mock_bluetooth_gatt_notify_session.cc',
        'test/mock_bluetooth_gatt_notify_session.h',
        'test/mock_bluetooth_gatt_service.cc',
        'test/mock_bluetooth_gatt_service.h',
        'test/mock_bluetooth_socket.cc',
        'test/mock_bluetooth_socket.h',
      ],
    },
  ],
  'conditions': [
    ['OS == "android"', {
      'targets': [
        {
          'target_name': 'device_bluetooth_jni_headers',
          'type': 'none',
          'sources': [
            'android/java/src/org/chromium/device/bluetooth/ChromeBluetoothAdapter.java',
            'android/java/src/org/chromium/device/bluetooth/ChromeBluetoothDevice.java',
            'android/java/src/org/chromium/device/bluetooth/ChromeBluetoothRemoteGattService.java',
            'android/java/src/org/chromium/device/bluetooth/Wrappers.java',
          ],
          'variables': {
            'jni_gen_package': 'device_bluetooth',
          },
          'includes': [ '../../build/jni_generator.gypi' ],
        },
        {
          'target_name': 'device_bluetooth_java',
          'type': 'none',
          'dependencies': [
            '../../base/base.gyp:base',
          ],
          'variables': {
            'java_in_dir': '../../device/bluetooth/android/java',
          },
          'includes': [ '../../build/java.gypi' ],
        },
      ],
    }],
  ],
}
