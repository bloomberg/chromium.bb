# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'domain_reliability',
      'type': '<(component)',
      'dependencies': [
        '../base/base.gyp:base',
        '../content/content.gyp:content_browser',
        '../net/net.gyp:net',
        '../url/url.gyp:url_lib',
      ],
      'include_dirs': [
        '..',
      ],
      'defines': [
        'DOMAIN_RELIABILITY_IMPLEMENTATION',
      ],
      'sources': [
        'domain_reliability/beacon.cc',
        'domain_reliability/beacon.h',
        'domain_reliability/config.cc',
        'domain_reliability/config.h',
        'domain_reliability/context.cc',
        'domain_reliability/context.h',
        'domain_reliability/domain_reliability_export.h',
        'domain_reliability/monitor.cc',
        'domain_reliability/monitor.h',
        'domain_reliability/util.cc',
        'domain_reliability/util.h',
      ],
    },
  ],
}
