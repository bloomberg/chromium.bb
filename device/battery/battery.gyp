# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      # GN version: //device/battery:battery_mojo
      'target_name': 'device_battery_mojo_bindings',
      'type': 'static_library',
      'includes': [
        '../../mojo/public/tools/bindings/mojom_bindings_generator.gypi',
      ],
      'sources': [
        'battery_monitor.mojom',
        'battery_status.mojom',
      ],
    },
    {
      # GN version: //device/battery
      'target_name': 'device_battery',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../../mojo/edk/mojo_edk.gyp:mojo_system_impl',
        '../../mojo/mojo_base.gyp:mojo_environment_chromium',
        '../../mojo/public/mojo_public.gyp:mojo_cpp_bindings',
        'device_battery_mojo_bindings',
      ],
      'defines': [
        'DEVICE_BATTERY_IMPLEMENTATION',
      ],
      'sources': [
        'android/battery_jni_registrar.cc',
        'android/battery_jni_registrar.h',
        'battery_monitor_impl.cc',
        'battery_monitor_impl.h',
        'battery_status_manager_android.cc',
        'battery_status_manager_android.h',
        'battery_status_manager_chromeos.cc',
        'battery_status_manager_default.cc',
        'battery_status_manager_linux.cc',
        'battery_status_manager_linux.h',
        'battery_status_manager_mac.cc',
        'battery_status_manager_win.cc',
        'battery_status_manager_win.h',
        'battery_status_manager.h',
        'battery_status_service.cc',
        'battery_status_service.h',
      ],
      'conditions': [
        ['OS == "android"', {
          'dependencies': [
            'device_battery_jni_headers',
          ],
          'sources!': [
            'battery_status_manager_default.cc',
          ],
        }],
        ['chromeos==1', {
          'dependencies': [
            '../../build/linux/system.gyp:dbus',
            '../../chromeos/chromeos.gyp:power_manager_proto',
          ],
          'sources!': [
            'battery_status_manager_default.cc',
            'battery_status_manager_linux.cc',
          ],
        }],
        ['OS == "linux" and use_dbus==1', {
          'sources!': [
            'battery_status_manager_default.cc',
          ],
          'dependencies': [
            '../../build/linux/system.gyp:dbus',
            '../../dbus/dbus.gyp:dbus',
          ],
        }, {  # OS != "linux" or use_dbus==0
          'sources!': [
            'battery_status_manager_linux.cc',
          ],
        }],
        ['OS == "mac"', {
          'sources!': [
            'battery_status_manager_default.cc',
          ],
        }],
        ['OS == "win"', {
          'sources!': [
            'battery_status_manager_default.cc',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['OS == "android"', {
      'targets': [
        {
          'target_name': 'device_battery_jni_headers',
          'type': 'none',
          'sources': [
            'android/java/src/org/chromium/device/battery/BatteryStatusManager.java',
          ],
          'variables': {
            'jni_gen_package': 'device_battery',
          },
          'includes': [ '../../build/jni_generator.gypi' ],
        },
        {
          'target_name': 'device_battery_java',
          'type': 'none',
          'dependencies': [
            '../../base/base.gyp:base',
          ],
          'variables': {
            'java_in_dir': '../../device/battery/android/java',
          },
          'includes': [ '../../build/java.gypi' ],
        },
        {
          'target_name': 'device_battery_javatests',
          'type': 'none',
          'variables': {
            'java_in_dir': '../../device/battery/android/javatests',
          },
          'dependencies': [
            '../../base/base.gyp:base',
            '../../base/base.gyp:base_java_test_support',
            'device_battery_java',
          ],
          'includes': [ '../../build/java.gypi' ],
        },
      ],
    }],
  ],
}
