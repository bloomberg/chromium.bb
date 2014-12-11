# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/dns_prefetch/common
      'target_name': 'dns_prefetch_common',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'sources': [
        'dns_prefetch/common/prefetch_common.cc',
        'dns_prefetch/common/prefetch_common.h',
        'dns_prefetch/common/prefetch_message_generator.cc',
        'dns_prefetch/common/prefetch_message_generator.h',
        'dns_prefetch/common/prefetch_messages.cc',
        'dns_prefetch/common/prefetch_messages.h',
      ],
    },
    {
      # GN version: //components/dns_prefetch/browser
      'target_name': 'dns_prefetch_browser',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../content/content.gyp:content_browser',
        '../net/net.gyp:net',
      ],
      'sources': [
        'dns_prefetch/browser/net_message_filter.cc',
        'dns_prefetch/browser/net_message_filter.h',
      ],
    },
  ],
  'conditions': [
    ['OS!="ios"', {
      'targets': [
        {
          # GN version: //components/dns_prefetch/renderer
          'target_name': 'dns_prefetch_renderer',
          'type': 'static_library',
          'include_dirs': [
            '..',
          ],
          'dependencies': [
            'dns_prefetch_common',
            '../content/content.gyp:content_renderer',
            '../third_party/WebKit/public/blink.gyp:blink',
          ],
          'sources': [
            'dns_prefetch/renderer/predictor_queue.cc',
            'dns_prefetch/renderer/predictor_queue.h',
            'dns_prefetch/renderer/prescient_networking_dispatcher.cc',
            'dns_prefetch/renderer/prescient_networking_dispatcher.h',
            'dns_prefetch/renderer/renderer_net_predictor.cc',
            'dns_prefetch/renderer/renderer_net_predictor.h',
          ],
        },
      ],
    }],
  ],
}
