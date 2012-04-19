# Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
        '<(DEPTH)/net/net.gyp:net',
        '<(DEPTH)/net/net.gyp:net_test_support',
        '<(DEPTH)/sync/sync.gyp:sync_notifier',
      ],
    },
  ],
}
