# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'browsing_data',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../content/content.gyp:content_browser',
        '../net/net.gyp:net',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'browsing_data/browsing_data_utils.cc',
        'browsing_data/browsing_data_utils.h',
        'browsing_data/conditional_cache_deletion_helper.cc',
        'browsing_data/conditional_cache_deletion_helper.h',
        'browsing_data/pref_names.cc',
        'browsing_data/pref_names.h',
        'browsing_data/storage_partition_http_cache_data_remover.cc',
        'browsing_data/storage_partition_http_cache_data_remover.h',
        'browsing_data/counters/browsing_data_counter.cc',
        'browsing_data/counters/browsing_data_counter.h',
      ],
    },
  ],
  'conditions': [
    ['OS == "android"', {
      'targets': [
        {
          # GN: //components/browsing_data:browsing_data_utils_java
          'target_name': 'browsing_data_utils_java',
          'type': 'none',
          'variables': {
            'source_file': 'components/browsing_data/browsing_data_utils.h',
          },
          'includes': [ '../build/android/java_cpp_enum.gypi' ],
        },
      ]
    }],
   ],

}
