# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      # GN version: //device/vibration:vibration_mojo
      'target_name': 'device_vibration_mojo_bindings',
      'type': 'static_library',
      'includes': [
        '../../third_party/mojo/mojom_bindings_generator.gypi',
      ],
      'sources': [
        'vibration_manager.mojom',
      ],
    },
    {
      # GN version: //device/vibration
      'target_name': 'device_vibration',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../../mojo/mojo_base.gyp:mojo_environment_chromium',
        '../../third_party/mojo/mojo_edk.gyp:mojo_system_impl',
        '../../third_party/mojo/mojo_public.gyp:mojo_cpp_bindings',
        'device_vibration_mojo_bindings',
      ],
      'defines': [
        'DEVICE_VIBRATION_IMPLEMENTATION',
      ],
      'sources': [
        'android/vibration_jni_registrar.cc',
        'android/vibration_jni_registrar.h',
        'vibration_manager_impl.h',
        'vibration_manager_impl_android.cc',
        'vibration_manager_impl_android.h',
        'vibration_manager_impl_default.cc',
      ],
      'conditions': [
        ['OS == "android"', {
          'dependencies': [
            'device_vibration_jni_headers',
          ],
          'sources!': [
            'vibration_manager_impl_default.cc',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['OS == "android"', {
      'targets': [
        {
          'target_name': 'device_vibration_jni_headers',
          'type': 'none',
          'sources': [
            'android/java/src/org/chromium/device/vibration/VibrationProvider.java',
          ],
          'variables': {
            'jni_gen_package': 'device_vibration',
          },
          'includes': [ '../../build/jni_generator.gypi' ],
        },
        {
          'target_name': 'device_vibration_java',
          'type': 'none',
          'dependencies': [
            '../../base/base.gyp:base',
          ],
          'variables': {
            'java_in_dir': '../../device/vibration/android/java',
          },
          'includes': [ '../../build/java.gypi' ],
        },
      ],
    }],
  ],
}
