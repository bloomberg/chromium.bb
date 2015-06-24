# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['archive_media_router_tests==1', {
      'targets': [
        {
          'target_name': 'media_router_e2e_tests_run',
          'type': 'none',
          'dependencies': [
            '../../chrome.gyp:browser_tests',
          ],
          'includes': [
            '../../../build/isolate.gypi',
          ],
          'sources': [
            'media_router_tests.isolate',
          ],
        },  # target_name: 'media_router_e2e_tests_run'
      ],
    }],
  ],
}