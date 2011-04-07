# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'sync_listen_notifications',
      'type': 'executable',
      'sources': [
        'sync_listen_notifications.cc',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/chrome/chrome.gyp:sync_notifier',
        '<(DEPTH)/chrome/chrome.gyp:test_support_common',
        '<(DEPTH)/content/content.gyp:content_browser',
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
