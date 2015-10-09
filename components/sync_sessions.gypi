# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/sync_sessions
      'target_name': 'sync_sessions',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../sync/sync.gyp:sync',
        'sync_driver',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'sync_sessions/revisit/current_tab_matcher.cc',
        'sync_sessions/revisit/current_tab_matcher.h',
        'sync_sessions/revisit/offset_tab_matcher.cc',
        'sync_sessions/revisit/offset_tab_matcher.h',
        'sync_sessions/revisit/page_equality.h',
        'sync_sessions/revisit/page_visit_observer.h',
        'sync_sessions/revisit/sessions_page_revisit_observer.cc',
        'sync_sessions/revisit/sessions_page_revisit_observer.h',
      ],
      'conditions': [
        ['OS!="ios"', {
          'dependencies': [
            'sessions_content',
          ],
        }, {  # OS==ios
          'dependencies': [
            'sessions_ios',
          ],
        }],
      ],
    },
  ],
}
