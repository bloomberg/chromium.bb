# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/variations
      'target_name': 'variations',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        # List of dependencies is intentionally very minimal. Please avoid
        # adding extra dependencies without first checking with OWNERS.
        '../base/base.gyp:base',
        '../third_party/mt19937ar/mt19937ar.gyp:mt19937ar',
      ],
      'sources': [
        'variations/active_field_trials.cc',
        'variations/active_field_trials.h',
        'variations/android/component_jni_registrar.cc',
        'variations/android/component_jni_registrar.h',
        'variations/android/variations_associated_data_android.cc',
        'variations/android/variations_associated_data_android.h',
        'variations/caching_permuted_entropy_provider.cc',
        'variations/caching_permuted_entropy_provider.h',
        'variations/entropy_provider.cc',
        'variations/entropy_provider.h',
        'variations/metrics_util.cc',
        'variations/metrics_util.h',
        'variations/pref_names.cc',
        'variations/pref_names.h',
        'variations/processed_study.cc',
        'variations/processed_study.h',
        'variations/proto/client_variations.proto',
        'variations/proto/permuted_entropy_cache.proto',
        'variations/proto/study.proto',
        'variations/proto/variations_seed.proto',
        'variations/study_filtering.cc',
        'variations/study_filtering.h',
        'variations/variations_associated_data.cc',
        'variations/variations_associated_data.h',
        'variations/variations_seed_processor.cc',
        'variations/variations_seed_processor.h',
        'variations/variations_seed_simulator.cc',
        'variations/variations_seed_simulator.h',
      ],
      'variables': {
        'proto_in_dir': 'variations/proto',
        'proto_out_dir': 'components/variations/proto',
      },
      'includes': [ '../build/protoc.gypi' ],
      'conditions': [
        ['OS == "android"', {
          'dependencies': [
            'variations_jni_headers',
          ],
        }],
      ],
    },
    {
      # GN version: //components/variations_http_provider
      'target_name': 'variations_http_provider',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        'components.gyp:google_core_browser',
        'variations',
      ],
      'sources': [
        'variations/variations_http_header_provider.cc',
        'variations/variations_http_header_provider.h',
      ],
    },
  ],
  'conditions': [
    ['OS=="android"', {
      'targets': [
        {
          'target_name': 'variations_java',
          'type': 'none',
          'dependencies': [
            '../base/base.gyp:base',
          ],
          'variables': {
            'java_in_dir': 'variations/android/java',
          },
          'includes': [ '../build/java.gypi' ],
        },
        {
          'target_name': 'variations_jni_headers',
          'type': 'none',
          'sources': [
            'variations/android/java/src/org/chromium/components/variations/VariationsAssociatedData.java',
          ],
          'variables': {
            'jni_gen_package': 'variations',
          },
          'includes': [ '../build/jni_generator.gypi' ],
        },
      ],
    }],
  ]
}
