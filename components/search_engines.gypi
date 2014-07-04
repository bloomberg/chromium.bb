# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'search_engines',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../google_apis/google_apis.gyp:google_apis',
        '../net/net.gyp:net',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        '../url/url.gyp:url_lib',
        'component_metrics_proto',
        'components_strings.gyp:components_strings',
        'google_core_browser',
        'policy',
        'pref_registry',
        'search_engines/prepopulated_engines.gyp:prepopulated_engines',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'search_engines/default_search_manager.cc',
        'search_engines/default_search_manager.h',
        'search_engines/default_search_policy_handler.cc',
        'search_engines/default_search_policy_handler.h',
        'search_engines/search_engine_type.h',
        'search_engines/search_engines_pref_names.cc',
        'search_engines/search_engines_pref_names.h',
        'search_engines/search_engines_switches.cc',
        'search_engines/search_engines_switches.h',
        'search_engines/search_terms_data.cc',
        'search_engines/search_terms_data.h',
        'search_engines/template_url.cc',
        'search_engines/template_url.h',
        'search_engines/template_url_data.cc',
        'search_engines/template_url_data.h',
        'search_engines/template_url_id.h',
        'search_engines/template_url_prepopulate_data.cc',
        'search_engines/template_url_prepopulate_data.h',
      ],
    },
  ],
}
