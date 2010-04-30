# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# TODO(akalin): Reference this file from all.gyp so it's always built.
{
  'defines': [
    'FEATURE_ENABLE_SSL',
  ],
  'targets': [
    {
      'target_name': 'sync_listen_notifications',
      'type': 'executable',
      'sources': [
        'sync_listen_notifications.cc',
        # We are directly including the sync_constants.cc and h files to avoid
        # pulling in browser.lib.
        # TODO(akalin): Fix this.
        '<(DEPTH)/chrome/browser/sync/sync_constants.cc',
        '<(DEPTH)/chrome/browser/sync/sync_constants.h',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/chrome/chrome.gyp:notifier',
        '<(DEPTH)/third_party/libjingle/libjingle.gyp:libjingle',
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
