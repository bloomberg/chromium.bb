# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Sysroot controller."""

from __future__ import print_function

from chromite.api import controller
from chromite.api import faux
from chromite.api import validate
from chromite.api.controller import controller_util
from chromite.api.metrics import deserialize_metrics_log
from chromite.lib import build_target_util
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import portage_util
from chromite.lib import sysroot_lib
from chromite.service import sysroot
from chromite.utils import metrics

_ACCEPTED_LICENSES = '@CHROMEOS'


@faux.all_empty
@validate.require('build_target.name')
@validate.validation_complete
def Create(input_proto, output_proto, _config):
  """Create or replace a sysroot."""
  update_chroot = not input_proto.flags.chroot_current
  replace_sysroot = input_proto.flags.replace

  build_target_name = input_proto.build_target.name
  profile = input_proto.profile.name or None

  build_target = build_target_util.BuildTarget(name=build_target_name,
                                               profile=profile)
  run_configs = sysroot.SetupBoardRunConfig(
      force=replace_sysroot, upgrade_chroot=update_chroot)

  try:
    created = sysroot.Create(build_target, run_configs,
                             accept_licenses=_ACCEPTED_LICENSES)
  except sysroot.Error as e:
    cros_build_lib.Die(e.message)

  output_proto.sysroot.path = created.path
  output_proto.sysroot.build_target.name = build_target_name

  return controller.RETURN_CODE_SUCCESS


def _MockFailedPackagesResponse(_input_proto, output_proto, _config):
  """Mock error response that populates failed packages."""
  pkg = output_proto.failed_packages.add()
  pkg.package_name = 'package'
  pkg.category = 'category'
  pkg.version = '1.0.0_rc-r1'

  pkg2 = output_proto.failed_packages.add()
  pkg2.package_name = 'bar'
  pkg2.category = 'foo'
  pkg2.version = '3.7-r99'


@faux.empty_success
@faux.error(_MockFailedPackagesResponse)
@validate.require('sysroot.path', 'sysroot.build_target.name')
@validate.exists('sysroot.path')
@validate.validation_complete
def InstallToolchain(input_proto, output_proto, _config):
  """Install the toolchain into a sysroot."""
  compile_source = input_proto.flags.compile_source

  sysroot_path = input_proto.sysroot.path
  build_target_name = input_proto.sysroot.build_target.name

  build_target = build_target_util.BuildTarget(name=build_target_name)
  target_sysroot = sysroot_lib.Sysroot(sysroot_path)
  run_configs = sysroot.SetupBoardRunConfig(usepkg=not compile_source)

  _LogBinhost(build_target.name)

  try:
    sysroot.InstallToolchain(build_target, target_sysroot, run_configs)
  except sysroot_lib.ToolchainInstallError as e:
    # Error installing - populate the failed package info.
    for package in e.failed_toolchain_info:
      package_info = output_proto.failed_packages.add()
      controller_util.CPVToPackageInfo(package, package_info)

    return controller.RETURN_CODE_UNSUCCESSFUL_RESPONSE_AVAILABLE

  return controller.RETURN_CODE_SUCCESS


@faux.empty_success
@faux.error(_MockFailedPackagesResponse)
@validate.require('sysroot.path', 'sysroot.build_target.name')
@validate.validation_complete
@metrics.collect_metrics
def InstallPackages(input_proto, output_proto, _config):
  """Install packages into a sysroot, building as necessary and permitted."""
  compile_source = input_proto.flags.compile_source
  event_file = input_proto.flags.event_file

  sysroot_path = input_proto.sysroot.path
  build_target_name = input_proto.sysroot.build_target.name
  packages = [controller_util.PackageInfoToString(x)
              for x in input_proto.packages]

  build_target = build_target_util.BuildTarget(build_target_name)
  target_sysroot = sysroot_lib.Sysroot(sysroot_path)

  if not target_sysroot.IsToolchainInstalled():
    cros_build_lib.Die('Toolchain must first be installed.')

  _LogBinhost(build_target.name)

  use_flags = [u.flag for u in input_proto.use_flags]
  build_packages_config = sysroot.BuildPackagesRunConfig(
      event_file=event_file, usepkg=not compile_source,
      install_debug_symbols=True, packages=packages, use_flags=use_flags)

  try:
    sysroot.BuildPackages(build_target, target_sysroot, build_packages_config)
  except sysroot_lib.PackageInstallError as e:
    if not e.failed_packages:
      # No packages to report, so just exit with an error code.
      return controller.RETURN_CODE_COMPLETED_UNSUCCESSFULLY

    # We need to report the failed packages.
    for package in e.failed_packages:
      package_info = output_proto.failed_packages.add()
      controller_util.CPVToPackageInfo(package, package_info)

    return controller.RETURN_CODE_UNSUCCESSFUL_RESPONSE_AVAILABLE

  # Read metric events log and pipe them into output_proto.events.
  deserialize_metrics_log(output_proto.events, prefix=build_target_name)


def _LogBinhost(board):
  """Log the portage binhost for the given board."""
  binhost = portage_util.PortageqEnvvar('PORTAGE_BINHOST', board=board,
                                        allow_undefined=True)
  if not binhost:
    logging.warning('Portage Binhost not found.')
  else:
    logging.info('Portage Binhost: %s', binhost)
