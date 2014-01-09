# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'variables': {
      'breakpad_component_target': 0,
    },
    'target_conditions': [
      ['breakpad_component_target==1', {
        'defines': ['BREAKPAD_IMPLEMENTATION'],
        'sources': [
          'breakpad/app/breakpad_client.cc',
          'breakpad/app/breakpad_client.h',
          'breakpad/app/breakpad_linux.cc',
          'breakpad/app/breakpad_linux.h',
          'breakpad/app/breakpad_linux_impl.h',
          'breakpad/app/breakpad_mac.h',
          'breakpad/app/breakpad_mac.mm',
          'breakpad/app/breakpad_win.cc',
          'breakpad/app/breakpad_win.h',
          'breakpad/app/hard_error_handler_win.cc',
          'breakpad/app/hard_error_handler_win.h',
        ],
      }],
    ],
  },
  'targets': [
    {
      # Note: if you depend on this target, you need to either link in
      # content.gyp:content_common, or add
      # content/public/common/content_switches.cc to your sources.
      'target_name': 'breakpad_component',
      'type': 'static_library',
      'variables': {
        'breakpad_component_target': 1,
      },
      'dependencies': [
        '../base/base.gyp:base',
      ],
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
            ['include', '^breakpad/app/breakpad_linux\\.cc$'],
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'breakpad_crash_service',
          'type': 'static_library',
          'dependencies': [
            '../base/base.gyp:base',
            '../breakpad/breakpad.gyp:breakpad_handler',
            '../breakpad/breakpad.gyp:breakpad_sender',
          ],
          'sources': [
            'breakpad/tools/crash_service.cc',
            'breakpad/tools/crash_service.h',
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
          'variables': {
            'breakpad_component_target': 1,
          },
          'defines': [
            'COMPILE_CONTENT_STATICALLY',
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
            'breakpad/tools/crash_service.cc',
            'breakpad/tools/crash_service.h',
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
            'breakpad/app/breakpad_client.cc',
            'breakpad/app/breakpad_client.h',
            'breakpad/app/breakpad_mac.h',
            'breakpad/app/breakpad_mac_stubs.mm',
          ],
        },
      ],
    }],
    ['os_posix == 1 and OS != "mac" and OS != "ios" and android_webview_build != 1', {
      'targets': [
        {
          'target_name': 'breakpad_host',
          'type': 'static_library',
          'dependencies': [
            'breakpad_component',
            '../base/base.gyp:base',
            '../breakpad/breakpad.gyp:breakpad_client',
            '../content/content.gyp:content_browser',
            '../content/content.gyp:content_common',
          ],
          'sources': [
            'breakpad/browser/crash_dump_manager_android.cc',
            'breakpad/browser/crash_dump_manager_android.h',
            'breakpad/browser/crash_handler_host_linux.cc',
            'breakpad/browser/crash_handler_host_linux.h',
          ],
          'include_dirs': [
            '../breakpad/src',
          ],
          'target_conditions': [
            # Need 'target_conditions' to override default filename_rules to include
            # the files on Android.
            ['OS=="android"', {
              'sources/': [
                ['include', '^breakpad/browser/crash_handler_host_linux\\.cc$'],
              ],
            }],
          ],
        },
      ],
    }],
  ],
}
