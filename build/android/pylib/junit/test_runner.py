# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from pylib import cmd_helper
from pylib import constants

class JavaTestRunner(object):
  """Runs java tests on the host."""

  def __init__(self, options):
    self._package_filter = options.package_filter
    self._runner_filter = options.runner_filter
    self._sdk_version = options.sdk_version
    self._test_filter = options.test_filter
    self._test_suite = options.test_suite

  def SetUp(self):
    pass

  def RunTest(self, _test):
    """Runs junit tests from |self._test_suite|."""
    command = ['java',
               '-jar', os.path.join(constants.GetOutDirectory(), 'lib.java',
                                    '%s.jar' % self._test_suite)]
    if self._test_filter:
      command.extend(['-gtest-filter', self._test_filter])
    if self._package_filter:
      command.extend(['-package-filter', self._package_filter])
    if self._runner_filter:
      command.extend(['-runner-filter', self._runner_filter])
    if self._sdk_version:
      command.extend(['-sdk-version', self._sdk_version])
    return cmd_helper.RunCmd(command)

  def TearDown(self):
    pass

