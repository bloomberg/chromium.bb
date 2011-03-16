# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'content_plugin',
      'type': '<(library)',
      'msvs_guid': '20A560A0-2CD0-4D9E-A58B-1F24B99C087A',
      'dependencies': [
        'content_common',
        '../skia/skia.gyp:skia',
        '../third_party/npapi/npapi.gyp:npapi',
        '../webkit/support/webkit_support.gyp:glue',
      ],
      'include_dirs': [
        '<(INTERMEDIATE_DIR)',
      ],
      'sources': [
        # All .cc, .h, .m, and .mm files under plugins except for tests and
        # mocks.
        'plugin/npobject_base.h',
        'plugin/npobject_proxy.cc',
        'plugin/npobject_proxy.h',
        'plugin/npobject_stub.cc',
        'plugin/npobject_stub.h',
        'plugin/npobject_util.cc',
        'plugin/npobject_util.h',
        'plugin/plugin_channel.cc',
        'plugin/plugin_channel.h',
        'plugin/plugin_channel_base.cc',
        'plugin/plugin_channel_base.h',
        'plugin/plugin_interpose_util_mac.mm',
        'plugin/plugin_interpose_util_mac.h',
        'plugin/plugin_main.cc',
        'plugin/plugin_main_linux.cc',
        'plugin/plugin_main_mac.mm',
        'plugin/plugin_thread.cc',
        'plugin/plugin_thread.h',
        'plugin/webplugin_accelerated_surface_proxy_mac.cc',
        'plugin/webplugin_accelerated_surface_proxy_mac.h',
        'plugin/webplugin_delegate_stub.cc',
        'plugin/webplugin_delegate_stub.h',
        'plugin/webplugin_proxy.cc',
        'plugin/webplugin_proxy.h',
      ],
      # These are layered in conditionals in the event other platforms
      # end up using this module as well.
      'conditions': [
        ['OS=="win"', {
          'include_dirs': [
            '<(DEPTH)/third_party/wtl/include',
          ],
        }],
        ['OS=="linux" or OS=="freebsd" or OS=="openbsd" or OS=="solaris"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
          ],
        }],
      ],
    },
  ],
}
