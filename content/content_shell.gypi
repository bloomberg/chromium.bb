# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'content_shell_lib',
      'type': 'static_library',
      'defines!': ['CONTENT_IMPLEMENTATION'],
      'variables': {
        'chromium_code': 1,
      },
      'dependencies': [
        'content_app',
        'content_browser',
        'content_common',
        'content_gpu',
        'content_plugin',
        'content_ppapi_plugin',
        'content_renderer',
        'content_utility',
        'content_worker',
        'content_resources.gyp:content_resources',
        '../base/base.gyp:base',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../build/temp_gyp/googleurl.gyp:googleurl',
        '../ipc/ipc.gyp:ipc',
        '../net/net.gyp:net',
        '../skia/skia.gyp:skia',
        '../third_party/WebKit/Source/WebKit/chromium/WebKit.gyp:webkit',
        '../ui/ui.gyp:ui',
        '../v8/tools/gyp/v8.gyp:v8',
        '../webkit/support/webkit_support.gyp:appcache',
        '../webkit/support/webkit_support.gyp:database',
        '../webkit/support/webkit_support.gyp:fileapi',
        '../webkit/support/webkit_support.gyp:glue',
        '../webkit/support/webkit_support.gyp:quota',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'browser/tab_contents/tab_contents_view_win.cc',
        'browser/tab_contents/tab_contents_view_win.h',
        'shell/shell.cc',
        'shell/shell.h',
        'shell/shell_gtk.cc',
        'shell/shell_mac.mm',
        'shell/shell_win.cc',
        'shell/shell_browser_context.cc',
        'shell/shell_browser_context.h',
        'shell/shell_browser_main.cc',
        'shell/shell_browser_main.h',
        'shell/shell_content_browser_client.cc',
        'shell/shell_content_browser_client.h',
        'shell/shell_content_client.cc',
        'shell/shell_content_client.h',
        'shell/shell_content_plugin_client.cc',
        'shell/shell_content_plugin_client.h',
        'shell/shell_content_renderer_client.cc',
        'shell/shell_content_renderer_client.h',
        'shell/shell_content_utility_client.cc',
        'shell/shell_content_utility_client.h',
        'shell/shell_download_manager_delegate.cc',
        'shell/shell_download_manager_delegate.h',
        'shell/shell_main_delegate.cc',
        'shell/shell_main_delegate.h',
        'shell/shell_resource_context.cc',
        'shell/shell_resource_context.h',
        'shell/shell_url_request_context_getter.cc',
        'shell/shell_url_request_context_getter.h',
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'SubSystem': '2',  # Set /SUBSYSTEM:WINDOWS
        },
      },
      'conditions': [
        ['OS=="win" and win_use_allocator_shim==1', {
          'dependencies': [
            '../base/allocator/allocator.gyp:allocator',
          ],
        }],
        ['OS=="win"', {
          'resource_include_dirs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit',
          ],
          'dependencies': [
            '<(DEPTH)/net/net.gyp:net_resources',
            '<(DEPTH)/webkit/support/webkit_support.gyp:webkit_resources',
            '<(DEPTH)/webkit/support/webkit_support.gyp:webkit_strings',
          ],
          'configurations': {
            'Debug_Base': {
              'msvs_settings': {
                'VCLinkerTool': {
                  'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
                },
              },
            },
          },
        }],
      ],
    },
    {
      # We build a minimal set of resources so WebKit in content_shell has
      # access to necessary resources.
      'target_name': 'content_shell_pak',
      'type': 'none',
      'variables': {
        'repack_path': '<(DEPTH)/tools/grit/grit/format/repack.py',
      },
      'actions': [
        {
          'action_name': 'repack_content_shell_pack',
          'variables': {
            'pak_inputs': [
              '<(SHARED_INTERMEDIATE_DIR)/content/content_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources/ui_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources_standard/ui_resources_standard.pak',
              '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_chromium_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_resources.pak',
            ],
            'conditions': [
              ['OS != "mac"', {
                'pak_inputs': [
                  '<(SHARED_INTERMEDIATE_DIR)/ui/gfx/gfx_resources.pak',
                ]
              }],
            ],
          },
          'inputs': [
            '<(repack_path)',
            '<@(pak_inputs)',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/content_shell.pak',
          ],
          'action': ['python', '<(repack_path)', '<@(_outputs)',
                     '<@(pak_inputs)'],
        },
      ],
    },
    {
      'target_name': 'content_shell',
      'type': 'executable',
      'defines!': ['CONTENT_IMPLEMENTATION'],
      'variables': {
        'chromium_code': 1,
      },
      'dependencies': [
        'content_shell_lib',
        'content_shell_pak',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'app/startup_helper_win.cc',
        'shell/shell_main.cc',
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'SubSystem': '2',  # Set /SUBSYSTEM:WINDOWS
        },
      },
      'conditions': [
        ['OS=="win" and win_use_allocator_shim==1', {
          'dependencies': [
            '../base/allocator/allocator.gyp:allocator',
          ],
        }],
        ['OS=="win"', {
          'configurations': {
            'Debug_Base': {
              'msvs_settings': {
                'VCLinkerTool': {
                  'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
                },
              },
            },
          },
        }],
        ['OS == "win" or (toolkit_uses_gtk == 1 and selinux == 0)', {
          'dependencies': [
            '../sandbox/sandbox.gyp:sandbox',
          ],
        }],
      ],
    },
  ],
}
