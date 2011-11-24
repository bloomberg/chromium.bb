# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../../build/win_precompile.gypi',
  ],
  'targets': [
    {
      'target_name': 'default_plugin',
      'type': 'static_library',
      'dependencies': [
        ':default_plugin_resources',
        '<(DEPTH)/base/base.gyp:base',
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
        'plugin_impl_aura.cc',
        'plugin_impl_aura.h',
        'plugin_impl_gtk.cc',
        'plugin_impl_gtk.h',
        'plugin_impl_mac.h',
        'plugin_impl_mac.mm',
        'plugin_impl_win.cc',
        'plugin_impl_win.h',
        'plugin_installer_base.cc',
        'plugin_installer_base.h',
        'plugin_main.cc',
        'plugin_main.h',
      ],
      'conditions': [
        ['OS=="win"', {
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
        ['toolkit_uses_gtk == 1', {
          'dependencies': [
            '<(DEPTH)/build/linux/system.gyp:gtk',
          ],
        }],
      ],
    },
    # This can't be part of chrome.gyp:chrome_resources because then there'd
    # be a cyclic dependency.
    {
      'target_name': 'default_plugin_resources',
      'type': 'none',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/chrome/default_plugin_resources',
      },
      'actions': [
        {
          'action_name': 'default_plugin_resources',
          'variables': {
            'grit_grd_file': 'default_plugin_resources.grd',
          },
          'includes': [ '../../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../../build/grit_target.gypi' ],
    },
  ],
}
