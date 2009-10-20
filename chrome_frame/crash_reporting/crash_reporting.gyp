# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../build/common.gypi',
  ],
  'target_defaults': {
    'include_dirs': [
      # all our own includes are relative to src/
      '../..',
    ],
  },
  'targets': [
    {
      'target_name': 'crash_report',
      'type': 'static_library',
      'sources': [
        'crash_report.cc',
        'crash_report.h',
        'vectored_handler-impl.h',
        'vectored_handler.h',
      ],
      'conditions': [
        ['OS=="win"', {
          'dependencies': [
            '../../breakpad/breakpad.gyp:breakpad_handler',
          ],
        }],
      ],
    },
    {
      'target_name': 'vectored_handler_tests',
      'type': 'executable',
      'sources': [
        'vectored_handler_unittest.cc',
      ],
      'dependencies': [
        'crash_report',
        '../../base/base.gyp:base',
        '../../testing/gmock.gyp:gmock',
        '../../testing/gtest.gyp:gtest',
        '../../testing/gtest.gyp:gtestmain',
      ],
    },
  ],
}

# vim: shiftwidth=2:et:ai:tabstop=2

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
