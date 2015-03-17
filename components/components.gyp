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
    'network_hints.gypi',
    'dom_distiller.gypi',
    'domain_reliability.gypi',
    'enhanced_bookmarks.gypi',
    'error_page.gypi',
    'favicon_base.gypi',
    'google.gypi',
    'handoff.gypi',
    'infobars.gypi',
    'json_schema.gypi',
    'keyed_service.gypi',
    'language_usage_metrics.gypi',
    'leveldb_proto.gypi',
    'login.gypi',
    'metrics.gypi',
    'navigation_metrics.gypi',
    'network_time.gypi',
    'onc.gypi',
    'os_crypt.gypi',
    'ownership.gypi',
    'packed_ct_ev_whitelist.gypi',
    'password_manager.gypi',
    'policy.gypi',
    'precache.gypi',
    'pref_registry.gypi',
    'query_parser.gypi',
    'rappor.gypi',
    'search.gypi',
    'search_provider_logos.gypi',
    'sessions.gypi',
    'signin.gypi',
    'startup_metric_utils.gypi',
    'suggestions.gypi',
    'translate.gypi',
    'ui_zoom.gypi',
    'update_client.gypi',
    'url_fixer.gypi',
    'url_matcher.gypi',
    'user_prefs.gypi',
    'variations.gypi',
    'wallpaper.gypi',
    'web_resource.gypi',
    'webdata.gypi',
  ],
  'conditions': [
    ['OS != "ios"', {
      'includes': [
        'app_modal.gypi',
        'cdm.gypi',
        'navigation_interception.gypi',
        'plugins.gypi',
        'power.gypi',
        'visitedlink.gypi',
        'web_cache.gypi',
        'web_contents_delegate_android.gypi',
        'web_modal.gypi',
        'webui_generator.gypi',
      ],
    }],
    ['OS == "ios"', {
      'includes': [
        'open_from_clipboard.gypi',
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
    ['android_webview_build == 0', {
      # Android WebView fails to build if a dependency on these targets is
      # introduced.
      'includes': [
        'favicon.gypi',
        'gcm_driver.gypi',
        'history.gypi',
        'omnibox.gypi',
        'renderer_context_menu.gypi',
        'search_engines.gypi',
        'sync_driver.gypi',
        'invalidation.gypi',
        'webdata_services.gypi',
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
  ],
}
