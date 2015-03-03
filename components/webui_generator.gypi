# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [{
    # GN version: //components/webui_generator
    'target_name': 'webui_generator',
    'type': '<(component)',
    'dependencies': [
      'login',
      '<(DEPTH)/base/base.gyp:base',
      '<(DEPTH)/components/components_resources.gyp:components_resources',
      '<(DEPTH)/content/content.gyp:content_browser',
      '<(DEPTH)/ui/base/ui_base.gyp:ui_base',
    ],
    'export_dependent_settings': [
      'login',
      '<(DEPTH)/base/base.gyp:base',
      '<(DEPTH)/content/content.gyp:content_browser',
    ],
    'defines': [
      'WUG_IMPLEMENTATION',
    ],
    'sources': [
      'webui_generator/data_source_util.cc',
      'webui_generator/data_source_util.h',
      'webui_generator/export.h',
      'webui_generator/view.cc',
      'webui_generator/view.h',
      'webui_generator/view_model.cc',
      'webui_generator/view_model.h',
      'webui_generator/web_ui_view.cc',
      'webui_generator/web_ui_view.h',
    ],
  }],
}
