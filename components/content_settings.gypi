# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/content_settings/core/browser
      'target_name': 'content_settings_core_browser',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        'content_settings_core_common',
      ],
      'variables': { 'enable_wexit_time_destructors': 1, },
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'content_settings/core/browser/content_settings_details.cc',
        'content_settings/core/browser/content_settings_details.h',
        'content_settings/core/browser/content_settings_observer.h',
        'content_settings/core/browser/content_settings_provider.h',
        'content_settings/core/browser/content_settings_observable_provider.cc',
        'content_settings/core/browser/content_settings_observable_provider.h',
        'content_settings/core/browser/content_settings_origin_identifier_value_map.cc',
        'content_settings/core/browser/content_settings_origin_identifier_value_map.h',
        'content_settings/core/browser/content_settings_rule.cc',
        'content_settings/core/browser/content_settings_rule.h',
      ],
    },
    {
      # GN version: //components/content_settings/core/common
      'target_name': 'content_settings_core_common',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../url/url.gyp:url_lib',
      ],
      'variables': { 'enable_wexit_time_destructors': 1, },
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'content_settings/core/common/content_settings.cc',
        'content_settings/core/common/content_settings.h',
        'content_settings/core/common/content_settings_pattern.cc',
        'content_settings/core/common/content_settings_pattern.h',
        'content_settings/core/common/content_settings_pattern_parser.cc',
        'content_settings/core/common/content_settings_pattern_parser.h',
        'content_settings/core/common/content_settings_types.h',
        'content_settings/core/common/permission_request_id.h',
        'content_settings/core/common/permission_request_id.cc',
      ],
    },
  ],
}
