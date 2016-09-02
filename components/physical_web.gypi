# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/physical_web/data_source
      'target_name': 'physical_web_data_source',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'physical_web/data_source/physical_web_data_source.h',
      ],
    },
    {
      # GN version: //components/physical_web/webui
      'target_name': 'physical_web_ui',
      'type': 'static_library',
      'sources': [
        # Note: sources list duplicated in GN build.
        'physical_web/webui/physical_web_ui_constants.cc',
        'physical_web/webui/physical_web_ui_constants.h',
      ],
    },
  ],
}
