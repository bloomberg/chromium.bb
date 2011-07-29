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
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'test/test_url_fetcher_factory.cc',
        'test/test_url_fetcher_factory.h',
      ],
    },
    {
      'target_name': 'content_unittests',
      'type': 'executable',
      'dependencies': [
	'test_support_content',
        '../base/base.gyp:test_support_base',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'test/run_all_unittests.cc',
      ],
    },
  ],
}
