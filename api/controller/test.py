# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test controller.

Handles all testing related functionality, it is not itself a test.
"""

from __future__ import print_function

import os

from chromite.api.controller import controller_util
from chromite.cbuildbot import commands
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import failures_lib
from chromite.lib import osutils
from chromite.lib import portage_util
from chromite.lib import sysroot_lib
from chromite.service import test


def DebugInfoTest(input_proto, _output_proto):
  """Run the debug info tests."""
  sysroot_path = input_proto.sysroot.path
  target_name = input_proto.sysroot.build_target.name

  if not sysroot_path:
    if target_name:
      sysroot_path = cros_build_lib.GetSysroot(target_name)
    else:
      cros_build_lib.Die("The sysroot path or the sysroot's build target name "
                         'must be provided.')

  # We could get away with out this, but it's a cheap check.
  sysroot = sysroot_lib.Sysroot(sysroot_path)
  if not sysroot.Exists():
    cros_build_lib.Die('The provided sysroot does not exist.')

  return 0 if test.DebugInfoTest(sysroot_path) else 1


def BuildTargetUnitTest(input_proto, output_proto):
  """Run a build target's ebuild unit tests."""
  # Required args.
  board = input_proto.build_target.name
  result_path = input_proto.result_path

  if not board:
    cros_build_lib.Die('build_target.name is required.')
  if not result_path:
    cros_build_lib.Die('result_path is required.')

  # Chroot handling.
  chroot = input_proto.chroot.path
  cache_dir = input_proto.chroot.cache_dir

  chroot_args = []
  if chroot:
    chroot_args.extend(['--chroot', chroot])
  else:
    chroot = constants.DEFAULT_CHROOT_PATH

  if cache_dir:
    chroot_args.extend(['--cache_dir', cache_dir])

  # TODO(crbug.com/954609) Service implementation.
  extra_env = {'USE': 'chrome_internal'}
  base_dir = os.path.join(chroot, 'tmp')
  # The called code also sets the status file in some cases, but the hacky
  # call makes it not always work, so set up our own just in case.
  with osutils.TempDir(base_dir=base_dir) as tempdir:
    full_sf_path = os.path.join(tempdir, 'status_file')
    chroot_sf_path = full_sf_path.replace(chroot, '')
    extra_env[constants.PARALLEL_EMERGE_STATUS_FILE_ENVVAR] = chroot_sf_path

    try:
      commands.RunUnitTests(constants.SOURCE_ROOT, board, extra_env=extra_env,
                            chroot_args=chroot_args)
    except failures_lib.PackageBuildFailure as e:
      # Add the failed packages.
      for pkg in e.failed_packages:
        cpv = portage_util.SplitCPV(pkg, strict=False)
        package_info = output_proto.failed_packages.add()
        controller_util.CPVToPackageInfo(cpv, package_info)

      return 1
    except failures_lib.BuildScriptFailure:
      # Check our status file for failed packages in case the calling code's
      # file didn't get set.
      for cpv in portage_util.ParseParallelEmergeStatusFile(full_sf_path):
        package_info = output_proto.failed_packages.add()
        controller_util.CPVToPackageInfo(cpv, package_info)

      return 1

    tarball = _BuildUnittestTarball(chroot, board, result_path)
    if tarball:
      output_proto.tarball_path = tarball


def _BuildUnittestTarball(chroot, board, result_path):
  """Build the unittest tarball."""
  tarball = 'unit_tests.tar'
  tarball_path = os.path.join(result_path, tarball)

  cwd = os.path.join(chroot, 'build', board, constants.UNITTEST_PKG_PATH)

  result = cros_build_lib.CreateTarball(tarball_path, cwd, chroot=chroot,
                                        compression=cros_build_lib.COMP_NONE,
                                        error_code_ok=True)

  return tarball_path if result.returncode == 0 else None
