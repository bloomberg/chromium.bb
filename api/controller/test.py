# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test controller.

Handles all testing related functionality, it is not itself a test.
"""

from __future__ import print_function

import os

from chromite.api import controller
from chromite.api.controller import controller_util
from chromite.api.gen.chromite.api import test_pb2
from chromite.cbuildbot import commands
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import failures_lib
from chromite.lib import image_lib
from chromite.lib import osutils
from chromite.lib import portage_util
from chromite.lib import sysroot_lib
from chromite.scripts import cros_set_lsb_release
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

  if test.DebugInfoTest(sysroot_path):
    return controller.RETURN_CODE_SUCCESS
  else:
    return controller.RETURN_CODE_COMPLETED_UNSUCCESSFULLY


def BuildTargetUnitTest(input_proto, output_proto):
  """Run a build target's ebuild unit tests."""
  # Required args.
  board = input_proto.build_target.name
  result_path = input_proto.result_path

  if not board:
    cros_build_lib.Die('build_target.name is required.')
  if not result_path:
    cros_build_lib.Die('result_path is required.')

  # Method flags.
  # An empty sysroot means build packages was not run.
  was_built = not input_proto.flags.empty_sysroot

  # Skipped tests.
  blacklisted_package_info = input_proto.package_blacklist
  blacklist = []
  for package_info in blacklisted_package_info:
    blacklist.append(controller_util.PackageInfoToString(package_info))

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
                            chroot_args=chroot_args, build_stage=was_built,
                            blacklist=blacklist)
    except failures_lib.PackageBuildFailure as e:
      # Add the failed packages.
      for pkg in e.failed_packages:
        cpv = portage_util.SplitCPV(pkg, strict=False)
        package_info = output_proto.failed_packages.add()
        controller_util.CPVToPackageInfo(cpv, package_info)

      if e.failed_packages:
        return controller.RETURN_CODE_UNSUCCESSFUL_RESPONSE_AVAILABLE
      else:
        return controller.RETURN_CODE_COMPLETED_UNSUCCESSFULLY
    except failures_lib.BuildScriptFailure:
      # Check our status file for failed packages in case the calling code's
      # file didn't get set.
      failed_packages = portage_util.ParseParallelEmergeStatusFile(full_sf_path)
      for cpv in failed_packages:
        package_info = output_proto.failed_packages.add()
        controller_util.CPVToPackageInfo(cpv, package_info)

      if failed_packages:
        return controller.RETURN_CODE_UNSUCCESSFUL_RESPONSE_AVAILABLE
      else:
        return controller.RETURN_CODE_COMPLETED_UNSUCCESSFULLY

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


def ChromiteUnitTest(_input_proto, _output_proto):
  """Run the chromite unit tests."""
  cmd = [os.path.join(constants.CHROMITE_DIR, 'scripts', 'run_tests')]
  result = cros_build_lib.RunCommand(cmd, error_code_ok=True)
  if result.returncode == 0:
    return controller.RETURN_CODE_SUCCESS
  else:
    return controller.RETURN_CODE_COMPLETED_UNSUCCESSFULLY


def VmTest(input_proto, _output_proto):
  """Run VM tests."""
  if not input_proto.HasField('build_target'):
    cros_build_lib.Die('build_target is required')
  build_target = input_proto.build_target

  vm_path = input_proto.vm_path
  if not vm_path.path:
    cros_build_lib.Die('vm_path.path is required.')

  test_harness = input_proto.test_harness
  if test_harness == test_pb2.VmTestRequest.UNSPECIFIED:
    cros_build_lib.Die('test_harness is required')

  vm_tests = input_proto.vm_tests
  if not vm_tests:
    cros_build_lib.Die('vm_tests must contain at least one element')

  cmd = ['cros_run_test', '--debug', '--no-display', '--copy-on-write',
         '--board', build_target.name, '--image-path', vm_path.path,
         '--%s' % test_pb2.VmTestRequest.TestHarness.Name(test_harness).lower()]
  cmd.extend(vm_test.pattern for vm_test in vm_tests)

  if input_proto.ssh_options.port:
    cmd.extend(['--ssh-port', str(input_proto.ssh_options.port)])

  if input_proto.ssh_options.private_key_path:
    cmd.extend(['--private-key', input_proto.ssh_options.private_key_path.path])

  # TODO(evanhernandez): Find a nice way to pass test_that-args through
  # the build API. Or obviate them.
  if test_harness == test_pb2.VmTestRequest.AUTOTEST:
    cmd.append('--test_that-args=--whitelist-chrome-crashes')

  with osutils.TempDir(prefix='vm-test-results.') as results_dir:
    cmd.extend(['--results-dir', results_dir])
    cros_build_lib.RunCommand(cmd, kill_timeout=10 * 60)


def MoblabVmTest(input_proto, _output_proto):
  """Run Moblab VM tests."""
  image_payload_dir = input_proto.image_payload.path.path
  cache_payload_dirs = [cp.path.path for cp in input_proto.cache_payloads]

  # Autotest and Moblab depend on the builder path, so we must read it from
  # the image.
  image_file = os.path.join(image_payload_dir, constants.TEST_IMAGE_BIN)
  with image_lib.LoopbackPartitions(image_file) as image_mount:
    lsb_release_file = os.path.join(image_mount.destination,
                                    constants.LSB_RELEASE_PATH.strip('/'))
    lsb_release_kvs = cros_build_lib.LoadKeyValueFile(lsb_release_file)
    builder = lsb_release_kvs.get(cros_set_lsb_release.LSB_KEY_BUILDER_PATH)

  if not builder:
    cros_build_lib.Die('Image did not contain key %s in %s',
                       cros_set_lsb_release.LSB_KEY_BUILDER_PATH,
                       constants.LSB_RELEASE_PATH)

  # Now we can run the tests.
  with osutils.TempDir() as workspace_dir, osutils.TempDir() as results_dir:
    vms = test.CreateMoblabVm(workspace_dir, image_payload_dir)
    cache_dir = test.PrepareMoblabVmImageCache(vms, builder, cache_payload_dirs)
    test.RunMoblabVmTest(vms, builder, cache_dir, results_dir)
    test.ValidateMoblabVmTest(results_dir)
