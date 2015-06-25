# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'variables': {
      'native_lib_placeholders_file%': '',
      # Temporarily turn off all relocation packing, pending fixes for
      # breakpad problems revealed by moving to Android-style packing.
      # See http://crbug.com/499747.
      'chrome_apk_use_relocation_packer%': 0,
      'conditions': [
        # Use the chromium linker unless cygprofile instrumentation is active.
        # Chromium linker causes instrumentation to return incorrect results.
        ['order_profiling == 0', {
          'chrome_apk_use_chromium_linker%': 1,
          'chrome_apk_load_library_from_zip%': 1,
        }, {
          'chrome_apk_use_chromium_linker%': 0,
          'chrome_apk_load_library_from_zip%': 0,
        }],
      ],
    },
    'asset_location': '<(PRODUCT_DIR)/assets/<(package_name)',
    'java_in_dir_suffix': '/src_dummy',
    'native_lib_version_name': '<(version_full)',
    'proguard_enabled': 'true',
    'proguard_flags_paths': ['<(DEPTH)/chrome/android/java/proguard.flags'],
    'additional_input_paths' : ['<@(chrome_android_pak_output_resources)'],
    'conditions': [
      ['component != "shared_library"', {
        # Only enable the chromium linker on regular builds, since the
        # component build crashes on Android 4.4. See b/11379966
        'use_chromium_linker': '<(chrome_apk_use_chromium_linker)',
        'conditions': [
          ['"<(native_lib_placeholders_file)" != ""', {
            'native_lib_placeholders': ['<!@(cat <(native_lib_placeholders_file))'],
          }],
          # Pack relocations where the chromium linker is enabled. Packing is
          # a no-op if this is not a Release build.
          # TODO: Enable packed relocations for x64. See: b/20532404
          ['chrome_apk_use_chromium_linker == 1 and target_arch != "x64"', {
            'use_relocation_packer': '<(chrome_apk_use_relocation_packer)',
          }],
        ],
      }],
    ],
    'run_findbugs': 0,
  },
  'includes': [ '../../build/java_apk.gypi' ],
}
