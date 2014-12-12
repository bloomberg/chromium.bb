# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from pylib.remote.device import remote_device_gtest_run
from pylib.remote.device import remote_device_uirobot_run

def CreateTestRun(args, env, test_instance, error_func):
  if args.environment == 'remote_device':
      if test_instance.TestType() == 'gtest':
        return remote_device_gtest_run.RemoteDeviceGtestRun(env, test_instance)
      if test_instance.TestType() == 'uirobot':
        return remote_device_uirobot_run.RemoteDeviceUirobotRun(
            env, test_instance)
  # TODO(jbudorick) Add local gtest test runs
  # TODO(jbudorick) Add local instrumentation test runs.
  error_func('Unable to create %s test run in %s environment' % (
      test_instance.TestType(), args.environment))
