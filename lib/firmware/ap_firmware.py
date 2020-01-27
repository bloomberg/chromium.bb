# -*- coding: utf-8 -*-
# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""AP firmware utilities."""

from __future__ import print_function

import collections
import importlib

from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import workon_helper

_BUILD_TARGET_CONFIG_MODULE = 'chromite.lib.firmware.ap_firmware_config.%s'
_CONFIG_BUILD_WORKON_PACKAGES = 'BUILD_WORKON_PACKAGES'
_CONFIG_BUILD_PACKAGES = 'BUILD_PACKAGES'

# The build configs. The workon and build fields both contain tuples of
# packages.
BuildConfig = collections.namedtuple('BuildConfig', ('workon', 'build'))


class Error(Exception):
  """Base error class for the module."""


class BuildError(Error):
  """Failure in the build command."""


class BuildTargetNotConfiguredError(Error):
  """Thrown when a config module does not exist for the build target."""


class InvalidConfigError(Error):
  """The config does not contain the required information for the operation."""


def build(build_target):
  """Build the AP Firmware.

  Args:
    build_target (BuildTarget): The build target (board) being built.
  """
  workon = workon_helper.WorkonHelper(build_target.root, build_target.name)
  config = _get_build_config(build_target)

  # Workon the required packages. Also calculate the list of packages that need
  # to be stopped to clean up after ourselves when we're done.
  if config.workon:
    before_workon = workon.ListAtoms()
    logging.info('cros_workon-%s start %s', build_target.name,
                 ' '.join(config.workon))
    workon.StartWorkingOnPackages(config.workon)
    after_workon = workon.ListAtoms()
    stop_packages = list(set(after_workon) - set(before_workon))
  else:
    stop_packages = []

  # Run the emerge command to build the packages. Don't raise an exception here
  # if it fails so we can cros workon stop afterwords.
  result = cros_build_lib.run(
      [build_target.get_command('emerge')] + list(config.build), check=False)

  # Reset the environment.
  if stop_packages:
    # Stop the packages we started.
    logging.info('cros_workon-%s stop %s', build_target.name,
                 ' '.join(stop_packages))
    workon.StopWorkingOnPackages(stop_packages)
  else:
    logging.info('No packages needed to be stopped.')

  if result.returncode:
    # Now raise the emerge failure since we're done cleaning up.
    raise BuildError(
        'The emerge command failed. See the logs above for details.')


def _get_build_config(build_target):
  """Get the relevant build config for |build_target|."""
  module = _get_config_module(build_target)
  workon_pkgs = getattr(module, _CONFIG_BUILD_WORKON_PACKAGES, None)
  build_pkgs = getattr(module, _CONFIG_BUILD_PACKAGES, None)

  if not build_pkgs:
    raise InvalidConfigError(
        'No packages specified to build in the configs for %s.' %
        build_target.name)

  return BuildConfig(workon=workon_pkgs, build=build_pkgs)


def _get_config_module(build_target):
  """Get the |build_target|'s config module."""
  name = _BUILD_TARGET_CONFIG_MODULE % build_target.name
  try:
    return importlib.import_module(name)
  except ImportError:
    raise BuildTargetNotConfiguredError(
        'Could not find a config module for %s.' % build_target.name)
