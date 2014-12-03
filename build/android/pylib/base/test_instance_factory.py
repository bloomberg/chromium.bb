# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from pylib import constants
from pylib.gtest import gtest_test_instance
from pylib.utils import isolator


def CreateTestInstance(args, error_func):

  if args.command == 'gtest':
    return gtest_test_instance.GtestTestInstance(
        args, isolator.Isolator(constants.ISOLATE_DEPS_DIR))
  # TODO(jbudorick) Add instrumentation test instance.

  error_func('Unable to create %s test instance.' % args.command)

