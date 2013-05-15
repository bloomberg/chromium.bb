# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'dependencies': [
    '../base/base.gyp:base',
    '../build/temp_gyp/googleurl.gyp:googleurl',
    '../components/tracing.gyp:tracing',
    '../ui/ui.gyp:ui',
  ],
  'include_dirs': [
    '..',
  ],
  'export_dependent_settings': [
    '../base/base.gyp:base',
  ],
  'sources': [
    'common/np_channel_base.cc',
    'common/np_channel_base.h',
    'common/npobject_base.h',
    'common/npobject_proxy.cc',
    'common/npobject_proxy.h',
    'common/npobject_stub.cc',
    'common/npobject_stub.h',
    'common/npobject_util.cc',
    'common/npobject_util.h',
    'common/plugin_messages.h',
    'common/plugin_param_traits.cc',
    'common/plugin_param_traits.h',
    'common/plugin_message_generator.cc',
    'common/plugin_message_generator.h',
  ],
  'conditions': [
    ['OS=="ios"', {
      'sources/': [
        # iOS only needs a small portion of content; exclude all the
        # implementation, and re-include what is used.
        ['exclude', '\\.(cc|mm)$'],
      ],
    }, {  # OS!="ios"
      'dependencies': [
        '../third_party/WebKit/Source/WebKit/chromium/WebKit.gyp:webkit',
        '../third_party/npapi/npapi.gyp:npapi',
        '../webkit/support/webkit_support.gyp:glue',
        '../webkit/support/webkit_support.gyp:webkit_base',
      ],
    }],
  ],
}
