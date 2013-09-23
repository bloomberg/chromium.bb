# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'dom_distiller_webui',
      'type': 'static_library',
      'dependencies': [
        'component_strings.gyp:component_strings',
        'dom_distiller_core',
        'dom_distiller_resources',
        '../base/base.gyp:base',
        '../content/content.gyp:content_browser',
        '../skia/skia.gyp:skia',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'dom_distiller/webui/dom_distiller_ui.cc',
        'dom_distiller/webui/dom_distiller_ui.h',
        'dom_distiller/webui/dom_distiller_handler.cc',
        'dom_distiller/webui/dom_distiller_handler.h',
      ],
    },
    {
      'target_name': 'dom_distiller_resources',
      'type': 'none',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/components',
      },
      'actions': [
        {
          'action_name': 'dom_distiller_resources',
          'variables': {
            'grit_grd_file': 'dom_distiller_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../build/grit_target.gypi' ],
    },
    {
      'target_name': 'dom_distiller_core',
      'type': 'static_library',
      'dependencies': [
        'dom_distiller_core_proto',
        '../base/base.gyp:base',
        '../third_party/protobuf/protobuf.gyp:protobuf_lite',
        '../third_party/leveldatabase/leveldatabase.gyp:leveldatabase',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'dom_distiller/core/dom_distiller_constants.cc',
        'dom_distiller/core/dom_distiller_constants.h',
        'dom_distiller/core/dom_distiller_database.cc',
        'dom_distiller/core/dom_distiller_database.h',
      ],
    },
    {
      'target_name': 'dom_distiller_core_proto',
      'type': 'static_library',
      'sources': [
        'dom_distiller/core/proto/article_entry.proto',
      ],
      'variables': {
        'proto_in_dir': 'dom_distiller/core/proto',
        'proto_out_dir': 'components/dom_distiller/core/proto',
      },
      'includes': [ '../build/protoc.gypi', ],
    },
  ],
}
