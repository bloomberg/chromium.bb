# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Sysroot controller."""

from __future__ import print_function

from chromite.lib import build_target_util
from chromite.lib import cros_build_lib
from chromite.lib import portage_util
from chromite.lib import sysroot_lib
from chromite.service import sysroot

_ACCEPTED_LICENSES = '@CHROMEOS'

# Return codes.
RC_ERROR = 1


def Create(input_proto, output_proto):
  """Create or replace a sysroot."""
  update_chroot = not input_proto.flags.chroot_current
  replace_sysroot = input_proto.flags.replace

  build_target_name = input_proto.build_target.name
  profile = input_proto.profile.name or None

  if not build_target_name:
    cros_build_lib.Die('The build target must be provided.')

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


def InstallToolchain(input_proto, output_proto):
  compile_source = input_proto.flags.compile_source

  sysroot_path = input_proto.sysroot.path
  build_target_name = input_proto.sysroot.build_target.name

  if not sysroot_path:
    cros_build_lib.Die('sysroot.path is required.')
  if not build_target_name:
    cros_build_lib.Die('sysroot.build_target.name is required.')

  build_target = build_target_util.BuildTarget(name=build_target_name)
  target_sysroot = sysroot_lib.Sysroot(sysroot_path)
  run_configs = sysroot.SetupBoardRunConfig(usepkg=not compile_source)

  if not target_sysroot.Exists():
    cros_build_lib.Die('Sysroot has not been created. Run Create first.')

  try:
    sysroot.InstallToolchain(build_target, target_sysroot, run_configs)
  except sysroot_lib.ToolchainInstallError as e:
    # Error installing - populate the failed package info.
    for package in e.failed_toolchain_info:
      package_info = output_proto.failed_packages.add()
      _CPVToPackageInfo(package, package_info)

    return RC_ERROR


def InstallPackages(input_proto, output_proto):
  compile_source = input_proto.flags.compile_source
  event_file = input_proto.flags.event_file

  sysroot_path = input_proto.sysroot.path
  build_target_name = input_proto.sysroot.build_target.name
  packages = map(_PackageInfoToCPV, input_proto.packages)

  if not build_target_name:
    cros_build_lib.Die('Build target name is required.')
  if not sysroot_path:
    cros_build_lib.Die('Sysroot path is required')

  build_target = build_target_util.BuildTarget(build_target_name)
  target_sysroot = sysroot_lib.Sysroot(sysroot_path)

  if not target_sysroot.IsToolchainInstalled():
    cros_build_lib.Die('Toolchain must first be installed.')

  build_packages_config = sysroot.BuildPackagesRunConfig(
      event_file=event_file, usepkg=not compile_source,
      install_debug_symbols=True, packages=packages)

  try:
    sysroot.BuildPackages(build_target, target_sysroot, build_packages_config)
  except sysroot_lib.PackageInstallError as e:
    for package in e.failed_packages:
      package_info = output_proto.failed_packages.add()
      _CPVToPackageInfo(package, package_info)

    return RC_ERROR


def _CPVToPackageInfo(cpv, package_info):
  """Helper to translate CPVs into a PackageInfo message."""
  package_info.package_name = cpv.package
  if cpv.category:
    package_info.category = cpv.category
  if cpv.version:
    package_info.version = cpv.version


def _PackageInfoToCPV(package_info):
  """Helper to translate a PackageInfo message into a CPV."""
  if not package_info or not package_info.package_name:
    return None

  # Combine the components into the full CPV string that SplitCPV parses.
  # TODO: Turn portage_util.CPV into a class that can handle building out an
  #  instance from components.
  c = ('%s/' % package_info.category) if package_info.category else ''
  p = package_info.package_name
  v = ('-%s' % package_info.version) if package_info.version else ''
  cpv = '%s%s%s' % (c, p, v)

  return portage_util.SplitCPV(cpv)
