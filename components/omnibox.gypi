# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/omnibox
      'target_name': 'omnibox',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../url/url.gyp:url_lib',
        'search',
        'search_engines',
        'variations',
        'component_metrics_proto',
        'components_resources.gyp:components_resources',
        'url_fixer',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'omnibox/autocomplete_input.cc',
        'omnibox/autocomplete_input.h',
        'omnibox/autocomplete_match.cc',
        'omnibox/autocomplete_match.h',
        'omnibox/autocomplete_match_type.cc',
        'omnibox/autocomplete_match_type.h',
        'omnibox/autocomplete_provider.cc',
        'omnibox/autocomplete_provider.h',
        'omnibox/autocomplete_provider_listener.h',
        'omnibox/autocomplete_scheme_classifier.h',
        'omnibox/omnibox_field_trial.cc',
        'omnibox/omnibox_field_trial.h',
        'omnibox/omnibox_switches.cc',
        'omnibox/omnibox_switches.h',
        'omnibox/search_suggestion_parser.cc',
        'omnibox/search_suggestion_parser.h',
        'omnibox/url_prefix.cc',
        'omnibox/url_prefix.h',
      ],
    },
    {
      # GN version: //components/omnibox:test_support
      'target_name': 'omnibox_test_support',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        'omnibox',
        'component_metrics_proto',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'omnibox/test_scheme_classifier.cc',
        'omnibox/test_scheme_classifier.h',
      ],
    },
  ],
}
