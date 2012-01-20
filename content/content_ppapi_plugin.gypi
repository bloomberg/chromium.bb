# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'dependencies': [
    '../base/base.gyp:base',
    '../ppapi/ppapi_internal.gyp:ppapi_proxy',
  ],
  'sources': [
    'ppapi_plugin/broker_process_dispatcher.cc',
    'ppapi_plugin/broker_process_dispatcher.h',
    'ppapi_plugin/plugin_process_dispatcher.cc',
    'ppapi_plugin/plugin_process_dispatcher.h',
    'ppapi_plugin/ppapi_broker_main.cc',
    'ppapi_plugin/ppapi_plugin_main.cc',
    'ppapi_plugin/ppapi_thread.cc',
    'ppapi_plugin/ppapi_thread.h',
    'ppapi_plugin/ppapi_webkit_thread.cc',
    'ppapi_plugin/ppapi_webkit_thread.h',
    'ppapi_plugin/ppapi_webkitplatformsupport_impl.cc',
    'ppapi_plugin/ppapi_webkitplatformsupport_impl.h',
  ],
  'include_dirs': [
    '..',
  ],
}
