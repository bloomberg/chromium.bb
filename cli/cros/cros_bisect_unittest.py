# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module tests the cros bisect command."""

from __future__ import print_function

from chromite.cros_bisect import autotest_evaluator
from chromite.cros_bisect import git_bisector
from chromite.cros_bisect import simple_chrome_builder
from chromite.cli.cros import cros_bisect
from chromite.lib import commandline
from chromite.lib import cros_test_lib
from chromite.lib import remote_access


class CrosBisectTest(cros_test_lib.MockTestCase):
  """Test BisectCommand."""

  def setUp(self):
    self.parser = commandline.ArgumentParser()
    cros_bisect.BisectCommand.AddParser(self.parser)

  def testAddParser(self):
    # Use commandline.ArgumentParser to get 'path' type support.
    options = self.parser.parse_args(
        ['--good', '900d900d', '--remote', '192.168.0.11',
         '--base-dir', '/tmp/bisect'])
    self.assertTrue(
        simple_chrome_builder.SimpleChromeBuilder.SanityCheckOptions(options))
    self.assertTrue(
        autotest_evaluator.AutotestEvaluator.SanityCheckOptions(options))
    self.assertTrue(
        git_bisector.GitBisector.SanityCheckOptions(options))

  def testRun(self):
    options = self.parser.parse_args(
        ['--good', '900d900d', '--remote', '192.168.0.11',
         '--base-dir', '/tmp/bisect', '--board', 'test'])

    self.PatchObject(simple_chrome_builder.SimpleChromeBuilder, '__init__',
                     return_value=None)
    self.PatchObject(autotest_evaluator.AutotestEvaluator, '__init__',
                     return_value=None)
    self.PatchObject(git_bisector.GitBisector, '__init__',
                     return_value=None)
    self.PatchObject(git_bisector.GitBisector, 'SetUp')
    self.PatchObject(git_bisector.GitBisector, 'Run')


    bisector = cros_bisect.BisectCommand(options)
    bisector.Run()

  def testProcessOptionsReuseFlag(self):
    options = self.parser.parse_args(
        ['--good', '900d900d', '--remote', '192.168.0.11',
         '--base-dir', '/tmp/bisect', '--board', 'test'])
    bisector = cros_bisect.BisectCommand(options)
    bisector.ProcessOptions()
    self.assertFalse(bisector.options.reuse_repo)
    self.assertFalse(bisector.options.reuse_build)
    self.assertFalse(bisector.options.reuse_eval)

    # Flip --reuse.
    options = self.parser.parse_args(
        ['--good', '900d900d', '--remote', '192.168.0.11',
         '--base-dir', '/tmp/bisect', '--board', 'test',
         '--reuse'])
    bisector = cros_bisect.BisectCommand(options)
    bisector.ProcessOptions()
    self.assertTrue(bisector.options.reuse_repo)
    self.assertTrue(bisector.options.reuse_build)
    self.assertTrue(bisector.options.reuse_eval)

  def testProcessOptionsResolveBoard(self):
    # No --board specified.
    options = self.parser.parse_args(
        ['--good', '900d900d', '--remote', '192.168.0.11',
         '--base-dir', '/tmp/bisect'])

    self.PatchObject(remote_access, 'ChromiumOSDevice',
                     return_value=cros_test_lib.EasyAttr(board='test_board'))
    bisector = cros_bisect.BisectCommand(options)
    bisector.ProcessOptions()
    self.assertEqual('test_board', bisector.options.board)

  def testProcessOptionsResolveBoardFailed(self):
    # No --board specified.
    options = self.parser.parse_args(
        ['--good', '900d900d', '--remote', '192.168.0.11',
         '--base-dir', '/tmp/bisect'])

    self.PatchObject(remote_access, 'ChromiumOSDevice',
                     return_value=cros_test_lib.EasyAttr(board=''))
    bisector = cros_bisect.BisectCommand(options)
    self.assertRaisesRegexp(Exception, 'Unable to obtain board name from DUT',
                            bisector.ProcessOptions)
