{
  'target_name': 'interactive_ui_tests',
  'type': 'executable',
  'msvs_guid': '018D4F38-6272-448F-A864-976DA09F05D0',
  'dependencies': [
    '<(DEPTH)/chrome/chrome.gyp:chrome',
    '<(DEPTH)/chrome/chrome.gyp:chrome_resources',
    '<(DEPTH)/chrome/chrome.gyp:chrome_strings',
    '<(DEPTH)/chrome/chrome.gyp:debugger',
    '<(DEPTH)/chrome/chrome.gyp:test_support_common',
    '<(DEPTH)/chrome/chrome.gyp:test_support_ui',
    '<(DEPTH)/chrome/chrome.gyp:syncapi',
    '<(DEPTH)/third_party/hunspell/hunspell.gyp:hunspell',
    '<(DEPTH)/net/net.gyp:net_resources',
    '<(DEPTH)/skia/skia.gyp:skia',
    '<(DEPTH)/third_party/icu/icu.gyp:icui18n',
    '<(DEPTH)/third_party/libpng/libpng.gyp:libpng',
    '<(DEPTH)/third_party/libxml/libxml.gyp:libxml',
    '<(DEPTH)/third_party/zlib/zlib.gyp:zlib',
    '<(DEPTH)/testing/gtest.gyp:gtest',
    '<(DEPTH)/third_party/npapi/npapi.gyp:npapi',
    # run time dependency
    '<(DEPTH)/webkit/webkit.gyp:webkit_resources',
  ],
  'include_dirs': [
    '<(DEPTH)',
  ],
  'defines': [ 'ALLOW_IN_PROC_BROWSER_TEST' ],
  'sources': [
    '<(DEPTH)/chrome/browser/autocomplete/autocomplete_edit_view_browsertest.cc',
    '<(DEPTH)/chrome/browser/browser_focus_uitest.cc',
    '<(DEPTH)/chrome/browser/browser_keyevents_browsertest.cc',
    '<(DEPTH)/chrome/browser/debugger/devtools_sanity_unittest.cc',
    '<(DEPTH)/chrome/browser/views/bookmark_bar_view_test.cc',
    '<(DEPTH)/chrome/browser/blocked_popup_container_interactive_uitest.cc',
    '<(DEPTH)/chrome/browser/views/find_bar_host_interactive_uitest.cc',
    '<(DEPTH)/chrome/browser/views/tabs/tab_dragging_test.cc',
    '<(DEPTH)/chrome/test/in_process_browser_test.cc',
    '<(DEPTH)/chrome/test/in_process_browser_test.h',
    '<(DEPTH)/chrome/test/interactive_ui/npapi_interactive_test.cc',
    '<(DEPTH)/chrome/test/interactive_ui/view_event_test_base.cc',
    '<(DEPTH)/chrome/test/interactive_ui/view_event_test_base.h',
    # TODO(jcampan): we should use in_proc_test_runner on Windows.
    '<(DEPTH)/chrome/test/test_launcher/out_of_proc_test_runner.cc',
    '<(DEPTH)/chrome/test/test_launcher/test_runner.cc',
    '<(DEPTH)/chrome/test/test_launcher/test_runner.h',
    '<(DEPTH)/chrome/test/test_launcher/run_all_unittests.cc',
    '<(DEPTH)/chrome/test/unit/chrome_test_suite.h',
  ],
  'conditions': [
    ['OS=="linux"', {
      'dependencies': [
        '<(DEPTH)/build/linux/system.gyp:gtk',
        '<(DEPTH)/tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
      ],
      'sources!': [
        # TODO(port)
        '<(DEPTH)/chrome/browser/views/bookmark_bar_view_test.cc',
        '<(DEPTH)/chrome/browser/views/find_bar_host_interactive_uitest.cc',
        '<(DEPTH)/chrome/browser/views/tabs/tab_dragging_test.cc',
        '<(DEPTH)/chrome/test/interactive_ui/npapi_interactive_test.cc',
        '<(DEPTH)/chrome/test/interactive_ui/view_event_test_base.cc',
        '<(DEPTH)/chrome/test/interactive_ui/view_event_test_base.h',
      ],
    }],  # OS=="linux"
    ['target_arch!="x64" and target_arch!="arm"', {
      'dependencies': [
        # run time dependency
        '<(DEPTH)/webkit/tools/test_shell/test_shell.gyp:npapi_test_plugin',
      ],
    }],  # target_arch
    ['OS=="linux" and toolkit_views==1', {
      'dependencies': [
        '<(DEPTH)/views/views.gyp:views',
      ],
    }],
    ['OS=="mac"', {
      'sources!': [
        # TODO(port)
        '<(DEPTH)/chrome/browser/autocomplete/autocomplete_edit_view_browsertest.cc',
        '<(DEPTH)/chrome/browser/browser_focus_uitest.cc',
        '<(DEPTH)/chrome/browser/browser_keyevents_browsertest.cc',
        '<(DEPTH)/chrome/browser/debugger/devtools_sanity_unittest.cc',
        '<(DEPTH)/chrome/browser/views/bookmark_bar_view_test.cc',
        '<(DEPTH)/chrome/browser/blocked_popup_container_interactive_uitest.cc',
        '<(DEPTH)/chrome/browser/views/find_bar_host_interactive_uitest.cc',
        '<(DEPTH)/chrome/browser/views/tabs/tab_dragging_test.cc',
        '<(DEPTH)/chrome/test/interactive_ui/npapi_interactive_test.cc',
        '<(DEPTH)/chrome/test/interactive_ui/view_event_test_base.cc',
        '<(DEPTH)/chrome/test/interactive_ui/view_event_test_base.h',
      ],
    }],  # OS=="mac"
    ['OS=="win"', {
      'include_dirs': [
        '<(DEPTH)/chrome/third_party/wtl/include',
      ],
      'dependencies': [
        '<(DEPTH)/app/app.gyp:app_resources',
        '<(DEPTH)/base/allocator/allocator.gyp:allocator',
        '<(DEPTH)/chrome/chrome.gyp:chrome_dll_version',
        '<(DEPTH)/chrome/chrome.gyp:crash_service',  # run time dependency
        '<(DEPTH)/chrome/installer/installer.gyp:installer_util_strings',
        '<(DEPTH)/views/views.gyp:views',
      ],
      'sources': [
        '<(DEPTH)/webkit/glue/resources/aliasb.cur',
        '<(DEPTH)/webkit/glue/resources/cell.cur',
        '<(DEPTH)/webkit/glue/resources/col_resize.cur',
        '<(DEPTH)/webkit/glue/resources/copy.cur',
        '<(DEPTH)/webkit/glue/resources/row_resize.cur',
        '<(DEPTH)/webkit/glue/resources/vertical_text.cur',
        '<(DEPTH)/webkit/glue/resources/zoom_in.cur',
        '<(DEPTH)/webkit/glue/resources/zoom_out.cur',

        '<(DEPTH)/chrome/app/chrome_dll.rc',
        '<(DEPTH)/chrome/test/data/resource.rc',

        # TODO:  It would be nice to have these pulled in
        # automatically from direct_dependent_settings in
        # their various targets (net.gyp:net_resources, etc.),
        # but that causes errors in other targets when
        # resulting .res files get referenced multiple times.
        '<(SHARED_INTERMEDIATE_DIR)/app/app_resources/app_resources.rc',
        '<(SHARED_INTERMEDIATE_DIR)/chrome/browser_resources.rc',
        '<(SHARED_INTERMEDIATE_DIR)/chrome/common_resources.rc',
        '<(SHARED_INTERMEDIATE_DIR)/chrome/renderer_resources.rc',
        '<(SHARED_INTERMEDIATE_DIR)/chrome/theme_resources.rc',
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
