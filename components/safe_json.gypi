# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/safe_json
      'target_name': 'safe_json',
      #'type': '<(component)',
      'type': 'static_library',
      'dependencies': [
        'safe_json_parser_message_filter',
        '../base/base.gyp:base',
        '../components/components_strings.gyp:components_strings',
        '../content/content.gyp:content_browser',
        '../ui/base/ui_base.gyp:ui_base',
      ],
      'include_dirs': [
        '..',
      ],
      'defines': [
        'SAFE_JSON_IMPLEMENTATION',
      ],
      'sources': [
        'safe_json/safe_json_parser.cc',
        'safe_json/safe_json_parser.h',
      ],
    },
    {
      'target_name': 'safe_json_parser_message_filter',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../content/content.gyp:content_browser',
        '../ipc/ipc.gyp:ipc',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'safe_json/safe_json_parser_message_filter.cc',
        'safe_json/safe_json_parser_message_filter.h',
        'safe_json/safe_json_parser_messages.cc',
        'safe_json/safe_json_parser_messages.h',
      ],
    },
  ],
}
