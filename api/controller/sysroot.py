# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Sysroot controller."""

from __future__ import print_function

from chromite.lib import build_target_util
from chromite.lib import cros_build_lib
from chromite.lib import sysroot_lib
from chromite.service import sysroot

_ACCEPTED_LICENSES = '@CHROMEOS'

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
  run_configs = sysroot.SetupBoardRunConfig(force=replace_sysroot,
                                            upgrade_chroot=update_chroot)

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
      package_info.category = package.category
      package_info.package_name = package.package
      if package.version:
        package_info.version = package.version

    return 1
