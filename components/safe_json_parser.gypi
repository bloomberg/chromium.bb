# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/safe_json_parser
      'target_name': 'safe_json_parser',
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
        'SAFE_JSON_PARSER_IMPLEMENTATION',
      ],
      'sources': [
        'safe_json_parser/safe_json_parser.h',
        'safe_json_parser/safe_json_parser.cc',
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
        'safe_json_parser/safe_json_parser_messages.cc',
        'safe_json_parser/safe_json_parser_messages.h',
        'safe_json_parser/safe_json_parser_message_filter.cc',
        'safe_json_parser/safe_json_parser_message_filter.h',
      ],
    },
  ],
}
