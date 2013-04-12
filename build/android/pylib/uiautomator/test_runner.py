# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Class for running uiautomator tests on a single device."""

from pylib.instrumentation import test_runner as instr_test_runner


class TestRunner(instr_test_runner.TestRunner):
  """Responsible for running a series of tests connected to a single device."""

  def __init__(self, options, device, shard_index, test_pkg, ports_to_forward):
    """Create a new TestRunner.

    Args:
      options: An options object similar to the one in parent class plus:
        - package_name: Application package name under test.
    """
    options.ensure_value('install_apk', True)
    options.ensure_value('wait_for_debugger', False)
    super(TestRunner, self).__init__(
        options, device, shard_index, test_pkg, ports_to_forward)

    self.package_name = options.package_name

  #override
  def PushDependencies(self):
      self.test_pkg.Install(self.adb)

  #override
  def _RunTest(self, test, timeout):
    self.adb.ClearApplicationState(self.package_name)
    if 'Feature:FirstRunExperience' in self.test_pkg.GetTestAnnotations(test):
      self.flags.RemoveFlags(['--disable-fre'])
    else:
      self.flags.AddFlags(['--disable-fre'])
    return self.adb.RunUIAutomatorTest(
        test, self.test_pkg.GetPackageName(), timeout)

