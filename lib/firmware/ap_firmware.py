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
from chromite.lib.firmware import flash_ap

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


class DeployError(Error):
  """Failure in the deploy command."""


class InvalidConfigError(Error):
  """The config does not contain the required information for the operation."""


def build(build_target, fw_name=None):
  """Build the AP Firmware.

  Args:
    build_target (BuildTarget): The build target (board) being built.
    fw_name (str|None): Optionally set the FW_NAME envvar to allow building
      the firmware for only a specific variant.
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

  extra_env = {'FW_NAME': fw_name} if fw_name else None
  # Run the emerge command to build the packages. Don't raise an exception here
  # if it fails so we can cros workon stop afterwords.
  result = cros_build_lib.run(
      [build_target.get_command('emerge')] + list(config.build),
      check=False,
      extra_env=extra_env)

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


def deploy(build_target,
           image,
           device,
           flashrom=False,
           fast=False,
           verbose=False,
           dryrun=False):
  """Deploy a firmware image to a device.

  Args:
    build_target (build_target_util.BuildTarget): The build target.
    image (str): The path to the image to flash.
    device (commandline.Device): The DUT being flashed.
    flashrom (bool): Whether to use flashrom or futility.
    fast (bool): Perform a faster flash that isn't validated.
    verbose (bool): Whether to enable verbose output of the flash commands.
    dryrun (bool): Whether to actually execute the deployment or just print the
      operations that would have been performed.
  """
  try:
    flash_ap.deploy(
        build_target=build_target,
        image=image,
        device=device,
        flashrom=flashrom,
        fast=fast,
        verbose=verbose,
        dryrun=dryrun)
  except flash_ap.Error as e:
    # Reraise as a DeployError for future compatibility.
    raise DeployError(str(e))


def _get_build_config(build_target):
  """Get the relevant build config for |build_target|."""
  module = _get_config_module(build_target)
  workon_pkgs = getattr(module, _CONFIG_BUILD_WORKON_PACKAGES, None)
  build_pkgs = getattr(module, _CONFIG_BUILD_PACKAGES, None)

  if not build_pkgs:
    raise InvalidConfigError(
        'No packages specified to build in the configs for %s. Check the '
        'configs in chromite/lib/firmware/ap_firmware_config/%s.py.' %
        (build_target.name, build_target.name))

  return BuildConfig(workon=workon_pkgs, build=build_pkgs)


def _get_config_module(build_target):
  """Get the |build_target|'s config module."""
  name = _BUILD_TARGET_CONFIG_MODULE % build_target.name
  try:
    return importlib.import_module(name)
  except ImportError:
    raise BuildTargetNotConfiguredError(
        'Could not find a config module for %s.' % build_target.name)
