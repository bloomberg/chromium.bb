# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'security_tests',
      'type': 'shared_library',
      'sources': [
        '../../../sandbox/tests/validation_tests/commands.cc',
        '../../../sandbox/tests/validation_tests/commands.h',
        '../injection_test_dll.h',
        'ipc_security_tests.cc',
        'ipc_security_tests.h',
        'security_tests.cc',
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
