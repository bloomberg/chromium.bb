# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This GYPI allows independent customization of the chrome shell in a manner
# similar to content shell (in content_shell.gypi). Notably, this file does
# NOT contain chrome_android_core, which is independent of the chrome shell
# and should be separately customized.
{
  'variables': {
    'package_name': 'chrome_shell_apk',
  },
  'targets': [
    {
      # GN: //chrome/android:chrome_shell_base
      'target_name': 'libchromeshell_base',
      'type': 'none',
      'dependencies': [
        '../base/base.gyp:base',
        'chrome_android_core',
        'chrome.gyp:browser_ui',
        '../content/content.gyp:content_app_browser',
      ],
      'direct_dependent_settings': {
        'ldflags': [
          # Some android targets still depend on --gc-sections to link.
          # TODO: remove --gc-sections for Debug builds (crbug.com/159847).
          '-Wl,--gc-sections',
        ],
      },
      'conditions': [
        [ 'order_profiling!=0', {
          'dependencies': [ '../tools/cygprofile/cygprofile.gyp:cygprofile', ],
        }],
        [ 'use_allocator!="none"', {
          'dependencies': [
            '../base/allocator/allocator.gyp:allocator', ],
        }],
        [ 'cld_version==0 or cld_version==2', {
          'dependencies': [
            # Chrome shell should always use the statically-linked CLD data.
            '<(DEPTH)/third_party/cld_2/cld_2.gyp:cld2_static', ],
        }],
      ],
    },
    {
      # GN: //chrome/android:chrome_shell
      'target_name': 'libchromeshell',
      'type': 'shared_library',
      'sources': [
        'android/shell/chrome_shell_entry_point.cc',
        'android/shell/chrome_main_delegate_chrome_shell_android.cc',
        'android/shell/chrome_main_delegate_chrome_shell_android.h',
      ],
      'dependencies': [
        'libchromeshell_base',
      ],
      'includes': [
        # File 'protection' is based on non-trivial linker magic. TODO(pasko):
        # remove it when crbug.com/424562 is fixed.
        '../base/files/protect_file_posix.gypi',
      ],
    },
    {
      # GN: //chrome/android:chrome_sync_shell
      'target_name': 'libchromesyncshell',
      'type': 'shared_library',
      'sources': [
        'android/shell/chrome_shell_entry_point.cc',
        'android/sync_shell/chrome_main_delegate_chrome_sync_shell_android.cc',
        'android/sync_shell/chrome_main_delegate_chrome_sync_shell_android.h',
      ],
      'dependencies': [
        'libchromeshell_base',
        '../sync/sync.gyp:test_support_sync_fake_server_android',
      ],
    },
    {
      # GN: //chrome/android:chrome_shell_manifest
      'target_name': 'chrome_shell_manifest',
      'type': 'none',
      'variables': {
        'jinja_inputs': ['android/shell/java/AndroidManifest.xml.jinja2'],
        'jinja_output': '<(SHARED_INTERMEDIATE_DIR)/chrome_shell_manifest/AndroidManifest.xml',
      },
      'includes': [ '../build/android/jinja_template.gypi' ],
    },
    {
      # GN: //chrome/android:chrome_shell_apk
      'target_name': 'chrome_shell_apk',
      'type': 'none',
      'dependencies': [
        'chrome_java',
        'chrome_android_paks_copy',
        'libchromeshell',
        '../media/media.gyp:media_java',
      ],
      'variables': {
        'apk_name': 'ChromeShell',
        'android_manifest_path': '<(SHARED_INTERMEDIATE_DIR)/chrome_shell_manifest/AndroidManifest.xml',
        'native_lib_version_name': '<(version_full)',
        'java_in_dir': 'android/shell/java',
        'resource_dir': 'android/shell/res',
        'asset_location': '<(PRODUCT_DIR)/../assets/<(package_name)',
        'native_lib_target': 'libchromeshell',
        'additional_input_paths': [
          '<@(chrome_android_pak_output_resources)',
        ],
      },
      'includes': [ '../build/java_apk.gypi', ],
    },
    {
      # GN: N/A
      # chrome_shell_apk creates a .jar as a side effect. Any java targets
      # that need that .jar in their classpath should depend on this target,
      # chrome_shell_apk_java. Dependents of chrome_shell_apk receive its
      # jar path in the variable 'apk_output_jar_path'.
      # This target should only be used by targets which instrument
      # chrome_shell_apk.
      'target_name': 'chrome_shell_apk_java',
      'type': 'none',
      'dependencies': [
        'chrome_shell_apk',
      ],
      'includes': [ '../build/apk_fake_jar.gypi' ],
    },
    {
      # GN: //chrome/android:chrome_sync_shell_manifest
      'target_name': 'chrome_sync_shell_manifest',
      'type': 'none',
      'variables': {
        'jinja_inputs': ['android/sync_shell/java/AndroidManifest.xml.jinja2'],
        'jinja_output': '<(SHARED_INTERMEDIATE_DIR)/chrome_sync_shell_manifest/AndroidManifest.xml',
      },
      'includes': [ '../build/android/jinja_template.gypi' ],
    },
    {
      # GN: //chrome/android:chrome_sync_shell_apk
      'target_name': 'chrome_sync_shell_apk',
      'type': 'none',
      'dependencies': [
        'chrome_java',
        'chrome_android_paks_copy',
        'libchromesyncshell',
        '../media/media.gyp:media_java',
        '../sync/sync.gyp:sync_java_test_support',
      ],
      'variables': {
        'apk_name': 'ChromeSyncShell',
        'android_manifest_path': '<(SHARED_INTERMEDIATE_DIR)/chrome_sync_shell_manifest/AndroidManifest.xml',
        'R_package': 'org.chromium.chrome.shell',
        'native_lib_version_name': '<(version_full)',
        'java_in_dir': 'android/shell/java',
        'resource_dir': 'android/shell/res',
        'asset_location': '<(PRODUCT_DIR)/../assets/<(package_name)',
        'native_lib_target': 'libchromesyncshell',
        'additional_input_paths': [
          '<@(chrome_android_pak_output_resources)',
        ],
      },
      'includes': [ '../build/java_apk.gypi', ],
    },
    {
      # GN: N/A
      # chrome_sync_shell_apk creates a .jar as a side effect. Any java
      # targets that need that .jar in their classpath should depend on this
      # target. Dependents of chrome_sync_shell_apk receive its jar path in the
      # variable 'apk_output_jar_path'. This target should only be used by
      # targets which instrument chrome_sync_shell_apk.
      'target_name': 'chrome_sync_shell_apk_java',
      'type': 'none',
      'dependencies': [
        'chrome_sync_shell_apk',
      ],
      'includes': [ '../build/apk_fake_jar.gypi' ],
    },
  ],

}
