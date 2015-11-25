# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/ntp_snippets
      'target_name': 'ntp_snippets',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../url/url.gyp:url_lib',
        'keyed_service_core',
      ],
      'sources': [
        'ntp_snippets/inner_iterator.h',
        'ntp_snippets/ntp_snippet.cc',
        'ntp_snippets/ntp_snippet.h',
        'ntp_snippets/ntp_snippets_service.cc',
        'ntp_snippets/ntp_snippets_service.h',
      ],
    },
  ],
}
