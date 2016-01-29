# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions' : [
    ['OS=="android"', {
      'targets': [
        {
          # GN: //components/enhanced_bookmarks:enhanced_bookmarks_java_enums_srcjar
          'target_name': 'enhanced_bookmarks_java_enums_srcjar',
          'type': 'none',
          'variables': {
            'source_file': 'enhanced_bookmarks/enhanced_bookmark_utils.h',
          },
          'includes': [ '../build/android/java_cpp_enum.gypi' ],
        },
      ],
     },
   ],
  ],
}
