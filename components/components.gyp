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
    'breakpad.gypi',
    'cloud_devices.gypi',
    'cronet.gypi',
    'data_reduction_proxy.gypi',
    'dom_distiller.gypi',
    'domain_reliability.gypi',
    'favicon.gypi',
    'json_schema.gypi',
    'keyed_service.gypi',
    'language_usage_metrics.gypi',
    'metrics.gypi',
    'navigation_metrics.gypi',
    'onc.gypi',
    'os_crypt.gypi',
    'password_manager.gypi',
    'policy.gypi',
    'precache.gypi',
    'rappor.gypi',
    'signin.gypi',
    'startup_metric_utils.gypi',
    'translate.gypi',
    'url_matcher.gypi',
    'user_prefs.gypi',
    'variations.gypi',
    'webdata.gypi',
  ],
  'conditions': [
    ['OS != "ios"', {
      'includes': [
        'navigation_interception.gypi',
        'plugins.gypi',
        'sessions.gypi',
        'storage_monitor.gypi',
        'visitedlink.gypi',
        'web_contents_delegate_android.gypi',
        'web_modal.gypi',
        'wifi.gypi',
      ],
    }],
    ['android_webview_build == 0', {
      # Android WebView fails to build if a dependency on sync.gyp:sync is
      # introduced.
      'includes': [
        'sync_driver.gypi',
      ],
    }],
  ],
}
