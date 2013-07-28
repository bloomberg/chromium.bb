# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Class for running uiautomator tests on a single device."""

from pylib.instrumentation import test_runner as instr_test_runner


class TestRunner(instr_test_runner.TestRunner):
  """Responsible for running a series of tests connected to a single device."""

  def __init__(self, package_name, build_type, test_data, save_perf_json,
               screenshot_failures, tool, wait_for_debugger,
               disable_assertions, push_deps, cleanup_test_files, device,
               shard_index, test_pkg, ports_to_forward):
    """Create a new TestRunner.

    Args:
      package_name: Application package name under test.
      See the super class for all other args.
    """
    super(TestRunner, self).__init__(
        build_type, test_data, save_perf_json, screenshot_failures, tool,
        wait_for_debugger, disable_assertions, push_deps, cleanup_test_files,
        device, shard_index, test_pkg, ports_to_forward)

    self.package_name = package_name

  #override
  def InstallTestPackage(self):
      self.test_pkg.Install(self.adb)

  #override
  def PushDataDeps(self):
    pass

  #override
  def _RunTest(self, test, timeout):
    self.adb.ClearApplicationState(self.package_name)
    if 'Feature:FirstRunExperience' in self.test_pkg.GetTestAnnotations(test):
      self.flags.RemoveFlags(['--disable-fre'])
    else:
      self.flags.AddFlags(['--disable-fre'])
    return self.adb.RunUIAutomatorTest(
        test, self.test_pkg.GetPackageName(), timeout)
