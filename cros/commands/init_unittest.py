# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module tests the init module."""

from __future__ import print_function

import glob

from chromite.lib import commandline
from chromite.lib import cros_build_lib_unittest
from chromite.lib import cros_import
from chromite.lib import cros_test_lib
from chromite.lib import partial_mock
from chromite.cros import commands


class MockCommand(partial_mock.PartialMock):
  """Mock class for a generic cros command."""
  ATTRS = ('Run',)
  COMMAND = None
  TARGET_CLASS = None

  def __init__(self, args, base_args=None):
    partial_mock.PartialMock.__init__(self)
    self.args = args
    self.rc_mock = cros_build_lib_unittest.RunCommandMock()
    self.rc_mock.SetDefaultCmdResult()
    parser = commandline.ArgumentParser(caching=True)
    subparsers = parser.add_subparsers()
    subparser = subparsers.add_parser(self.COMMAND, caching=True)
    self.TARGET_CLASS.AddParser(subparser)

    args = base_args if base_args else []
    args += [self.COMMAND] + self.args
    options = parser.parse_args(args)
    self.inst = options.cros_class(options)

  def Run(self, inst):
    with self.rc_mock:
      self.backup['Run'](inst)


class CommandTest(cros_test_lib.MockTestCase):
  """This test class tests that we can load modules correctly."""

  # pylint: disable=W0212

  def testFindModules(self):
    """Tests that we can return modules correctly when mocking out glob."""
    fake_command_file = 'cros_command_test.py'
    filtered_file = 'cros_command_unittest.py'
    mydir = 'mydir'

    self.PatchObject(glob, 'glob',
                     return_value=[fake_command_file, filtered_file])

    self.assertEqual(commands._FindModules(mydir), [fake_command_file])

  def testLoadCommands(self):
    """Tests import commands correctly."""
    fake_module = 'cros_command_test'
    fake_command_file = '%s.py' % fake_module
    module_path = ('chromite', 'cros', 'commands', fake_module)

    self.PatchObject(commands, '_FindModules', return_value=[fake_command_file])
    # The code doesn't use the return value, so stub it out lazy-like.
    load_mock = self.PatchObject(cros_import, 'ImportModule', return_value=None)

    commands._ImportCommands()

    load_mock.assert_called_with(module_path)

  def testListCommands(self):
    """Tests we get a sane list back."""
    cros_commands = commands.ListCommands()
    # Pick some commands that are likely to not go away.
    self.assertIn('chrome-sdk', cros_commands)
    self.assertIn('flash', cros_commands)
