# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'athena_main',
      'type': 'executable',
      'dependencies': [
        '../athena.gyp:athena_lib',
        '../athena.gyp:athena_content_lib',
        '../resources/athena_resources.gyp:athena_pak',
        '../resources/athena_resources.gyp:athena_resources',
	# debug_widow.cc depends on this. Remove this once debug_window
	# is removed.
        '../../ash/ash_resources.gyp:ash_resources',
        '../../chromeos/chromeos.gyp:power_manager_proto',
        '../../components/components.gyp:autocomplete',
        '../../components/components.gyp:component_metrics_proto',
        '../../components/components.gyp:history_core_browser',
        # infobars_test_support is required to declare some symbols used in the
        # search_engines and its dependencies. See crbug.com/386171
        # TODO(mukai): declare those symbols for Athena.
        '../../components/components.gyp:infobars_test_support',
        '../../components/components.gyp:search_engines',
        '../../extensions/shell/app_shell.gyp:app_shell_lib',
        '../../skia/skia.gyp:skia',
        '../../ui/accessibility/accessibility.gyp:ax_gen',
        '../../ui/app_list/app_list.gyp:app_list',
        '../../ui/chromeos/ui_chromeos.gyp:ui_chromeos',
        '../../ui/keyboard/keyboard.gyp:keyboard',
        '../../ui/native_theme/native_theme.gyp:native_theme',
        '../../ui/views/views.gyp:views',
        '../../url/url.gyp:url_lib',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'athena_app_window_controller.cc',
        'athena_app_window_controller.h',
        'athena_launcher.cc',
        'athena_launcher.h',
        'debug/debug_window.cc',
        'debug/debug_window.h',
        'debug/network_selector.cc',
        'debug/network_selector.h',
        'url_search_provider.cc',
        'url_search_provider.h',
        'athena_main.cc',
        'placeholder.cc',
        'placeholder.h',
      ],
    },
    {
      'target_name': 'athena_shell',
      'type': 'executable',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:base_i18n',
        '../../skia/skia.gyp:skia',
        '../../ui/accessibility/accessibility.gyp:ax_gen',
        '../../ui/aura/aura.gyp:aura',
        '../../ui/compositor/compositor.gyp:compositor_test_support',
        '../../ui/gfx/gfx.gyp:gfx',
        '../athena.gyp:athena_lib',
        '../athena.gyp:athena_test_support',
        '../resources/athena_resources.gyp:athena_pak',
      ],
      'sources': [
        'athena_shell.cc',
      ],
    }
  ],  # targets
}
