# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'cast_certificate',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        'cast_certificate_proto',
      ],
      'sources': [
        'cast_certificate/cast_cert_validator.cc',
        'cast_certificate/cast_cert_validator.h',
        'cast_certificate/cast_crl.cc',
        'cast_certificate/cast_crl.h',
        'cast_certificate/cast_root_ca_cert_der-inc.h',
        'cast_certificate/eureka_root_ca_der-inc.h',
      ],
    },
    {
      'target_name': 'cast_certificate_proto',
      'type': 'static_library',
      'sources': [
        'cast_certificate/proto/revocation.proto',
      ],
      'includes': [
        '../build/protoc.gypi'
      ],
      'variables': {
        'proto_in_dir': 'cast_certificate/proto',
        'proto_out_dir': 'components/cast_certificate/proto',
      },
    },
    {
      'target_name': 'cast_certificate_test_proto',
      'type': 'static_library',
      'sources': [
        'cast_certificate/proto/test_suite.proto',
      ],
      'includes': [
        '../build/protoc.gypi'
      ],
      'variables': {
        'proto_in_dir': 'cast_certificate/proto',
        'proto_out_dir': 'components/cast_certificate/proto',
      },
    },
    {
      'target_name': 'cast_certificate_test_support',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../testing/gtest.gyp:gtest',
        'cast_certificate',
      ],
      'sources': [
        'cast_certificate/cast_cert_validator_test_helpers.cc',
        'cast_certificate/cast_cert_validator_test_helpers.h',
      ],
    },
  ],
}
