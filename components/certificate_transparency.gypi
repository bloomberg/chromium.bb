# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/certificate_transparency
      'target_name': 'certificate_transparency',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../components/components.gyp:safe_json',
        '../net/net.gyp:net',
        '../url/url.gyp:url_lib',
      ],
      'sources': [
        'certificate_transparency/log_proof_fetcher.h',
        'certificate_transparency/log_proof_fetcher.cc',
      ],
    }
  ],
}
