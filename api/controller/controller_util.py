# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility functions that are useful for controllers."""

from __future__ import print_function

from chromite.lib import chroot_lib
from chromite.lib import portage_util


def ParseChroot(chroot_message):
  """Create a chroot object from the chroot message."""
  path = chroot_message.path
  cache_dir = chroot_message.cache_dir
  chrome_root = chroot_message.chrome_dir

  use_flags = [u.flag for u in chroot_message.env.use_flags]
  features = [f.feature for f in chroot_message.env.features]

  env = {}
  if use_flags:
    env['USE'] = ' '.join(use_flags)

  # TODO(saklein) Remove the default when fully integrated in recipes.
  env['FEATURES'] = 'separatedebug'
  if features:
    env['FEATURES'] = ' '.join(features)

  return chroot_lib.Chroot(path=path, cache_dir=cache_dir,
                           chrome_root=chrome_root, env=env)


def CPVToPackageInfo(cpv, package_info):
  """Helper to translate CPVs into a PackageInfo message."""
  package_info.package_name = cpv.package
  if cpv.category:
    package_info.category = cpv.category
  if cpv.version:
    package_info.version = cpv.version


def PackageInfoToCPV(package_info):
  """Helper to translate a PackageInfo message into a CPV."""
  if not package_info or not package_info.package_name:
    return None

  return portage_util.SplitCPV(PackageInfoToString(package_info), strict=False)


def PackageInfoToString(package_info):
  # Combine the components into the full CPV string that SplitCPV parses.
  # TODO: Turn portage_util.CPV into a class that can handle building out an
  #  instance from components.
  if not package_info.package_name:
    raise ValueError('Invalid package_info.')

  c = ('%s/' % package_info.category) if package_info.category else ''
  p = package_info.package_name
  v = ('-%s' % package_info.version) if package_info.version else ''
  return '%s%s%s' % (c, p, v)


def CPVToString(cpv):
  """Get the most useful string representation from a CPV.

  Args:
    cpv (portage_util.CPV): The CPV object.

  Returns:
    str

  Raises:
    ValueError - when the CPV has no useful fields set.
  """
  if cpv.cpf:
    return cpv.cpf
  elif cpv.cpv:
    return cpv.cpv
  elif cpv.cp:
    return cpv.cp
  elif cpv.package:
    return cpv.package
  else:
    raise ValueError('Invalid CPV provided.')
