# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'crash_component_lib',
      'type': 'static_library',
      'sources': [
        'crash/app/crash_reporter_client.cc',
        'crash/app/crash_reporter_client.h',
        'crash/app/crash_keys_win.cc',
        'crash/app/crash_keys_win.h',
      ],
      'include_dirs': [
        '..',
        '../breakpad/src',
      ],
    },
    {
      'variables': {
        'conditions': [
          ['OS == "ios" ', {
            # On IOS there are no files compiled into the library, and we
            # can't have libraries with zero objects.
            'crash_component_target_type%': 'none',
          }, {
            'crash_component_target_type%': 'static_library',
          }],
        ],
      },
      # Note: if you depend on this target, you need to either link in
      # content.gyp:content_common, or add
      # content/public/common/content_switches.cc to your sources.
      #
      # GN version: //components/crash/app
      'target_name': 'crash_component',
      'type': '<(crash_component_target_type)',
      'sources': [
        'crash/app/breakpad_linux.cc',
        'crash/app/breakpad_linux.h',
        'crash/app/breakpad_linux_impl.h',
        'crash/app/breakpad_mac.h',
        'crash/app/breakpad_mac.mm',
        'crash/app/breakpad_win.cc',
        'crash/app/breakpad_win.h',
        'crash/app/hard_error_handler_win.cc',
        'crash/app/hard_error_handler_win.h',
      ],
      'dependencies': [
        'crash_component_lib',
        '../base/base.gyp:base',
      ],
      'defines': ['CRASH_IMPLEMENTATION'],
      'conditions': [
        ['OS=="mac"', {
          'dependencies': [
            '../breakpad/breakpad.gyp:breakpad',
          ],
        }],
        ['OS=="win"', {
          'dependencies': [
            '../breakpad/breakpad.gyp:breakpad_handler',
            '../breakpad/breakpad.gyp:breakpad_sender',
            '../sandbox/sandbox.gyp:sandbox',
          ],
        }],
        ['os_posix == 1 and OS != "mac" and OS != "ios" and android_webview_build != 1', {
          'dependencies': [
            '../breakpad/breakpad.gyp:breakpad_client',
          ],
          'include_dirs': [
            '../breakpad/src',
          ],
        }],
      ],
      'target_conditions': [
        # Need 'target_conditions' to override default filename_rules to include
        # the files on Android.
        ['OS=="android"', {
          'sources/': [
            ['include', '^crash/app/breakpad_linux\\.cc$'],
          ],
        }],
      ],
    },
    {
      # GN version: //components/crash/app:test_support
      'target_name': 'crash_test_support',
      'type': 'none',
      'dependencies': [
        'crash_component_lib',
      ],
      'direct_dependent_settings': {
        'include_dirs' : [
          '../breakpad/src',
        ],
      }
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          # GN version: //components/crash/tools:crash_service
          'target_name': 'breakpad_crash_service',
          'type': 'static_library',
          'dependencies': [
            '../base/base.gyp:base',
            '../breakpad/breakpad.gyp:breakpad_handler',
            '../breakpad/breakpad.gyp:breakpad_sender',
          ],
          'sources': [
            'crash/tools/crash_service.cc',
            'crash/tools/crash_service.h',
          ],
        },
      ],
    }],
    ['OS=="win" and target_arch=="ia32"', {
      'targets': [
        {
          # Note: if you depend on this target, you need to either link in
          # content.gyp:content_common, or add
          # content/public/common/content_switches.cc to your sources.
          'target_name': 'breakpad_win64',
          'type': 'static_library',
          'sources': [
            'crash/app/crash_reporter_client.cc',
            'crash/app/crash_reporter_client.h',
            'crash/app/breakpad_linux.cc',
            'crash/app/breakpad_linux.h',
            'crash/app/breakpad_linux_impl.h',
            'crash/app/breakpad_mac.h',
            'crash/app/breakpad_mac.mm',
            'crash/app/breakpad_win.cc',
            'crash/app/breakpad_win.h',
            # TODO(siggi): test the x64 version too.
            'crash/app/crash_keys_win.cc',
            'crash/app/crash_keys_win.h',
            'crash/app/hard_error_handler_win.cc',
            'crash/app/hard_error_handler_win.h',
          ],
          'defines': [
            'COMPILE_CONTENT_STATICALLY',
            'CRASH_IMPLEMENTATION',
          ],
          'dependencies': [
            '../base/base.gyp:base_win64',
            '../breakpad/breakpad.gyp:breakpad_handler_win64',
            '../breakpad/breakpad.gyp:breakpad_sender_win64',
            '../sandbox/sandbox.gyp:sandbox_win64',
          ],
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
        },
        {
          'target_name': 'breakpad_crash_service_win64',
          'type': 'static_library',
          'dependencies': [
            '../base/base.gyp:base_win64',
            '../breakpad/breakpad.gyp:breakpad_handler_win64',
            '../breakpad/breakpad.gyp:breakpad_sender_win64',
          ],
          'sources': [
            'crash/tools/crash_service.cc',
            'crash/tools/crash_service.h',
          ],
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
        },
      ],
    }],
    ['OS=="mac"', {
      'targets': [
        {
          'target_name': 'breakpad_stubs',
          'type': 'static_library',
          'dependencies': [
            '../base/base.gyp:base',
          ],
          'sources': [
            'crash/app/crash_reporter_client.cc',
            'crash/app/crash_reporter_client.h',
            'crash/app/breakpad_mac.h',
            'crash/app/breakpad_mac_stubs.mm',
          ],
        },
      ],
    }],
    ['os_posix == 1 and OS != "mac" and OS != "ios" and android_webview_build != 1', {
      'targets': [
        {
          # GN version: //components/crash/browser
          'target_name': 'breakpad_host',
          'type': 'static_library',
          'dependencies': [
            'crash_component',
            '../base/base.gyp:base',
            '../breakpad/breakpad.gyp:breakpad_client',
            '../content/content.gyp:content_browser',
            '../content/content.gyp:content_common',
          ],
          'sources': [
            'crash/browser/crash_dump_manager_android.cc',
            'crash/browser/crash_dump_manager_android.h',
            'crash/browser/crash_handler_host_linux.cc',
            'crash/browser/crash_handler_host_linux.h',
          ],
          'include_dirs': [
            '../breakpad/src',
          ],
          'target_conditions': [
            # Need 'target_conditions' to override default filename_rules to include
            # the files on Android.
            ['OS=="android"', {
              'sources/': [
                ['include', '^crash/browser/crash_handler_host_linux\\.cc$'],
              ],
            }],
          ],
        },
      ],
    }],
  ],
}
