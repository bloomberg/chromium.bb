# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'sources': [
    '<@(schema_files)',
  ],
  'variables': {
    'main_schema_files': [
      'accessibility_private.json',
      'activity_log_private.json',
      'alarms.idl',
      'audio.idl',
      'automation.idl',
      'automation_internal.idl',
      'autotest_private.idl',
      'bookmark_manager_private.json',
      'bookmarks.json',
      'braille_display_private.idl',
      'browser.idl',
      'chrome_web_view_internal.json',
      'cloud_print_private.json',
      'command_line_private.json',
      'content_settings.json',
      'context_menus_internal.json',
      'context_menus.json',
      'cookies.json',
      'copresence.idl',
      'copresence_private.idl',
      'debugger.json',
      'developer_private.idl',
      'dial.idl',
      'downloads.idl',
      'downloads_internal.idl',
      'easy_unlock_private.idl',
      'echo_private.json',
      'enterprise_platform_keys_private.json',
      'experience_sampling_private.json',
      'feedback_private.idl',
      'file_manager_private.idl',
      'file_manager_private_internal.idl',
      'file_system.idl',
      'file_system_provider.idl',
      'file_system_provider_internal.idl',
      'font_settings.json',
      'gcd_private.idl',
      'gcm.json',
      'hangouts_private.idl',
      'history.json',
      'hotword_private.idl',
      'i18n.json',
      'identity.idl',
      'identity_private.idl',
      'idle.json',
      'image_writer_private.idl',
      'input_ime.json',
      'location.idl',
      'management.json',
      'manifest_types.json',
      'mdns.idl',
      'media_galleries.idl',
      'media_galleries_private.idl',
      'metrics_private.json',
      'networking_private.json',
      'notification_provider.idl',
      'notifications.idl',
      'omnibox.json',
      'page_capture.json',
      'permissions.json',
      'preferences_private.json',
      'push_messaging.idl',
      'reading_list_private.json',
      'screenlock_private.idl',
      'signed_in_devices.idl',
      'streams_private.idl',
      'synced_notifications_private.idl',
      'sync_file_system.idl',
      'system_indicator.idl',
      'system_private.json',
      'terminal_private.json',
      'types.json',
      'virtual_keyboard_private.json',
      'web_navigation.json',
      # Despite the name, this API does not rely on any
      # WebRTC-specific bits and as such does not belong in
      # the enable_webrtc==0 section below.
      'webrtc_audio_private.idl',
      'webrtc_logging_private.idl',
      'webstore_private.json',
    ],
    'main_schema_include_rules': [
      'extensions/common/api:extensions::core_api::%(namespace)s',
    ],
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
    ],
    # APIs that are causing crashes on athena.
    # TODO(oshima): Fix crashes and add them back. crbug.com/414340.
    'non_athena_schema_files': [
      'desktop_capture.json',
      'sessions.json',
      'tab_capture.idl',
      'tabs.json',
      'windows.json',
    ],
    # ChromeOS-specific schemas.
    'chromeos_schema_files': [
      'accessibility_features.json',
      'diagnostics.idl',
      'enterprise_platform_keys.idl',
      'enterprise_platform_keys_internal.idl',
      'file_browser_handler_internal.json',
      'first_run_private.json',
      'log_private.idl',
      'wallpaper.json',
      'wallpaper_private.json',
      'webcam_private.idl',
    ],

    'webrtc_schema_files': [
      'cast_streaming_rtp_stream.idl',
      'cast_streaming_session.idl',
      'cast_streaming_udp_transport.idl',
    ],

    'chromium_code': 1,
    # Disable schema compiler to generate model extension API code.
    # Only register the extension functions in extension system.
    'conditions': [
      # TODO(thestig): Remove this file from non-extensions build so the
      # conditional and else branch goes away.
      # Do the same for chrome/common/extensions/api/schemas.gni.
      ['enable_extensions==1', {
        'non_compiled_schema_files': [
          '<@(main_non_compiled_schema_files)',
        ],
        'schema_dependencies': [
          '<(DEPTH)/extensions/common/api/api.gyp:extensions_api',
        ],
        'schema_files': [
          '<@(main_schema_files)',
        ],
        'schema_include_rules': [
          '<@(main_schema_include_rules)',
        ],
      }, {  # enable_extensions==0
        'non_compiled_schema_files': [
        ],
        'schema_dependencies': [
        ],
        'schema_files': [
        ],
      }],
      ['chromeos==1', {
        'schema_files': [
          '<@(chromeos_schema_files)',
        ],
      }],
      ['use_athena==0', {
        'schema_files': [
          '<@(non_athena_schema_files)',
        ],
      }],
      ['enable_extensions==1 and enable_webrtc==1', {
        'schema_files': [
          '<@(webrtc_schema_files)',
        ],
      }],
    ],
    'cc_dir': 'chrome/common/extensions/api',
    'root_namespace': 'extensions::api::%(namespace)s',
    'impl_dir_': 'chrome/browser/extensions/api',
  },
}
