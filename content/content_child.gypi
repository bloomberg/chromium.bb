# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'dependencies': [
    '../base/base.gyp:base',
    '../components/tracing.gyp:tracing',
    '../ui/ui.gyp:ui',
    '../url/url.gyp:url_lib',
  ],
  'include_dirs': [
    '..',
  ],
  'export_dependent_settings': [
    '../base/base.gyp:base',
  ],
  'sources': [
    'child/fileapi/webfilesystem_callback_adapters.cc',
    'child/fileapi/webfilesystem_callback_adapters.h',
    'child/fileapi/webfilesystem_impl.cc',
    'child/fileapi/webfilesystem_impl.h',
    'child/fileapi/webfilewriter_impl.cc',
    'child/fileapi/webfilewriter_impl.h',
    'child/indexed_db/indexed_db_dispatcher.cc',
    'child/indexed_db/indexed_db_dispatcher.h',
    'child/indexed_db/indexed_db_message_filter.cc',
    'child/indexed_db/indexed_db_message_filter.h',
    'child/indexed_db/proxy_webidbcursor_impl.cc',
    'child/indexed_db/proxy_webidbcursor_impl.h',
    'child/indexed_db/proxy_webidbdatabase_impl.cc',
    'child/indexed_db/proxy_webidbdatabase_impl.h',
    'child/indexed_db/proxy_webidbfactory_impl.cc',
    'child/indexed_db/proxy_webidbfactory_impl.h',
    'child/np_channel_base.cc',
    'child/np_channel_base.h',
    'child/npobject_base.h',
    'child/npobject_proxy.cc',
    'child/npobject_proxy.h',
    'child/npobject_stub.cc',
    'child/npobject_stub.h',
    'child/npobject_util.cc',
    'child/npobject_util.h',
    'child/plugin_message_generator.cc',
    'child/plugin_message_generator.h',
    'child/plugin_messages.h',
    'child/plugin_param_traits.cc',
    'child/plugin_param_traits.h',
    'child/web_database_observer_impl.cc',
    'child/web_database_observer_impl.h',
    'child/webblobregistry_impl.cc',
    'child/webblobregistry_impl.h',
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
