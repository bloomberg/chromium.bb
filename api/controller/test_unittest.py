# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The test controller tests."""

from __future__ import print_function

from chromite.api.controller import test as test_controller
from chromite.api.gen.chromite.api import test_pb2
from chromite.cbuildbot import commands
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import failures_lib
from chromite.lib import osutils
from chromite.lib import portage_util


class UnitTestTest(cros_test_lib.MockTempDirTestCase):
  """Tests for the UnitTest function."""

  def _GetInput(self, board=None, result_path=None, chroot_path=None,
                cache_dir=None):
    """Helper to build an input message instance."""
    return test_pb2.BuildTargetUnitTestRequest(
        build_target={'name': board}, result_path=result_path,
        chroot={'path': chroot_path, 'cache_dir': cache_dir}
    )

  def _GetOutput(self):
    """Helper to get an empty output message instance."""
    return test_pb2.BuildTargetUnitTestResponse()

  def testNoArgumentFails(self):
    """Test no arguments fails."""
    input_msg = self._GetInput()
    output_msg = self._GetOutput()
    with self.assertRaises(cros_build_lib.DieSystemExit):
      test_controller.BuildTargetUnitTest(input_msg, output_msg)

  def testNoBuildTargetFails(self):
    """Test missing build target name fails."""
    input_msg = self._GetInput(result_path=self.tempdir)
    output_msg = self._GetOutput()
    with self.assertRaises(cros_build_lib.DieSystemExit):
      test_controller.BuildTargetUnitTest(input_msg, output_msg)

  def testNoResultPathFails(self):
    """Test missing result path fails."""
    # Missing result_path.
    input_msg = self._GetInput(board='board')
    output_msg = self._GetOutput()
    with self.assertRaises(cros_build_lib.DieSystemExit):
      test_controller.BuildTargetUnitTest(input_msg, output_msg)

  def testPackageBuildFailure(self):
    """Test handling of raised BuildPackageFailure."""
    tempdir = osutils.TempDir(base_dir=self.tempdir)
    self.PatchObject(osutils, 'TempDir', return_value=tempdir)

    pkgs = ['cat/pkg', 'foo/bar']
    expected = [('cat', 'pkg'), ('foo', 'bar')]
    rce = cros_build_lib.RunCommandError('error',
                                         cros_build_lib.CommandResult())
    error = failures_lib.PackageBuildFailure(rce, 'shortname', pkgs)
    self.PatchObject(commands, 'RunUnitTests', side_effect=error)

    input_msg = self._GetInput(board='board', result_path=self.tempdir)
    output_msg = self._GetOutput()

    rc = test_controller.BuildTargetUnitTest(input_msg, output_msg)

    self.assertNotEqual(0, rc)
    self.assertTrue(output_msg.failed_packages)
    failed = []
    for pi in output_msg.failed_packages:
      failed.append((pi.category, pi.package_name))
    self.assertItemsEqual(expected, failed)

  def testPopulatedEmergeFile(self):
    """Test build script failure due to using outside emerge status file."""
    tempdir = osutils.TempDir(base_dir=self.tempdir)
    self.PatchObject(osutils, 'TempDir', return_value=tempdir)

    pkgs = ['cat/pkg', 'foo/bar']
    cpvs = [portage_util.SplitCPV(pkg, strict=False) for pkg in pkgs]
    expected = [('cat', 'pkg'), ('foo', 'bar')]
    rce = cros_build_lib.RunCommandError('error',
                                         cros_build_lib.CommandResult())
    error = failures_lib.BuildScriptFailure(rce, 'shortname')
    self.PatchObject(commands, 'RunUnitTests', side_effect=error)
    self.PatchObject(portage_util, 'ParseParallelEmergeStatusFile',
                     return_value=cpvs)

    input_msg = self._GetInput(board='board', result_path=self.tempdir)
    output_msg = self._GetOutput()

    rc = test_controller.BuildTargetUnitTest(input_msg, output_msg)

    self.assertNotEqual(0, rc)
    self.assertTrue(output_msg.failed_packages)
    failed = []
    for pi in output_msg.failed_packages:
      failed.append((pi.category, pi.package_name))
    self.assertItemsEqual(expected, failed)

  def testOtherBuildScriptFailure(self):
    """Test build script failure due to non-package emerge error."""
    tempdir = osutils.TempDir(base_dir=self.tempdir)
    self.PatchObject(osutils, 'TempDir', return_value=tempdir)

    rce = cros_build_lib.RunCommandError('error',
                                         cros_build_lib.CommandResult())
    error = failures_lib.BuildScriptFailure(rce, 'shortname')
    self.PatchObject(commands, 'RunUnitTests', side_effect=error)
    self.PatchObject(portage_util, 'ParseParallelEmergeStatusFile',
                     return_value=[])

    input_msg = self._GetInput(board='board', result_path=self.tempdir)
    output_msg = self._GetOutput()

    rc = test_controller.BuildTargetUnitTest(input_msg, output_msg)

    self.assertNotEqual(0, rc)
    self.assertFalse(output_msg.failed_packages)
