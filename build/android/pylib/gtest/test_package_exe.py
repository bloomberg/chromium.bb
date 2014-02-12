# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Defines TestPackageExecutable to help run stand-alone executables."""

import logging
import os
import sys
import tempfile

from pylib import cmd_helper
from pylib import constants
from pylib import pexpect
from pylib.gtest.test_package import TestPackage


class TestPackageExecutable(TestPackage):
  """A helper class for running stand-alone executables."""

  _TEST_RUNNER_RET_VAL_FILE = 'gtest_retval'

  def __init__(self, suite_name):
    """
    Args:
      suite_name: Name of the test suite (e.g. base_unittests).
    """
    TestPackage.__init__(self, suite_name)
    self.suite_path = os.path.join(constants.GetOutDirectory(), suite_name)
    self._symbols_dir = os.path.join(constants.GetOutDirectory(),
                                     'lib.target')

  #override
  def GetGTestReturnCode(self, adb):
    ret = None
    ret_code = 1  # Assume failure if we can't find it
    ret_code_file = tempfile.NamedTemporaryFile()
    try:
      if not adb.Adb().Pull(
          constants.TEST_EXECUTABLE_DIR + '/' +
          TestPackageExecutable._TEST_RUNNER_RET_VAL_FILE,
          ret_code_file.name):
        logging.critical('Unable to pull gtest ret val file %s',
                         ret_code_file.name)
        raise ValueError
      ret_code = file(ret_code_file.name).read()
      ret = int(ret_code)
    except ValueError:
      logging.critical('Error reading gtest ret val file %s [%s]',
                       ret_code_file.name, ret_code)
      ret = 1
    return ret

  @staticmethod
  def _AddNativeCoverageExports(adb):
    # export GCOV_PREFIX set the path for native coverage results
    # export GCOV_PREFIX_STRIP indicates how many initial directory
    #                          names to strip off the hardwired absolute paths.
    #                          This value is calculated in buildbot.sh and
    #                          depends on where the tree is built.
    # Ex: /usr/local/google/code/chrome will become
    #     /code/chrome if GCOV_PREFIX_STRIP=3
    try:
      depth = os.environ['NATIVE_COVERAGE_DEPTH_STRIP']
    except KeyError:
      logging.info('NATIVE_COVERAGE_DEPTH_STRIP is not defined: '
                   'No native coverage.')
      return ''
    export_string = ('export GCOV_PREFIX="%s/gcov"\n' %
                     adb.GetExternalStorage())
    export_string += 'export GCOV_PREFIX_STRIP=%s\n' % depth
    return export_string

  #override
  def ClearApplicationState(self, adb):
    adb.KillAllBlocking(self.suite_name, 30)

  #override
  def CreateCommandLineFileOnDevice(self, adb, test_filter, test_arguments):
    tool_wrapper = self.tool.GetTestWrapper()
    sh_script_file = tempfile.NamedTemporaryFile()
    # We need to capture the exit status from the script since adb shell won't
    # propagate to us.
    sh_script_file.write('cd %s\n'
                         '%s'
                         '%s %s/%s --gtest_filter=%s %s\n'
                         'echo $? > %s' %
                         (constants.TEST_EXECUTABLE_DIR,
                          self._AddNativeCoverageExports(adb),
                          tool_wrapper, constants.TEST_EXECUTABLE_DIR,
                          self.suite_name,
                          test_filter, test_arguments,
                          TestPackageExecutable._TEST_RUNNER_RET_VAL_FILE))
    sh_script_file.flush()
    cmd_helper.RunCmd(['chmod', '+x', sh_script_file.name])
    adb.PushIfNeeded(
        sh_script_file.name,
        constants.TEST_EXECUTABLE_DIR + '/chrome_test_runner.sh')
    logging.info('Conents of the test runner script: ')
    for line in open(sh_script_file.name).readlines():
      logging.info('  ' + line.rstrip())

  #override
  def GetAllTests(self, adb):
    all_tests = adb.RunShellCommand(
        '%s %s/%s --gtest_list_tests' %
        (self.tool.GetTestWrapper(),
         constants.TEST_EXECUTABLE_DIR,
         self.suite_name))
    return self._ParseGTestListTests(all_tests)

  #override
  def SpawnTestProcess(self, adb):
    args = ['adb', '-s', adb.GetDevice(), 'shell', 'sh',
            constants.TEST_EXECUTABLE_DIR + '/chrome_test_runner.sh']
    logging.info(args)
    return pexpect.spawn(args[0], args[1:], logfile=sys.stdout)

  #override
  def Install(self, adb):
    if self.tool.NeedsDebugInfo():
      target_name = self.suite_path
    else:
      target_name = self.suite_path + '_stripped'
      if not os.path.isfile(target_name):
        raise Exception('Did not find %s, build target %s' %
                        (target_name, self.suite_name + '_stripped'))

      target_mtime = os.stat(target_name).st_mtime
      source_mtime = os.stat(self.suite_path).st_mtime
      if target_mtime < source_mtime:
        raise Exception(
            'stripped binary (%s, timestamp %d) older than '
            'source binary (%s, timestamp %d), build target %s' %
            (target_name, target_mtime, self.suite_path, source_mtime,
             self.suite_name + '_stripped'))

    test_binary = constants.TEST_EXECUTABLE_DIR + '/' + self.suite_name
    adb.PushIfNeeded(target_name, test_binary)
