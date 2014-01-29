# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'password_manager_core_test_support',
      'type': 'static_library',
      'dependencies': [
        'autofill_core_common',
        '../base/base.gyp:base',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'password_manager/core/password_form_data.cc',
        'password_manager/core/password_form_data.h',
      ],
    },
  ],
}
