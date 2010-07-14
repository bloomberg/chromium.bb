# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'chrome_async_socket',
      'type': '<(library)',
      'sources': [
        'chrome_async_socket.cc',
        'chrome_async_socket.h',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/net/net.gyp:net',
        '<(DEPTH)/third_party/libjingle/libjingle.gyp:libjingle',
      ],
      'export_dependent_settings': [
        '<(DEPTH)/third_party/libjingle/libjingle.gyp:libjingle',
      ],
    },
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
        'chrome_async_socket',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/chrome/chrome.gyp:common_constants',
        '<(DEPTH)/chrome/chrome.gyp:sync_notifier',
        '<(DEPTH)/jingle/jingle.gyp:notifier',
        '<(DEPTH)/third_party/libjingle/libjingle.gyp:libjingle',
      ],
    },
    {
      'target_name': 'chrome_async_socket_unit_tests',
      'type': 'executable',
      'sources': [
        # TODO(akalin): Write our own test suite and runner.
        '<(DEPTH)/base/test/run_all_unittests.cc',
        '<(DEPTH)/base/test/test_suite.h',
        'chrome_async_socket_unittest.cc',
      ],
      'dependencies': [
        'chrome_async_socket',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/net/net.gyp:net',
        '<(DEPTH)/net/net.gyp:net_test_support',
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
      # TODO(akalin): Remove this once we have our own test suite and
      # runner.
      'conditions': [
        ['OS == "linux" or OS == "freebsd" or OS == "openbsd" or OS == "solaris"', {
          'dependencies': [
            # Needed to handle the #include chain:
            #   base/test/test_suite.h
            #   gtk/gtk.h
            '<(DEPTH)/build/linux/system.gyp:gtk',
          ],
        }],
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
