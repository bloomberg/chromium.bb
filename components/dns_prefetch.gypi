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
          ],
          'sources': [
            'dns_prefetch/renderer/predictor_queue.cc',
            'dns_prefetch/renderer/predictor_queue.h',
            'dns_prefetch/renderer/renderer_net_predictor.cc',
            'dns_prefetch/renderer/renderer_net_predictor.h',
          ],
        },
      ],
    }],
  ],
}
