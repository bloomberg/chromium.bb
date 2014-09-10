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
        '../resources/athena_resources.gyp:athena_resources',
        '../../components/components.gyp:component_metrics_proto',
        '../../components/components.gyp:history_core_browser',
        # infobars_test_support is required to declare some symbols used in the
        # search_engines and its dependencies. See crbug.com/386171
        # TODO(mukai): declare those symbols for Athena.
        '../../components/components.gyp:infobars_test_support',
        '../../components/components.gyp:omnibox',
        '../../components/components.gyp:pdf_renderer',
        '../../components/components.gyp:search_engines',
        '../../pdf/pdf.gyp:pdf',
        '../../skia/skia.gyp:skia',
        '../../ui/app_list/app_list.gyp:app_list',
        '../../ui/native_theme/native_theme.gyp:native_theme',
        '../../ui/views/views.gyp:views',
        '../../url/url.gyp:url_lib',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'athena_content_client.cc',
        'athena_content_client.h',
        'athena_frame_view.cc',
        'athena_frame_view.h',
        'athena_launcher.cc',
        'athena_renderer_pdf_helper.cc',
        'athena_renderer_pdf_helper.h',
        'athena_views_delegate.cc',
        'athena_views_delegate.h',
        'placeholder.cc',
        'placeholder.h',
        'public/athena_launcher.h',
        'url_search_provider.cc',
        'url_search_provider.h',
      ],
    },
    {
      'target_name': 'athena_main',
      'type': 'executable',
      'dependencies': [
        '../../ui/accessibility/accessibility.gyp:ax_gen',
        '../athena.gyp:athena_app_shell_lib',
        '../resources/athena_resources.gyp:athena_pak',
        'athena_main_lib',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'athena_main.cc',
      ],
    }
  ],  # targets
}
