# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'athena_main_lib',
      'type': 'static_library',
      'dependencies': [
        '../athena.gyp:athena_lib',
        '../athena.gyp:athena_content_lib',
        '../athena.gyp:athena_content_support_lib',
        '../resources/athena_resources.gyp:athena_resources',
	# debug_widow.cc depends on this. Remove this once debug_window
	# is removed.
        '../../ash/ash_resources.gyp:ash_resources',
        '../../chromeos/chromeos.gyp:power_manager_proto',
        '../../components/components.gyp:component_metrics_proto',
        '../../components/components.gyp:history_core_browser',
        # infobars_test_support is required to declare some symbols used in the
        # search_engines and its dependencies. See crbug.com/386171
        # TODO(mukai): declare those symbols for Athena.
        '../../components/components.gyp:infobars_test_support',
        '../../components/components.gyp:omnibox',
        '../../components/components.gyp:search_engines',
        '../../skia/skia.gyp:skia',
        '../../ui/app_list/app_list.gyp:app_list',
        '../../ui/chromeos/ui_chromeos.gyp:ui_chromeos',
        '../../ui/native_theme/native_theme.gyp:native_theme',
        '../../ui/views/views.gyp:views',
        '../../url/url.gyp:url_lib',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'athena_launcher.cc',
        'athena_launcher.h',
        'debug/debug_window.cc',
        'debug/debug_window.h',
        'debug/network_selector.cc',
        'debug/network_selector.h',
        'url_search_provider.cc',
        'url_search_provider.h',
        'placeholder.cc',
        'placeholder.h',
      ],
    },
    {
      'target_name': 'athena_main',
      'type': 'executable',
      'dependencies': [
        '../../ui/accessibility/accessibility.gyp:ax_gen',
        '../resources/athena_resources.gyp:athena_pak',
	'../../extensions/shell/app_shell.gyp:app_shell_lib',
        'athena_main_lib',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'athena_app_window_controller.cc',
        'athena_app_window_controller.h',
        'athena_main.cc',
      ],
    }
  ],  # targets
}
