# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'debugger',
      'type': '<(library)',
      'msvs_guid': '57823D8C-A317-4713-9125-2C91FDFD12D6',
      'dependencies': [
        'chrome_resources',
        'chrome_strings',
        'theme_resources',
        '../skia/skia.gyp:skia',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'browser/debugger/debugger_remote_service.cc',
        'browser/debugger/debugger_remote_service.h',
        'browser/debugger/debugger_wrapper.cc',
        'browser/debugger/debugger_wrapper.h',
        'browser/debugger/devtools_client_host.h',
        'browser/debugger/devtools_manager.cc',
        'browser/debugger/devtools_manager.h',
        'browser/debugger/devtools_protocol_handler.cc',
        'browser/debugger/devtools_protocol_handler.h',
        'browser/debugger/devtools_remote.h',
        'browser/debugger/devtools_remote_listen_socket.cc',
        'browser/debugger/devtools_remote_listen_socket.h',
        'browser/debugger/devtools_remote_message.cc',
        'browser/debugger/devtools_remote_message.h',
        'browser/debugger/devtools_remote_service.cc',
        'browser/debugger/devtools_remote_service.h',
        'browser/debugger/devtools_window.cc',
        'browser/debugger/devtools_window.h',
        'browser/debugger/extension_ports_remote_service.cc',
        'browser/debugger/extension_ports_remote_service.h',
        'browser/debugger/inspectable_tab_proxy.cc',
        'browser/debugger/inspectable_tab_proxy.h',
      ],
      'conditions': [
        ['OS=="linux" or OS=="freebsd"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
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
