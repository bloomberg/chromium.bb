# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Sysroot controller tests."""

from __future__ import print_function

import os

from chromite.api import api_config
from chromite.api import controller
from chromite.api.controller import sysroot as sysroot_controller
from chromite.api.gen.chromite.api import sysroot_pb2
from chromite.lib import build_target_util
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import portage_util
from chromite.lib import sysroot_lib
from chromite.service import sysroot as sysroot_service


class CreateTest(cros_test_lib.MockTestCase, api_config.ApiConfigMixin):
  """Create function tests."""

  def _InputProto(self, build_target=None, profile=None, replace=False,
                  current=False):
    """Helper to build and input proto instance."""
    proto = sysroot_pb2.SysrootCreateRequest()
    if build_target:
      proto.build_target.name = build_target
    if profile:
      proto.profile.name = profile
    if replace:
      proto.flags.replace = replace
    if current:
      proto.flags.chroot_current = current

    return proto

  def _OutputProto(self):
    """Helper to build output proto instance."""
    return sysroot_pb2.SysrootCreateResponse()

  def testValidateOnly(self):
    """Sanity check that a validate only call does not execute any logic."""
    patch = self.PatchObject(sysroot_service, 'Create')

    board = 'board'
    profile = None
    force = False
    upgrade_chroot = True
    in_proto = self._InputProto(build_target=board, profile=profile,
                                replace=force, current=not upgrade_chroot)
    sysroot_controller.Create(in_proto, self._OutputProto(),
                              self.validate_only_config)
    patch.assert_not_called()

  def testMockCall(self):
    """Sanity check that a mock call does not execute any logic."""
    patch = self.PatchObject(sysroot_service, 'Create')
    request = self._InputProto()
    response = self._OutputProto()

    rc = sysroot_controller.Create(request, response, self.mock_call_config)

    patch.assert_not_called()
    self.assertEqual(controller.RETURN_CODE_SUCCESS, rc)

  def testMockError(self):
    """Sanity check that a mock error does not execute any logic."""
    patch = self.PatchObject(sysroot_service, 'Create')
    request = self._InputProto()
    response = self._OutputProto()

    rc = sysroot_controller.Create(request, response, self.mock_error_config)

    patch.assert_not_called()
    self.assertEqual(controller.RETURN_CODE_UNRECOVERABLE, rc)

  def testArgumentValidation(self):
    """Test the input argument validation."""
    # Error when no name provided.
    in_proto = self._InputProto()
    out_proto = self._OutputProto()
    with self.assertRaises(cros_build_lib.DieSystemExit):
      sysroot_controller.Create(in_proto, out_proto, self.api_config)

    # Valid when board passed.
    result = sysroot_lib.Sysroot('/sysroot/path')
    patch = self.PatchObject(sysroot_service, 'Create', return_value=result)
    in_proto = self._InputProto('board')
    out_proto = self._OutputProto()
    sysroot_controller.Create(in_proto, out_proto, self.api_config)
    patch.assert_called_once()

  def testArgumentHandling(self):
    """Test the arguments get processed and passed correctly."""
    sysroot_path = '/sysroot/path'

    sysroot = sysroot_lib.Sysroot(sysroot_path)
    create_patch = self.PatchObject(sysroot_service, 'Create',
                                    return_value=sysroot)
    target_patch = self.PatchObject(build_target_util, 'BuildTarget')
    rc_patch = self.PatchObject(sysroot_service, 'SetupBoardRunConfig')

    # Default values.
    board = 'board'
    profile = None
    force = False
    upgrade_chroot = True
    in_proto = self._InputProto(build_target=board, profile=profile,
                                replace=force, current=not upgrade_chroot)
    out_proto = self._OutputProto()
    sysroot_controller.Create(in_proto, out_proto, self.api_config)

    # Default value checks.
    target_patch.assert_called_with(name=board, profile=profile)
    rc_patch.assert_called_with(force=force, upgrade_chroot=upgrade_chroot)
    self.assertEqual(board, out_proto.sysroot.build_target.name)
    self.assertEqual(sysroot_path, out_proto.sysroot.path)

    # Not default values.
    create_patch.reset_mock()
    board = 'board'
    profile = 'profile'
    force = True
    upgrade_chroot = False
    in_proto = self._InputProto(build_target=board, profile=profile,
                                replace=force, current=not upgrade_chroot)
    out_proto = self._OutputProto()
    sysroot_controller.Create(in_proto, out_proto, self.api_config)

    # Not default value checks.
    target_patch.assert_called_with(name=board, profile=profile)
    rc_patch.assert_called_with(force=force, upgrade_chroot=upgrade_chroot)
    self.assertEqual(board, out_proto.sysroot.build_target.name)
    self.assertEqual(sysroot_path, out_proto.sysroot.path)


class InstallToolchainTest(cros_test_lib.MockTempDirTestCase,
                           api_config.ApiConfigMixin):
  """Install toolchain function tests."""

  def setUp(self):
    self.PatchObject(cros_build_lib, 'IsInsideChroot', return_value=True)
    # Avoid running the portageq command.
    self.PatchObject(sysroot_controller, '_LogBinhost')
    self.board = 'board'
    self.sysroot = os.path.join(self.tempdir, 'board')
    self.invalid_sysroot = os.path.join(self.tempdir, 'invalid', 'sysroot')
    osutils.SafeMakedirs(self.sysroot)

  def _InputProto(self, build_target=None, sysroot_path=None,
                  compile_source=False):
    """Helper to build an input proto instance."""
    proto = sysroot_pb2.InstallToolchainRequest()
    if build_target:
      proto.sysroot.build_target.name = build_target
    if sysroot_path:
      proto.sysroot.path = sysroot_path
    if compile_source:
      proto.flags.compile_source = compile_source

    return proto

  def _OutputProto(self):
    """Helper to build output proto instance."""
    return sysroot_pb2.InstallToolchainResponse()

  def testValidateOnly(self):
    """Sanity check that a validate only call does not execute any logic."""
    patch = self.PatchObject(sysroot_service, 'InstallToolchain')

    in_proto = self._InputProto(build_target=self.board,
                                sysroot_path=self.sysroot)
    sysroot_controller.InstallToolchain(in_proto, self._OutputProto(),
                                        self.validate_only_config)
    patch.assert_not_called()

  def testMockCall(self):
    """Sanity check that a mock call does not execute any logic."""
    patch = self.PatchObject(sysroot_service, 'InstallToolchain')
    request = self._InputProto()
    response = self._OutputProto()

    rc = sysroot_controller.InstallToolchain(request, response,
                                             self.mock_call_config)

    patch.assert_not_called()
    self.assertEqual(controller.RETURN_CODE_SUCCESS, rc)

  def testMockError(self):
    """Sanity check that a mock error does not execute any logic."""
    patch = self.PatchObject(sysroot_service, 'InstallToolchain')
    request = self._InputProto()
    response = self._OutputProto()

    rc = sysroot_controller.InstallToolchain(request, response,
                                             self.mock_error_config)

    patch.assert_not_called()
    self.assertEqual(controller.RETURN_CODE_UNSUCCESSFUL_RESPONSE_AVAILABLE, rc)
    self.assertTrue(response.failed_packages)

  def testArgumentValidation(self):
    """Test the argument validation."""
    # Test errors on missing inputs.
    out_proto = self._OutputProto()
    # Both missing.
    in_proto = self._InputProto()
    with self.assertRaises(cros_build_lib.DieSystemExit):
      sysroot_controller.InstallToolchain(in_proto, out_proto, self.api_config)

    # Sysroot path missing.
    in_proto = self._InputProto(build_target=self.board)
    with self.assertRaises(cros_build_lib.DieSystemExit):
      sysroot_controller.InstallToolchain(in_proto, out_proto, self.api_config)

    # Build target name missing.
    in_proto = self._InputProto(sysroot_path=self.sysroot)
    with self.assertRaises(cros_build_lib.DieSystemExit):
      sysroot_controller.InstallToolchain(in_proto, out_proto, self.api_config)

    # Both provided, but invalid sysroot path.
    in_proto = self._InputProto(build_target=self.board,
                                sysroot_path=self.invalid_sysroot)
    with self.assertRaises(cros_build_lib.DieSystemExit):
      sysroot_controller.InstallToolchain(in_proto, out_proto, self.api_config)

  def testSuccessOutputHandling(self):
    """Test the output is processed and recorded correctly."""
    self.PatchObject(sysroot_service, 'InstallToolchain')
    out_proto = self._OutputProto()
    in_proto = self._InputProto(build_target=self.board,
                                sysroot_path=self.sysroot)

    rc = sysroot_controller.InstallToolchain(in_proto, out_proto,
                                             self.api_config)
    self.assertFalse(rc)
    self.assertFalse(out_proto.failed_packages)


  def testErrorOutputHandling(self):
    """Test the error output is processed and recorded correctly."""
    out_proto = self._OutputProto()
    in_proto = self._InputProto(build_target=self.board,
                                sysroot_path=self.sysroot)

    err_pkgs = ['cat/pkg', 'cat2/pkg2']
    err_cpvs = [portage_util.SplitCPV(pkg, strict=False) for pkg in err_pkgs]
    expected = [('cat', 'pkg'), ('cat2', 'pkg2')]
    err = sysroot_lib.ToolchainInstallError('Error',
                                            cros_build_lib.CommandResult(),
                                            tc_info=err_cpvs)
    self.PatchObject(sysroot_service, 'InstallToolchain', side_effect=err)

    rc = sysroot_controller.InstallToolchain(in_proto, out_proto,
                                             self.api_config)
    self.assertEqual(controller.RETURN_CODE_UNSUCCESSFUL_RESPONSE_AVAILABLE, rc)
    self.assertTrue(out_proto.failed_packages)
    for package in out_proto.failed_packages:
      cat_pkg = (package.category, package.package_name)
      self.assertIn(cat_pkg, expected)


class InstallPackagesTest(cros_test_lib.MockTempDirTestCase,
                          api_config.ApiConfigMixin):
  """InstallPackages tests."""

  def setUp(self):
    self.PatchObject(cros_build_lib, 'IsInsideChroot', return_value=True)
    # Avoid running the portageq command.
    self.PatchObject(sysroot_controller, '_LogBinhost')
    self.build_target = 'board'
    self.sysroot = os.path.join(self.tempdir, 'build', 'board')
    osutils.SafeMakedirs(self.sysroot)

  def _InputProto(self, build_target=None, sysroot_path=None,
                  build_source=False):
    """Helper to build an input proto instance."""
    instance = sysroot_pb2.InstallPackagesRequest()

    if build_target:
      instance.sysroot.build_target.name = build_target
    if sysroot_path:
      instance.sysroot.path = sysroot_path
    if build_source:
      instance.flags.build_source = build_source

    return instance

  def _OutputProto(self):
    """Helper to build an empty output proto instance."""
    return sysroot_pb2.InstallPackagesResponse()

  def testValidateOnly(self):
    """Sanity check that a validate only call does not execute any logic."""
    patch = self.PatchObject(sysroot_service, 'BuildPackages')

    in_proto = self._InputProto(build_target=self.build_target,
                                sysroot_path=self.sysroot)
    sysroot_controller.InstallPackages(in_proto, self._OutputProto(),
                                       self.validate_only_config)
    patch.assert_not_called()

  def testMockCall(self):
    """Sanity check that a mock call does not execute any logic."""
    patch = self.PatchObject(sysroot_service, 'BuildPackages')
    request = self._InputProto()
    response = self._OutputProto()

    rc = sysroot_controller.InstallPackages(request, response,
                                            self.mock_call_config)

    patch.assert_not_called()
    self.assertEqual(controller.RETURN_CODE_SUCCESS, rc)

  def testMockError(self):
    """Sanity check that a mock error does not execute any logic."""
    patch = self.PatchObject(sysroot_service, 'BuildPackages')
    request = self._InputProto()
    response = self._OutputProto()

    rc = sysroot_controller.InstallPackages(request, response,
                                            self.mock_error_config)

    patch.assert_not_called()
    self.assertEqual(controller.RETURN_CODE_UNSUCCESSFUL_RESPONSE_AVAILABLE, rc)
    self.assertTrue(response.failed_packages)

  def testArgumentValidationAllMissing(self):
    """Test missing all arguments."""
    out_proto = self._OutputProto()
    in_proto = self._InputProto()
    with self.assertRaises(cros_build_lib.DieSystemExit):
      sysroot_controller.InstallPackages(in_proto, out_proto, self.api_config)

  def testArgumentValidationNoSysroot(self):
    """Test missing sysroot path."""
    out_proto = self._OutputProto()
    in_proto = self._InputProto(build_target=self.build_target)
    with self.assertRaises(cros_build_lib.DieSystemExit):
      sysroot_controller.InstallPackages(in_proto, out_proto, self.api_config)

  def testArgumentValidationNoBuildTarget(self):
    """Test missing build target name."""
    out_proto = self._OutputProto()
    in_proto = self._InputProto(sysroot_path=self.sysroot)
    with self.assertRaises(cros_build_lib.DieSystemExit):
      sysroot_controller.InstallPackages(in_proto, out_proto, self.api_config)

  def testArgumentValidationInvalidSysroot(self):
    """Test sysroot that hasn't had the toolchain installed."""
    out_proto = self._OutputProto()
    in_proto = self._InputProto(build_target=self.build_target,
                                sysroot_path=self.sysroot)
    self.PatchObject(sysroot_lib.Sysroot, 'IsToolchainInstalled',
                     return_value=False)
    with self.assertRaises(cros_build_lib.DieSystemExit):
      sysroot_controller.InstallPackages(in_proto, out_proto, self.api_config)

  def testSuccessOutputHandling(self):
    """Test successful call output handling."""
    # Prevent argument validation error.
    self.PatchObject(sysroot_lib.Sysroot, 'IsToolchainInstalled',
                     return_value=True)

    in_proto = self._InputProto(build_target=self.build_target,
                                sysroot_path=self.sysroot)
    out_proto = self._OutputProto()
    self.PatchObject(sysroot_service, 'BuildPackages')

    rc = sysroot_controller.InstallPackages(in_proto, out_proto,
                                            self.api_config)
    self.assertFalse(rc)
    self.assertFalse(out_proto.failed_packages)

  def testFailureOutputHandling(self):
    """Test failed package handling."""
    # Prevent argument validation error.
    self.PatchObject(sysroot_lib.Sysroot, 'IsToolchainInstalled',
                     return_value=True)

    in_proto = self._InputProto(build_target=self.build_target,
                                sysroot_path=self.sysroot)
    out_proto = self._OutputProto()

    # Failed package info and expected list for verification.
    err_pkgs = ['cat/pkg', 'cat2/pkg2']
    err_cpvs = [portage_util.SplitCPV(cpv, strict=False) for cpv in err_pkgs]
    expected = [('cat', 'pkg'), ('cat2', 'pkg2')]

    # Force error to be raised with the packages.
    error = sysroot_lib.PackageInstallError('Error',
                                            cros_build_lib.CommandResult(),
                                            packages=err_cpvs)
    self.PatchObject(sysroot_service, 'BuildPackages', side_effect=error)

    rc = sysroot_controller.InstallPackages(in_proto, out_proto,
                                            self.api_config)
    # This needs to return 2 to indicate the available error response.
    self.assertEqual(controller.RETURN_CODE_UNSUCCESSFUL_RESPONSE_AVAILABLE, rc)
    for package in out_proto.failed_packages:
      cat_pkg = (package.category, package.package_name)
      self.assertIn(cat_pkg, expected)

  def testNoPackageFailureOutputHandling(self):
    """Test failure handling without packages to report."""
    # Prevent argument validation error.
    self.PatchObject(sysroot_lib.Sysroot, 'IsToolchainInstalled',
                     return_value=True)

    in_proto = self._InputProto(build_target=self.build_target,
                                sysroot_path=self.sysroot)
    out_proto = self._OutputProto()

    # Force error to be raised with no packages.
    error = sysroot_lib.PackageInstallError('Error',
                                            cros_build_lib.CommandResult(),
                                            packages=[])
    self.PatchObject(sysroot_service, 'BuildPackages', side_effect=error)

    rc = sysroot_controller.InstallPackages(in_proto, out_proto,
                                            self.api_config)
    # All we really care about is it's not 0 or 2 (response available), so
    # test for that rather than a specific return code.
    self.assertTrue(rc)
    self.assertNotEqual(controller.RETURN_CODE_UNSUCCESSFUL_RESPONSE_AVAILABLE,
                        rc)
