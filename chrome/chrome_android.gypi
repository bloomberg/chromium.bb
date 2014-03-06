# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'chromium_code': 1,
    'package_name': 'chromium_testshell',
  },
  'includes': [
    'chrome_android_paks.gypi', # Included for the list of pak resources.
  ],
  'targets': [
    {
      'target_name': 'libchromiumtestshell',
      'type': 'shared_library',
      'dependencies': [
        '../base/base.gyp:base',
        'chrome_android_core',
        'chrome.gyp:browser_ui',
        '../content/content.gyp:content_app_browser',
      ],
      'sources': [
        # This file must always be included in the shared_library step to ensure
        # JNI_OnLoad is exported.
        'app/android/chrome_jni_onload.cc',
        'android/shell/chrome_main_delegate_testshell_android.cc',
        'android/shell/chrome_main_delegate_testshell_android.h',
        "android/shell/testshell_google_location_settings_helper.cc",
        "android/shell/testshell_google_location_settings_helper.h",
      ],
      'include_dirs': [
        '../skia/config',
      ],
      'conditions': [
        [ 'order_profiling!=0', {
          'conditions': [
            [ 'OS=="android"', {
              'dependencies': [ '../tools/cygprofile/cygprofile.gyp:cygprofile', ],
            }],
          ],
        }],
        [ 'android_use_tcmalloc==1', {
          'dependencies': [
            '../base/allocator/allocator.gyp:allocator', ],
        }],
      ],
    },
    {
      'target_name': 'chromium_testshell',
      'type': 'none',
      'dependencies': [
        'chrome_java',
        'chromium_testshell_paks',
        'libchromiumtestshell',
        '../media/media.gyp:media_java',
      ],
      'variables': {
        'apk_name': 'ChromiumTestShell',
        'manifest_package_name': 'org.chromium.chrome.shell',
        'java_in_dir': 'android/shell/java',
        'resource_dir': 'android/shell/res',
        'asset_location': '<(PRODUCT_DIR)/../assets/<(package_name)',
        'native_lib_target': 'libchromiumtestshell',
        'native_lib_version_name': '<(version_full)',
        'additional_input_paths': [
          '<@(chrome_android_pak_output_resources)',
        ],
      },
      'includes': [ '../build/java_apk.gypi', ],
    },
    {
      # chromium_testshell creates a .jar as a side effect. Any java targets
      # that need that .jar in their classpath should depend on this target,
      # chromium_testshell_java. Dependents of chromium_testshell receive its
      # jar path in the variable 'apk_output_jar_path'.
      'target_name': 'chromium_testshell_java',
      'type': 'none',
      'dependencies': [
        'chromium_testshell',
      ],
      'includes': [ '../build/apk_fake_jar.gypi' ],
    },
    {
      'target_name': 'chrome_android_core',
      'type': 'static_library',
      'dependencies': [
        'chrome.gyp:browser',
        'chrome.gyp:browser_ui',
        'chrome.gyp:plugin',
        'chrome.gyp:renderer',
        'chrome.gyp:utility',
        '../content/content.gyp:content',
        '../content/content.gyp:content_app_browser',
      ],
      'include_dirs': [
        '..',
        '<(android_ndk_include)',
      ],
      'sources': [
        'app/android/chrome_android_initializer.cc',
        'app/android/chrome_android_initializer.h',
        'app/android/chrome_main_delegate_android.cc',
        'app/android/chrome_main_delegate_android.h',
        'app/chrome_main_delegate.cc',
        'app/chrome_main_delegate.h',
      ],
      'link_settings': {
        'libraries': [
          '-landroid',
          '-ljnigraphics',
        ],
      },
    },
    {
      'target_name': 'chromium_testshell_paks',
      'type': 'none',
      'dependencies': [
        '<(DEPTH)/chrome/chrome_resources.gyp:packed_resources',
        '<(DEPTH)/chrome/chrome_resources.gyp:packed_extra_resources',
      ],
      'copies': [
        {
          'destination': '<(chrome_android_pak_output_folder)',
          'files': [
            '<@(chrome_android_pak_input_resources)',
          ],
        }
      ],
    },
  ],
}
