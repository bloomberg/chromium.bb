# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'test_support_content',
      'type': 'static_library',
      'dependencies': [
        'content_common',
        '../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # TODO(phajdan.jr): All of those files should live in content/test.
        'common/test_url_constants.cc',
        'common/test_url_constants.h',
        'renderer/mock_content_renderer_client.cc',
        'test/content_test_suite.cc',
        'test/content_test_suite.h',
        'test/test_content_client.cc',
        'test/test_content_client.h',
        'test/test_url_fetcher_factory.cc',
        'test/test_url_fetcher_factory.h',
      ],
    },
    {
      'target_name': 'content_unittests',
      'type': 'executable',
      'dependencies': [
        'content_browser',
        'content_plugin',
        'content_renderer',
        'test_support_content',
        '../base/base.gyp:test_support_base',
        '../crypto/crypto.gyp:crypto',
        '../media/media.gyp:media_test_support',
        '../net/net.gyp:net_test_support',
        '../skia/skia.gyp:skia',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/libjingle/libjingle.gyp:libjingle',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'browser/browser_thread_unittest.cc',
        'browser/child_process_security_policy_unittest.cc',
        'browser/renderer_host/gtk_key_bindings_handler_unittest.cc',
        'browser/ssl/ssl_host_state_unittest.cc',
        'browser/trace_subscriber_stdio_unittest.cc',
        'common/process_watcher_unittest.cc',
        'common/property_bag_unittest.cc',
        'common/resource_dispatcher_unittest.cc',
        'common/url_fetcher_unittest.cc',
        'renderer/active_notification_tracker_unittest.cc',
        'renderer/media/audio_message_filter_unittest.cc',
        'renderer/media/audio_renderer_impl_unittest.cc',
        'renderer/media/capture_video_decoder_unittest.cc',
        'renderer/media/media_stream_dispatcher_unittest.cc',
        'renderer/media/rtc_video_decoder_unittest.cc',
        'renderer/media/video_capture_impl_unittest.cc',
        'renderer/media/video_capture_message_filter_unittest.cc',
        'renderer/paint_aggregator_unittest.cc',
        'test/run_all_unittests.cc',
      ],
      'conditions': [
        ['os_posix!=1', {
          'sources!': [
            # TODO(port): port those unit tests.
            'common/process_watcher_unittest.cc',
          ],
        }],
        ['OS=="win" and win_use_allocator_shim==1', {
          'dependencies': [
            '../base/allocator/allocator.gyp:allocator',
          ],
        }],
        ['chromeos==1', {
          'sources/': [
            ['exclude', '^browser/renderer_host/gtk_key_bindings_handler_unittest.cc'],
          ],
        }],
      ],
    },
  ],
}
