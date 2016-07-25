# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/browsing_data/core
      'target_name': 'browsing_data_core',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        'components.gyp:autofill_core_browser',
        'components.gyp:history_core_browser',
        'components.gyp:password_manager_core_browser',
        'components.gyp:sync_driver',
        'components.gyp:webdata_common',
        'prefs/prefs.gyp:prefs',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'browsing_data/core/browsing_data_utils.cc',
        'browsing_data/core/browsing_data_utils.h',
        'browsing_data/core/pref_names.cc',
        'browsing_data/core/pref_names.h',
        'browsing_data/core/counters/autofill_counter.cc',
        'browsing_data/core/counters/autofill_counter.h',
        'browsing_data/core/counters/browsing_data_counter.cc',
        'browsing_data/core/counters/browsing_data_counter.h',
        'browsing_data/core/counters/history_counter.cc',
        'browsing_data/core/counters/history_counter.h',
        'browsing_data/core/counters/passwords_counter.cc',
        'browsing_data/core/counters/passwords_counter.h',
      ],
    },
  ],
  'conditions': [
    ['OS == "android"', {
      'targets': [
        {
          # GN: //components/browsing_data/core:browsing_data_utils_java
          'target_name': 'browsing_data_utils_java',
          'type': 'none',
          'variables': {
            'source_file': 'components/browsing_data/core/browsing_data_utils.h',
          },
          'includes': [ '../build/android/java_cpp_enum.gypi' ],
        },
      ],
    }],
    ['OS != "ios"', {
      'targets': [
        {
          #GN version: //components/browsing_data/content
          'target_name': 'browsing_data_content',
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
            'browsing_data/content/conditional_cache_deletion_helper.cc',
            'browsing_data/content/conditional_cache_deletion_helper.h',
            'browsing_data/content/storage_partition_http_cache_data_remover.cc',
            'browsing_data/content/storage_partition_http_cache_data_remover.h',
          ],
        },
      ],
    }],
  ],
}
