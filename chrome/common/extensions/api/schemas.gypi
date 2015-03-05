# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'sources': [
    '<@(schema_files)',
  ],
  'variables': {
    'main_schema_files': [
      'accessibility_features.json',
      'accessibility_private.json',
      'activity_log_private.json',
      'audio_modem.idl',
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
      'cryptotoken_private.idl',
      'data_reduction_proxy.json',
      'debugger.json',
      'desktop_capture.json',
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
      'font_settings.json',
      'gcd_private.idl',
      'gcm.json',
      'hangouts_private.idl',
      'history.json',
      'hotword_private.idl',
      'i18n.json',
      'identity.idl',
      'identity_private.idl',
      'image_writer_private.idl',
      'inline_install_private.idl',
      'input_ime.json',
      'launcher_page.idl',
      'location.idl',
      'manifest_types.json',
      'mdns.idl',
      'media_galleries.idl',
      'metrics_private.json',
      'notification_provider.idl',
      'notifications.idl',
      'omnibox.json',
      'page_capture.json',
      'permissions.json',
      'preferences_private.json',
      'push_messaging.idl',
      'reading_list_private.json',
      'screenlock_private.idl',
      'sessions.json',
      'signed_in_devices.idl',
      'streams_private.idl',
      'sync_file_system.idl',
      'system_indicator.idl',
      'system_private.json',
      'tab_capture.idl',
      'tabs.json',
      'terminal_private.json',
      'types.json',
      'web_navigation.json',
      # Despite the name, this API does not rely on any
      # WebRTC-specific bits and as such does not belong in
      # the enable_webrtc==0 section below.
      'webrtc_audio_private.idl',
      'webrtc_logging_private.idl',
      'webstore_private.json',
      'windows.json',
    ],
    'main_schema_include_rules': [
      'extensions/common/api:extensions::core_api::%(namespace)s',
    ],
    'main_non_compiled_schema_files': [
      'browsing_data.json',
      'chromeos_info_private.json',
      'extension.json',
      'idltest.idl',
      'media_player_private.json',
      'music_manager_private.idl',
      'principals_private.idl',
      'top_sites.json',
    ],

    # ChromeOS-specific schemas.
    'chromeos_schema_files': [
      'enterprise_platform_keys.idl',
      'enterprise_platform_keys_internal.idl',
      'file_browser_handler_internal.json',
      'file_system_provider.idl',
      'file_system_provider_internal.idl',
      'first_run_private.json',
      'log_private.idl',
      'platform_keys.idl',
      'platform_keys_internal.idl',
      'wallpaper.json',
      'wallpaper_private.json',
    ],

    'webrtc_schema_files': [
      'cast_streaming_receiver_session.idl',
      'cast_streaming_rtp_stream.idl',
      'cast_streaming_session.idl',
      'cast_streaming_udp_transport.idl',
    ],

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

    'chromium_code': 1,
    # Disable schema compiler to generate model extension API code.
    # Only register the extension functions in extension system.
    'conditions': [
      ['chromeos==1', {
        'schema_files': [
          '<@(chromeos_schema_files)',
        ],
      }],
      ['enable_webrtc==1', {
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
