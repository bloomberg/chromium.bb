# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'dbus',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../build/linux/system.gyp:dbus',
      ],
      'sources': [
        'bus.cc',
        'bus.h',
        'exported_object.h',
        'exported_object.cc',
        'message.cc',
        'message.h',
        'object_proxy.cc',
        'object_proxy.h',
        'scoped_dbus_error.h',
      ],
    },
    {
      'target_name': 'dbus_unittests',
      'type': 'executable',
      'dependencies': [
        'dbus',
        '../base/base.gyp:test_support_base',
        '../testing/gtest.gyp:gtest',
        '../build/linux/system.gyp:dbus',
      ],
      'sources': [
        '../base/test/run_all_unittests.cc',
        'bus_unittest.cc',
        'message_unittest.cc',
        'end_to_end_async_unittest.cc',
        'end_to_end_sync_unittest.cc',
        'test_service.cc',
        'test_service.h',
      ],
      'include_dirs': [
        '..',
      ],
    },
  ],
}
