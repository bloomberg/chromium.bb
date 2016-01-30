# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# We have 2 separate browser targets because //components/html_viewer requires
# startup_metric_utils_browser, but has symbols that conflict with mojo symbols
# that startup_metric_utils_browser_message_filter indirectly depends on.

{
  'targets': [
    {
      # GN version: //components/startup_metric_utils/browser:lib
      'target_name': 'startup_metric_utils_browser',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:base_prefs',
        'components.gyp:version_info',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'startup_metric_utils/browser/pref_names.cc',
        'startup_metric_utils/browser/pref_names.h',
        'startup_metric_utils/browser/startup_metric_utils.cc',
        'startup_metric_utils/browser/startup_metric_utils.h',
      ],
    },
    {
      # GN version: //components/startup_metric_utils/browser:message_filter_lib
      'target_name': 'startup_metric_utils_browser_message_filter',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../content/content.gyp:content_browser',
        'startup_metric_utils_browser',
        'startup_metric_utils_common',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'startup_metric_utils/browser/startup_metric_message_filter.cc',
        'startup_metric_utils/browser/startup_metric_message_filter.h',
      ],
    },
    {
      # GN version: //components/startup_metric_utils/common
      'target_name': 'startup_metric_utils_common',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../ipc/ipc.gyp:ipc',
        'components.gyp:variations',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'startup_metric_utils/common/pre_read_field_trial_utils_win.cc',
        'startup_metric_utils/common/pre_read_field_trial_utils_win.h',
        'startup_metric_utils/common/startup_metric_message_generator.cc',
        'startup_metric_utils/common/startup_metric_message_generator.h',
        'startup_metric_utils/common/startup_metric_messages.h',
      ],
    },
  ],
}
