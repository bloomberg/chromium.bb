# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    'chrome_android_paks.gypi', # Included for the list of pak resources.
  ],
  'targets': [
    {
      'target_name': 'libchromiumtestshell',
      'type': 'shared_library',
      'dependencies': [
        'chrome_android_core',
      ],
      'sources': [
        'android/testshell/chrome_main_delegate_testshell_android.cc',
        'android/testshell/chrome_main_delegate_testshell_android.h',
        'android/testshell/testshell_entry_point.cc',
        'android/testshell/testshell_stubs.cc',
      ],
      'include_dirs': [
        '<(SHARED_INTERMEDIATE_DIR)/android',
        '../skia/config',
      ],
      'conditions': [
        [ 'order_profiling!=0', {
          'conditions': [
            [ 'OS="android"', {
              'dependencies': [ '../tools/cygprofile/cygprofile.gyp:cygprofile', ],
            }],
          ],
        }],
      ],
    },
    {
      'target_name': 'chromium_testshell',
      'type': 'none',
      'dependencies': [
        'chrome_android_paks',
        'libchromiumtestshell',
      ],
      'actions': [
        {
          'action_name': 'copy_and_strip_so',
          'inputs': ['<(SHARED_LIB_DIR)/libchromiumtestshell.so'],
          'outputs': ['<(PRODUCT_DIR)/chromium_testshell/libs/<(android_app_abi)/libchromiumtestshell.so'],
          'action': [
            '<(android_strip)',
            '--strip-unneeded',
            '<@(_inputs)',
            '-o',
            '<@(_outputs)',
          ],
        },
        {
          'action_name': 'chromium_testshell_generate_apk',
          'inputs': [
            '<@(chrome_android_pak_output_resources)',
            '<!@(find android/testshell/java/src -name "*.java")',
            # TODO(dtrainor): Uncomment these once resources are added.
            #'<!@(find android/testshell/java/res -name "*.png")',
            #'<!@(find android/testshell/java/res -name "*.xml")',
            '<(PRODUCT_DIR)/lib.java/chromium_base.jar',
            '<(PRODUCT_DIR)/lib.java/chromium_net.jar',
            '<(PRODUCT_DIR)/lib.java/chromium_media.jar',
            '<(PRODUCT_DIR)/lib.java/chromium_content.jar',
            '<(PRODUCT_DIR)/lib.java/chromium_chrome.jar',
            '<(PRODUCT_DIR)/lib.java/chromium_web_contents_delegate_android.jar',
            '<(PRODUCT_DIR)/chromium_testshell/libs/<(android_app_abi)/libchromiumtestshell.so'
          ],
          'outputs': [
            '<(PRODUCT_DIR)/chromium_testshell/ChromiumTestShell-debug.apk',
          ],
          'action': [
            'ant',
            '-DPRODUCT_DIR=<(ant_build_out)',
            '-DAPP_ABI=<(android_app_abi)',
            '-DANDROID_SDK=<(android_sdk)',
            '-DANDROID_SDK_ROOT=<(android_sdk_root)',
            '-DANDROID_SDK_TOOLS=<(android_sdk_tools)',
            '-DANDROID_SDK_VERSION=<(android_sdk_version)',
            '-DANDROID_GDBSERVER=<(android_gdbserver)',
            '-DCONFIGURATION_NAME=<(CONFIGURATION_NAME)',
            '-buildfile',
            '<(DEPTH)/chrome/android/testshell/java/chromium_testshell_apk.xml',
          ],
        },
      ],
    },
    {
      'target_name': 'chrome_android_core',
      'type': 'static_library',
      'dependencies': [
        'chrome.gyp:browser',
        'chrome.gyp:plugin',
        'chrome.gyp:renderer',
        'chrome.gyp:utility',
        '../content/content.gyp:content',
      ],
      'include_dirs': [
        '..',
        '<(SHARED_INTERMEDIATE_DIR)/android',
        '<(SHARED_INTERMEDIATE_DIR)/chrome',
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
      'target_name': 'chrome_android_paks',
      'type': 'none',
      'dependencies': [
        '<(DEPTH)/chrome/chrome_resources.gyp:packed_resources',
        '<(DEPTH)/chrome/chrome_resources.gyp:packed_extra_resources',
      ],
      'copies': [
        {
          'destination': '<(chrome_android_pak_output_folder)',
          'files': [ '<@(chrome_android_pak_input_resources)' ],
        }
      ],
    },
  ],
}

