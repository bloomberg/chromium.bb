# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'api',
      'type': 'static_library',
      'sources': [
        '<@(idl_schema_files)',
        '<@(json_schema_files)',
      ],
      'includes': [
        '../../../../build/json_schema_bundle_compile.gypi',
        '../../../../build/json_schema_compile.gypi',
      ],
      'variables': {
        'chromium_code': 1,
        'json_schema_files': [
          'bookmarks.json',
          'cloud_print_private.json',
          'content_settings.json',
          'context_menus.json',
          'cookies.json',
          'debugger.json',
          'events.json',
          'experimental_history.json',
          'experimental_record.json',
          'file_browser_handler_internal.json',
          'i18n.json',
          'font_settings.json',
          'history.json',
          'management.json',
          'page_capture.json',
          'permissions.json',
          'storage.json',
          'tabs.json',
          'web_navigation.json',
          'web_request.json',
          'windows.json',
        ],
        'idl_schema_files': [
          'alarms.idl',
          'app_current_window_internal.idl',
          'app_runtime.idl',
          'app_window.idl',
          'autotest_private.idl',
          'bluetooth.idl',
          'developer_private.idl',
          'dial.idl',
          'downloads.idl',
          'experimental_discovery.idl',
          'experimental_dns.idl',
          'experimental_identity.idl',
          'experimental_idltest.idl',
          'experimental_media_galleries.idl',
          'experimental_notification.idl',
          'experimental_system_info_cpu.idl',
          'experimental_system_info_display.idl',
          'experimental_system_info_memory.idl',
          'experimental_system_info_storage.idl',
          'file_system.idl',
          'media_galleries.idl',
          'media_galleries_private.idl',
          'push_messaging.idl',
          'rtc_private.idl',
          'serial.idl',
          'socket.idl',
          'sync_file_system.idl',
          'system_indicator.idl',
          'tab_capture.idl',
          'usb.idl',
        ],
        'cc_dir': 'chrome/common/extensions/api',
        'root_namespace': 'extensions::api',
      },
      'conditions': [
        ['OS=="android"', {
          'idl_schema_files!': [
            'usb.idl',
          ],
        }],
        ['OS!="chromeos"', {
          'json_schema_files!': [
            'file_browser_handler_internal.json',
          ],
          'idl_schema_files!': [
            'rtc_private.idl',
          ],
        }],
      ],
    },
  ],
}
