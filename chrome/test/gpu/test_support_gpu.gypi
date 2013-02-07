# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is meant to be included into targets which run gpu tests.
{
  'variables': {
     'test_list_out_dir': '<(SHARED_INTERMEDIATE_DIR)/chrome/test/gpu',
     'src_dir': '../../..',
  },
  'dependencies': [
    'browser',
    'chrome',
    'chrome_resources.gyp:chrome_resources',
    'chrome_resources.gyp:chrome_strings',
    'renderer',
    'test_support_common',
    'test_support_ui',
    '<(src_dir)/base/base.gyp:base',
    '<(src_dir)/base/base.gyp:test_support_base',
    '<(src_dir)/net/net.gyp:net_test_support',
    '<(src_dir)/skia/skia.gyp:skia',
    '<(src_dir)/testing/gmock.gyp:gmock',
    '<(src_dir)/testing/gtest.gyp:gtest',
    '<(src_dir)/third_party/icu/icu.gyp:icui18n',
    '<(src_dir)/third_party/icu/icu.gyp:icuuc',
  ],
  'defines': [
    'HAS_OUT_OF_PROC_TEST_RUNNER',
  ],
  'include_dirs': [
    '<(src_dir)',
    '<(test_list_out_dir)',
  ],
  'sources': [
    '<(src_dir)/chrome/test/base/chrome_test_launcher.cc',
  ],
  # hard_dependency is necessary for this target because it has actions
  # that generate a header file included by dependent targets. The header
  # file must be generated before the dependents are compiled. The usual
  # semantics are to allow the two targets to build concurrently.
  'hard_dependency': 1,
  'conditions': [
    ['OS=="win"', {
      'dependencies': [
        'chrome_version_resources',
      ],
      'include_dirs': [
        '<(DEPTH)/third_party/wtl/include',
      ],
      'sources': [
        '<(src_dir)/chrome/app/chrome_dll.rc',
        '<(src_dir)/chrome/app/chrome_dll_resource.h',
        '<(src_dir)/chrome/app/chrome_version.rc.version',
        '<(SHARED_INTERMEDIATE_DIR)/chrome/browser_resources.rc',
        '<(SHARED_INTERMEDIATE_DIR)/chrome/chrome_unscaled_resources.rc',
        '<(SHARED_INTERMEDIATE_DIR)/chrome/common_resources.rc',
        '<(SHARED_INTERMEDIATE_DIR)/chrome/extensions_api_resources.rc',
        '<(SHARED_INTERMEDIATE_DIR)/chrome_version/other_version.rc',
        '<(SHARED_INTERMEDIATE_DIR)/content/content_resources.rc',
        '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.rc',
        '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_chromium_resources.rc',
      ],
      'conditions': [
        ['win_use_allocator_shim==1', {
          'dependencies': [
            '<(allocator_target)',
          ],
        }],
      ],
      'configurations': {
        'Debug': {
          'msvs_settings': {
            'VCLinkerTool': {
              'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
            },
          },
        },
      },
    }],
    ['OS=="mac"', {
      # See comments about "xcode_settings" elsewhere in this file.
      'xcode_settings': {'OTHER_LDFLAGS': ['-Wl,-ObjC']},
    }],
    ['toolkit_uses_gtk == 1', {
       'dependencies': [
         '<(src_dir)/build/linux/system.gyp:gtk',
       ],
    }],
    ['toolkit_uses_gtk == 1 or chromeos==1 or (OS=="linux" and use_aura==1)', {
      'dependencies': [
        '<(src_dir)/build/linux/system.gyp:ssl',
      ],
    }],
    ['toolkit_views==1', {
      'dependencies': [
       '<(src_dir)/ui/views/views.gyp:views',
      ],
    }],
    ['OS=="android"', {
      'dependencies!': [
        'chrome',
      ],
      'dependencies': [
        '<@(chromium_dependencies)',
        'chrome_resources.gyp:packed_resources',
        'chrome_resources.gyp:packed_extra_resources',
      ],
    }],
  ],
}
