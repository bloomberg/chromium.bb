# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module tests the brillo test command."""

from __future__ import print_function

from chromite.cli import command_unittest
from chromite.cli.brillo import brillo_test
from chromite.lib import chroot_util
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import portage_util
from chromite.lib import workon_helper


class FakeWorkonHelper(object):
  """Fakes the workon helper module."""

  def __init__(self, packages):
    self._packages = packages

  def ListAtoms(self, *_args, **_kwargs):
    """Returns the list of packages available."""
    return self._packages


class MockTestCommand(command_unittest.MockCommand):
  """Mock out the `brillo test` command."""
  TARGET = 'chromite.cli.brillo.brillo_test.TestCommand'
  TARGET_CLASS = brillo_test.TestCommand
  COMMAND = 'test'


class TestCommandTest(cros_test_lib.WorkspaceTestCase):
  """Test the flow of TestCommand.Run()."""

  _PACKAGES_WITH_TESTS = set(['chromeos-base/shill',
                              'chromeos-base/metrics'])

  _PACKAGES_WITHOUT_TESTS = set(['brillo-base/foo'])

  def setUp(self):
    """Patches objects."""
    self.cmd_mock = None
    self.run_unittests_mock = None
    self.CreateWorkspace()
    self.blueprint = self.CreateBlueprint()

  def SetupCommandMock(self, cmd_args):
    """Sets up the MockTestCommand."""
    self.cmd_mock = MockTestCommand(cmd_args)
    self.StartPatcher(self.cmd_mock)
    fake_workon = FakeWorkonHelper(
        list(self._PACKAGES_WITH_TESTS | self._PACKAGES_WITHOUT_TESTS))
    self.PatchObject(workon_helper, 'WorkonHelper', return_value=fake_workon)
    self.PatchObject(portage_util, 'PackagesWithTest',
                     side_effect=self._PackagesWithTests)
    self.run_unittests_mock = self.PatchObject(chroot_util, 'RunUnittests')
    self.PatchObject(cros_build_lib, 'IsInsideChroot', return_value=True)

  def _PackagesWithTests(self, _sysroot, packages):
    """Fakes portage_util.PackagesWithTests."""
    return packages & self._PACKAGES_WITH_TESTS

  def RunCommandMock(self, *args, **kwargs):
    """Sets up and runs the MockTestCommand."""
    self.SetupCommandMock(*args, **kwargs)
    self.cmd_mock.inst.Run()

  def testMatchingPackages(self):
    """Tests that we match the correct packages."""
    self.SetupCommandMock([self.blueprint.path, 'foo'])
    # Getting an unknown package.
    unknown = set(['chromeos-base/bar'])
    cmd = self.cmd_mock.inst
    # pylint: disable=protected-access
    self.assertEqual((set(), unknown),
                     cmd._GetMatchingPackages(unknown))

    # Only the names are specified.
    name_only = set(['foo', 'metrics'])
    self.assertEqual((name_only, set()),
                     cmd._GetMatchingPackages(name_only))

    # The full category/package_name is specified.
    full_name = set(['chromeos-base/shill'])
    self.assertEqual((full_name, set()),
                     cmd._GetMatchingPackages(full_name))

    # Both known and unknown packages are passed.
    self.assertEqual((name_only, unknown),
                     cmd._GetMatchingPackages(name_only | unknown))

  def testExistingPackage(self):
    """Tests that RunUnittests is called when the package is valid."""
    self.RunCommandMock([self.blueprint.path, 'chromeos-base/shill'])
    self.assertEqual(1, self.run_unittests_mock.call_count)

  def testUnknownPackage(self):
    """Tests that we don't do anything when the package is not valid."""
    self.RunCommandMock([self.blueprint.path, 'brillo-base/foo'])
    self.assertEqual(0, self.run_unittests_mock.call_count)
