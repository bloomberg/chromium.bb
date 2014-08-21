# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/suggestions
      'target_name': 'suggestions',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../ui/gfx/gfx.gyp:gfx',
        '../url/url.gyp:url_lib',
        'components.gyp:keyed_service_core',
        'components.gyp:pref_registry',
        'components.gyp:variations',
      ],
      'sources': [
        'suggestions/blacklist_store.cc',
        'suggestions/blacklist_store.h',
        'suggestions/image_manager.h',
        'suggestions/proto/suggestions.proto',
        'suggestions/suggestions_pref_names.cc',
        'suggestions/suggestions_pref_names.h',
        'suggestions/suggestions_service.cc',
        'suggestions/suggestions_service.h',
        'suggestions/suggestions_store.cc',
        'suggestions/suggestions_store.h',
        'suggestions/suggestions_utils.cc',
        'suggestions/suggestions_utils.h',
      ],
      'variables': {
        'proto_in_dir': 'suggestions/proto',
        'proto_out_dir': 'components/suggestions/proto',
      },
      'includes': [ '../build/protoc.gypi' ],
    },
  ],
}
