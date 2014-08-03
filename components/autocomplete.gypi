# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/autocomplete
      'target_name': 'autocomplete',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../url/url.gyp:url_lib',
        'search_engines',
        'component_metrics_proto',
        'components_resources.gyp:components_resources',
        'url_fixer',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'autocomplete/autocomplete_input.cc',
        'autocomplete/autocomplete_input.h',
        'autocomplete/autocomplete_match.cc',
        'autocomplete/autocomplete_match.h',
        'autocomplete/autocomplete_match_type.cc',
        'autocomplete/autocomplete_match_type.h',
        'autocomplete/autocomplete_provider.cc',
        'autocomplete/autocomplete_provider.h',
        'autocomplete/autocomplete_provider_listener.h',
        'autocomplete/autocomplete_scheme_classifier.h',
        'autocomplete/search_suggestion_parser.cc',
        'autocomplete/search_suggestion_parser.h',
        'autocomplete/url_prefix.cc',
        'autocomplete/url_prefix.h',
      ],
    },
    {
      # GN version: //components/autocomplete:test_support
      'target_name': 'autocomplete_test_support',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        'autocomplete',
        'component_metrics_proto',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'autocomplete/test_scheme_classifier.cc',
        'autocomplete/test_scheme_classifier.h',
      ],
    },
  ],
}
