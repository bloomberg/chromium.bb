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
    'autofill.gypi',
    'auto_login_parser.gypi',
    'breakpad.gypi',
    'dom_distiller.gypi',
    'json_schema.gypi',
    'navigation_metrics.gypi',
    'onc.gypi',
    'policy.gypi',
    'precache.gypi',
    'startup_metric_utils.gypi',
    'translate.gypi',
    'user_prefs.gypi',
    'variations.gypi',
    'webdata.gypi',
  ],
  'conditions': [
    ['OS != "ios"', {
      'includes': [
        'browser_context_keyed_service.gypi',
        'navigation_interception.gypi',
        'plugins.gypi',
        'sessions.gypi',
        'url_matcher.gypi',
        'visitedlink.gypi',
        'web_contents_delegate_android.gypi',
        'web_modal.gypi',
        'wifi.gypi',
      ],
    }],
  ],
}
