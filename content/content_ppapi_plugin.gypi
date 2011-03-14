# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'content_ppapi_plugin',
      'type': '<(library)',
      'dependencies': [
        '../base/base.gyp:base',
        '../ppapi/ppapi.gyp:ppapi_proxy',
      ],
      'sources': [
        'ppapi_plugin/plugin_process_dispatcher.cc',
        'ppapi_plugin/plugin_process_dispatcher.h',
        'ppapi_plugin/ppapi_plugin_main.cc',
        'ppapi_plugin/ppapi_process.cc',
        'ppapi_plugin/ppapi_process.h',
        'ppapi_plugin/ppapi_thread.cc',
        'ppapi_plugin/ppapi_thread.h',
      ],
      'include_dirs': [
        '..',
      ],
    },
  ],
}
