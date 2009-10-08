{
  'conditions': [
    ['OS=="linux" or OS=="win"', {
      'targets': [
        {
          # TODO: enable on mac.
          'target_name': 'interactive_ui_tests',
          'type': 'executable',
          'msvs_guid': '018D4F38-6272-448F-A864-976DA09F05D0',
          'dependencies': [
            '../../chrome.gyp:chrome',
            '../../chrome.gyp:chrome_resources',
            '../../chrome.gyp:chrome_strings',
            '../../chrome.gyp:debugger',
            '../../chrome.gyp:test_support_common',
            '../../chrome.gyp:test_support_ui',
            '../../chrome.gyp:syncapi',
            '../../../third_party/hunspell/hunspell.gyp:hunspell',
            '../../../net/net.gyp:net_resources',
            '../../../skia/skia.gyp:skia',
            '../../../third_party/icu/icu.gyp:icui18n',
            '../../../third_party/libpng/libpng.gyp:libpng',
            '../../../third_party/libxml/libxml.gyp:libxml',
            '../../../third_party/zlib/zlib.gyp:zlib',
            '../../../testing/gtest.gyp:gtest',
            '../../../third_party/npapi/npapi.gyp:npapi',
            # run time dependency
            '../../../webkit/webkit.gyp:webkit_resources',
          ],
          'include_dirs': [
            '../../../',
          ],
          # TODO(phajdan.jr): Only temporary, to make transition easier.
          'defines': [ 'ALLOW_IN_PROC_BROWSER_TEST' ],
          'sources': [
            '../../browser/autocomplete/autocomplete_edit_view_browsertest.cc',
            '../../browser/browser_focus_uitest.cc',
            '../../browser/debugger/devtools_sanity_unittest.cc',
            '../../browser/views/bookmark_bar_view_test.cc',
            '../../browser/blocked_popup_container_interactive_uitest.cc',
            '../../browser/views/find_bar_host_interactive_uitest.cc',
            '../../browser/views/tabs/tab_dragging_test.cc',
            '../../test/in_process_browser_test.cc',
            '../../test/in_process_browser_test.h',
            '../../test/interactive_ui/npapi_interactive_test.cc',
            '../../test/interactive_ui/view_event_test_base.cc',
            '../../test/interactive_ui/view_event_test_base.h',
          ],
          'conditions': [
            ['OS=="linux"', {
              'dependencies': [
                '../../../build/linux/system.gyp:gtk',
                '../../../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
              ],
              'sources!': [
                # TODO(port)
                '../../browser/views/bookmark_bar_view_test.cc',
                '../../browser/views/find_bar_host_interactive_uitest.cc',
                '../../browser/views/tabs/tab_dragging_test.cc',
                '../../test/interactive_ui/npapi_interactive_test.cc',
                '../../test/interactive_ui/view_event_test_base.cc',
                '../../test/interactive_ui/view_event_test_base.h',
              ],
            }],  # OS=="linux"
            ['target_arch!="x64"', {
              'dependencies': [
                # run time dependency
                '../../../webkit/tools/test_shell/test_shell.gyp:npapi_test_plugin',
              ],
            }],  # target_arch
            ['OS=="linux" and toolkit_views==1', {
              'dependencies': [
                '../../../views/views.gyp:views',
              ],
            }],
            ['OS=="mac"', {
              'sources!': [
                # TODO(port)
                '../../browser/autocomplete/autocomplete_edit_view_browsertest.cc',
                '../../browser/browser_focus_uitest.cc',
                '../../browser/debugger/devtools_sanity_unittest.cc',
                '../../browser/views/bookmark_bar_view_test.cc',
                '../../browser/blocked_popup_container_interactive_uitest.cc',
                '../../browser/views/find_bar_host_interactive_uitest.cc',
                '../../browser/views/tabs/tab_dragging_test.cc',
                '../../test/interactive_ui/npapi_interactive_test.cc',
                '../../test/interactive_ui/view_event_test_base.cc',
                '../../test/interactive_ui/view_event_test_base.h',
              ],
            }],  # OS=="mac"
            ['OS=="win"', {
              'include_dirs': [
                '../../third_party/wtl/include',
              ],
              'dependencies': [
                '../../chrome.gyp:chrome_dll_version',
                '../../chrome.gyp:crash_service',  # run time dependency
                '../../installer/installer.gyp:installer_util_strings',
                '../../../views/views.gyp:views',
                '../../../third_party/tcmalloc/tcmalloc.gyp:tcmalloc',
              ],
              'sources': [
                '../../../webkit/glue/resources/aliasb.cur',
                '../../../webkit/glue/resources/cell.cur',
                '../../../webkit/glue/resources/col_resize.cur',
                '../../../webkit/glue/resources/copy.cur',
                '../../../webkit/glue/resources/row_resize.cur',
                '../../../webkit/glue/resources/vertical_text.cur',
                '../../../webkit/glue/resources/zoom_in.cur',
                '../../../webkit/glue/resources/zoom_out.cur',

                '../../app/chrome_dll.rc',
                '../../test/data/resource.rc',

                # TODO:  It would be nice to have these pulled in
                # automatically from direct_dependent_settings in
                # their various targets (net.gyp:net_resources, etc.),
                # but that causes errors in other targets when
                # resulting .res files get referenced multiple times.
                '<(SHARED_INTERMEDIATE_DIR)/chrome/browser_resources.rc',
                '<(SHARED_INTERMEDIATE_DIR)/chrome/common_resources.rc',
                '<(SHARED_INTERMEDIATE_DIR)/chrome/renderer_resources.rc',
                '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.rc',
                '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_resources.rc',
              ],
              'configurations': {
                'Debug': {
                  'msvs_settings': {
                    'VCLinkerTool': {
                      'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
                    },
                  },
                },
              },  # configurations
            }],  # OS=="win"
          ],  # conditions
        }
      ],  # targets
    }],  # 'OS=="linux" or OS=="win"'
    ['OS=="mac"', {
      'targets': [
        {
          # TODO: enable on mac.
          'target_name': 'interactive_ui_tests',
          'type': 'none',
        }
      ]
    }]
  ]  # conditions
}
