# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # This turns on e.g. the filename-based detection of which
    # platforms to include source files on (e.g. files ending in
    # _mac.h or _mac.cc are only compiled on MacOSX).
    'chromium_code': 1,
  },
  'includes': [
    'auto_login_parser.gypi',
    'autofill.gypi',
    'bookmarks.gypi',
    'captive_portal.gypi',
    'cloud_devices.gypi',
    'component_updater.gypi',
    'content_settings.gypi',
    'crash.gypi',
    'cronet.gypi',
    'crx_file.gypi',
    'data_reduction_proxy.gypi',
    'device_event_log.gypi',
    'dom_distiller.gypi',
    'domain_reliability.gypi',
    'enhanced_bookmarks.gypi',
    'error_page.gypi',
    'favicon.gypi',
    'favicon_base.gypi',
    'gcm_driver.gypi',
    'google.gypi',
    'guest_view.gypi',
    'handoff.gypi',
    'history.gypi',
    'infobars.gypi',
    'invalidation.gypi',
    'json_schema.gypi',
    'keyed_service.gypi',
    'language_usage_metrics.gypi',
    'leveldb_proto.gypi',
    'login.gypi',
    'metrics.gypi',
    'navigation_metrics.gypi',
    'network_hints.gypi',
    'network_time.gypi',
    'offline_pages.gypi',
    'omnibox.gypi',
    'onc.gypi',
    'open_from_clipboard.gypi',
    'os_crypt.gypi',
    'ownership.gypi',
    'packed_ct_ev_whitelist.gypi',
    'password_manager.gypi',
    'plugins.gypi',
    'policy.gypi',
    'precache.gypi',
    'pref_registry.gypi',
    'proxy_config.gypi',
    'query_parser.gypi',
    'rappor.gypi',
    'renderer_context_menu.gypi',
    'search.gypi',
    'search_engines.gypi',
    'search_provider_logos.gypi',
    'security_interstitials.gypi',
    'sessions.gypi',
    'signin.gypi',
    'startup_metric_utils.gypi',
    'suggestions.gypi',
    'sync_driver.gypi',
    'toolbar.gypi',
    'translate.gypi',
    'ui_zoom.gypi',
    'undo.gypi',
    'update_client.gypi',
    'upload_list.gypi',
    'url_matcher.gypi',
    'user_prefs.gypi',
    'variations.gypi',
    'version_info.gypi',
    'wallpaper.gypi',
    'web_resource.gypi',
    'webdata.gypi',
    'webdata_services.gypi',
  ],
  'conditions': [
    ['OS == "android"', {
      'includes': [
        'external_video_surface.gypi',
        'service_tab_launcher.gypi',
      ],
    }],
    ['OS != "ios"', {
      'includes': [
        'app_modal.gypi',
        'browsing_data.gypi',
        'cdm.gypi',
        'devtools_discovery.gypi',
        'devtools_http_handler.gypi',
        'drive.gypi',
        'message_port.gypi',
        'navigation_interception.gypi',
        'power.gypi',
        'safe_json.gypi',
        'visitedlink.gypi',
        'web_cache.gypi',
        'web_contents_delegate_android.gypi',
        'web_modal.gypi',
      ],
    }],
    ['OS == "ios"', {
      'includes': [
        'webp_transcode.gypi',
      ],
    }],
    ['OS != "ios" and OS != "android"', {
      'includes': [
        'audio_modem.gypi',
        'copresence.gypi',
        'feedback.gypi',
        'proximity_auth.gypi',
        'storage_monitor.gypi',
      ]
    }],
    ['chromeos == 1', {
      'includes': [
        'pairing.gypi',
        'timers.gypi',
        'wifi_sync.gypi',
      ],
    }],
    ['OS == "win" or OS == "mac"', {
      'includes': [
        'wifi.gypi',
      ],
    }],
    ['OS == "win"', {
      'includes': [
        'browser_watcher.gypi',
      ],
    }],
    ['chromeos == 1 or use_ash == 1', {
      'includes': [
        'session_manager.gypi',
        'user_manager.gypi',
      ],
    }],
    ['toolkit_views==1', {
      'includes': [
        'constrained_window.gypi',
      ],
    }],
    ['enable_basic_printing==1 or enable_print_preview==1', {
      'includes': [
        'printing.gypi',
      ],
    }],
    ['enable_plugins==1', {
      'includes': [
        'pdf.gypi',
      ],
    }],
    # TODO(tbarzic): Remove chromeos condition when there are non-chromeos apps
    # in components/apps.
    ['enable_extensions == 1 and chromeos == 1', {
      'includes': [
        'chrome_apps.gypi',
      ],
    }],
    ['enable_rlz_support==1', {
      'includes': [
        'rlz.gypi',
      ],
    }]
  ],
}
