# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'password_manager_core_browser',
      'type': 'static_library',
      'dependencies': [
        'autofill_core_common',
        'password_manager_core_common',
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../url/url.gyp:url_lib',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'password_manager/core/browser/psl_matching_helper.cc',
        'password_manager/core/browser/psl_matching_helper.h',
      ],
    },
    {
      'target_name': 'password_manager_core_browser_test_support',
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
        'password_manager/core/browser/password_form_data.cc',
        'password_manager/core/browser/password_form_data.h',
      ],
    },
    {
      'target_name': 'password_manager_core_common',
      'type': 'static_library',
      'dependencies': [
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'password_manager/core/common/password_manager_switches.cc',
        'password_manager/core/common/password_manager_switches.h',
      ],
    },
  ],
}
