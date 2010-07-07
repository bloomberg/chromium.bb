# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
   },
  'targets': [
    {
      'target_name': 'default_plugin',
      'type': '<(library)',
      'dependencies': [
        '<(DEPTH)/net/net.gyp:net_resources',
        '<(DEPTH)/third_party/icu/icu.gyp:icui18n',
        '<(DEPTH)/third_party/icu/icu.gyp:icuuc',
        '<(DEPTH)/third_party/libxml/libxml.gyp:libxml',
        '<(DEPTH)/third_party/npapi/npapi.gyp:npapi',
        '<(DEPTH)/webkit/support/webkit_support.gyp:webkit_resources',
        '<(DEPTH)/webkit/support/webkit_support.gyp:webkit_strings',
      ],
      'include_dirs': [
        '<(DEPTH)',
        '<(DEPTH)/third_party/wtl/include',
      ],
      'sources': [
        'plugin_impl_gtk.cc',
        'plugin_impl_gtk.h',
        'plugin_impl_mac.h',
        'plugin_impl_mac.mm',
        'plugin_impl_win.cc',
        'plugin_impl_win.h',
        'plugin_main.cc',
        'plugin_main.h',
      ],
      'conditions': [
         ['OS=="win"', {
            'msvs_guid': '5916D37D-8C97-424F-A904-74E52594C2D6',
            'link_settings': {
              'libraries': ['-lurlmon.lib'],
            },
            'sources': [
              'default_plugin_resources.h',
              'install_dialog.cc',
              'install_dialog.h',
              'plugin_database_handler.cc',
              'plugin_database_handler.h',
              'plugin_install_job_monitor.cc',
              'plugin_install_job_monitor.h',
            ],
         }],
         ['OS=="linux"', {
            'dependencies': [
              '<(DEPTH)/build/linux/system.gyp:gtk',
            ],
         }],
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
