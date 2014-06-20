# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  # All files are stored in these lists which are referenced in the target
  # below so that the GN build of this target can read in this dictionary and
  # duplicate the same logic without the lists getting out-of-sync. The GN
  # .gypi reader can not process conditions and does not know about targets,
  # etc., it just reads Python dictionaries.
  #
  # In addition, the GN build treats .idl files and .json files separately,
  # since they run through different scripts. If you add a new category, also
  # add it to the BUILD.gn file in this directory.
  'variables': {
    # These duplicate other lists and are the only ones used on Android. They
    # should be eliminated. See crbug.com/305852.
    'android_schema_files_idl': [
      'file_system.idl',
      'sync_file_system.idl',
      'tab_capture.idl',
    ],
    'android_schema_files_json': [
      'activity_log_private.json',
      'events.json',
      'manifest_types.json',
      'permissions.json',
      'tabs.json',
      'types.json',
      'webview.json',
      'web_navigation.json',
      'windows.json',
    ],

    # These are used everywhere except Android.
    'main_schema_files_idl': [
      'alarms.idl',
      'app_current_window_internal.idl',
      'app_window.idl',
      'audio.idl',
      'automation_internal.idl',
      'automation.idl',
      'autotest_private.idl',
      'bluetooth.idl',
      'bluetooth_low_energy.idl',
      'bluetooth_socket.idl',
      'browser.idl',
      'braille_display_private.idl',
      'cast_channel.idl',
      'developer_private.idl',
      'dial.idl',
      'downloads.idl',
      'downloads_internal.idl',
      'feedback_private.idl',
      'file_browser_private.idl',
      'file_browser_private_internal.idl',
      'file_system.idl',
      'file_system_provider.idl',
      'file_system_provider_internal.idl',
      'gcd_private.idl',
      'hangouts_private.idl',
      'hid.idl',
      'hotword_private.idl',
      'identity.idl',
      'identity_private.idl',
      'image_writer_private.idl',
      'location.idl',
      'mdns.idl',
      'media_galleries.idl',
      'media_galleries_private.idl',
      'notifications.idl',
      'power.idl',
      'push_messaging.idl',
      'screenlock_private.idl',
      'serial.idl',
      'signed_in_devices.idl',
      'streams_private.idl',
      'sync_file_system.idl',
      'synced_notifications_private.idl',
      'system_cpu.idl',
      'system_display.idl',
      'system_indicator.idl',
      'system_memory.idl',
      'system_network.idl',
      'system_storage.idl',
      'tab_capture.idl',
      # Despite the name, this API does not rely on any
      # WebRTC-specific bits and as such does not belong in
      # the enable_webrtc=0 section below.
      'webrtc_audio_private.idl',
      'webrtc_logging_private.idl',
    ],
    'main_schema_files_json': [
      'accessibility_private.json',
      'activity_log_private.json',
      'bluetooth_private.json',
      'bookmark_manager_private.json',
      'bookmarks.json',
      'cloud_print_private.json',
      'command_line_private.json',
      'content_settings.json',
      'context_menus.json',
      'context_menus_internal.json',
      'cookies.json',
      'debugger.json',
      'desktop_capture.json',
      'echo_private.json',
      'enterprise_platform_keys_private.json',
      'events.json',
      'font_settings.json',
      'gcm.json',
      'guest_view_internal.json',
      'history.json',
      'i18n.json',
      'idle.json',
      'input_ime.json',
      'management.json',
      'manifest_types.json',
      'metrics_private.json',
      'networking_private.json',
      'omnibox.json',
      'page_capture.json',
      'permissions.json',
      'preferences_private.json',
      'reading_list_private.json',
      'sessions.json',
      'system_private.json',
      'tabs.json',
      'terminal_private.json',
      'types.json',
      'virtual_keyboard_private.json',
      'web_navigation.json',
      'web_request.json',
      'webstore_private.json',
      'webview.json',
      'windows.json',
    ],
    # The non-compiled shcema files don't need to be separated out by type
    # since they're only given to the bundle script which doesn't care about
    # type.
    'main_non_compiled_schema_files': [
      'browsing_data.json',
      'chromeos_info_private.json',
      'extension.json',
      'idltest.idl',
      'infobars.json',
      'media_player_private.json',
      'music_manager_private.idl',
      'principals_private.idl',
      'top_sites.json',
      'web_request_internal.json',
    ],

    # ChromeOS-specific schemas.
    'chromeos_schema_files_idl': [
      'diagnostics.idl',
      'enterprise_platform_keys.idl',
      'enterprise_platform_keys_internal.idl',
      'log_private.idl',
      'webcam_private.idl',
    ],
    'chromeos_schema_files_json': [
      'accessibility_features.json',
      'file_browser_handler_internal.json',
      'first_run_private.json',
      'wallpaper.json',
      'wallpaper_private.json',
    ],
    'chromeos_branded_schema_files_idl': [
      'ledger/ledger.idl',
    ],

    'webrtc_schema_files_idl': [
      'cast_streaming_rtp_stream.idl',
      'cast_streaming_session.idl',
      'cast_streaming_udp_transport.idl',
    ],
  },
  'targets': [
    {
      'target_name': 'chrome_api',
      'type': 'static_library',
      'sources': [
        '<@(schema_files)',
      ],
      # TODO(jschuh): http://crbug.com/167187 size_t -> int
      'msvs_disabled_warnings': [ 4267 ],
      'includes': [
        '../../../../build/json_schema_bundle_compile.gypi',
        '../../../../build/json_schema_compile.gypi',
      ],
      'variables': {
        'chromium_code': 1,
        # Disable schema compiler to generate model extension API code.
        # Only register the extension functions in extension system.
        'conditions': [
          ['OS!="android"', {
            'non_compiled_schema_files': [
              '<@(main_non_compiled_schema_files)',
            ],
            'schema_files': [
              '<@(main_schema_files_idl)',
              '<@(main_schema_files_json)',
            ],
          }, {  # OS=="android"
            'non_compiled_schema_files': [
            ],
            'schema_files': [
              # These should be eliminated. See crbug.com/305852.
              '<@(android_schema_files_idl)',
              '<@(android_schema_files_json)',
            ],
          }],
          ['chromeos==1', {
            'schema_files': [
              '<@(chromeos_schema_files_idl)',
              '<@(chromeos_schema_files_json)',
            ],
          }],
          ['enable_webrtc==1', {
            'schema_files': [
              '<@(webrtc_schema_files_idl)',
            ],
          }],
          ['branding=="Chrome" and chromeos==1', {
            'schema_files': [
              '<@(chromeos_branded_schema_files_idl)',
            ],
          }],
        ],
        'cc_dir': 'chrome/common/extensions/api',
        'root_namespace': 'extensions::api',
      },
      'dependencies': [
        '<(DEPTH)/content/content.gyp:content_browser',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/sync/sync.gyp:sync',
      ],
      'conditions': [
        ['chromeos==1', {
          'dependencies': [
            '<(DEPTH)/chrome/chrome.gyp:drive_proto',
          ],
        }],
      ],
    },
  ],
}
