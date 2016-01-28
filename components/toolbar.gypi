# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/toolbar
      'target_name': 'toolbar',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../url/url.gyp:url_lib',
        'security_state',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'toolbar/toolbar_model.cc',
        'toolbar/toolbar_model.h',
      ],
    },
    {
      # GN version: //components/toolbar:test_support
      'target_name': 'toolbar_test_support',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../ui/gfx/gfx.gyp:gfx_vector_icons',
        'components_resources.gyp:components_resources',
        'toolbar',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'toolbar/test_toolbar_model.cc',
        'toolbar/test_toolbar_model.h',
      ],
      'conditions': [
        ['toolkit_views==1', {
          # Needed to get the TOOLKIT_VIEWS define.
          'dependencies': [
            '<(DEPTH)/ui/views/views.gyp:views',
          ],
        }],
      ],
    },
  ],
}
