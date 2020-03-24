# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Package related functionality."""

from __future__ import print_function

import sys

from chromite.api import faux
from chromite.api import validate
from chromite.api.controller import controller_util
from chromite.api.gen.chromite.api import binhost_pb2
from chromite.api.gen.chromiumos import common_pb2
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib.uprev_lib import GitRef
from chromite.service import packages


assert sys.version_info >= (3, 6), 'This module requires Python 3.6+'


_OVERLAY_TYPE_TO_NAME = {
    binhost_pb2.OVERLAYTYPE_PUBLIC: constants.PUBLIC_OVERLAYS,
    binhost_pb2.OVERLAYTYPE_PRIVATE: constants.PRIVATE_OVERLAYS,
    binhost_pb2.OVERLAYTYPE_BOTH: constants.BOTH_OVERLAYS,
}

def _UprevResponse(_input_proto, output_proto, _config):
  """Add fake paths to a successful uprev response."""
  output_proto.modified_ebuilds.add().path = '/fake/path1'
  output_proto.modified_ebuilds.add().path = '/fake/path2'

@faux.success(_UprevResponse)
@faux.empty_error
@validate.require('overlay_type')
@validate.is_in('overlay_type', _OVERLAY_TYPE_TO_NAME)
@validate.validation_complete
def Uprev(input_proto, output_proto, _config):
  """Uprev all cros workon ebuilds that have changes."""
  build_targets = controller_util.ParseBuildTargets(input_proto.build_targets)
  overlay_type = _OVERLAY_TYPE_TO_NAME[input_proto.overlay_type]
  chroot = controller_util.ParseChroot(input_proto.chroot)
  output_dir = input_proto.output_dir or None

  try:
    uprevved = packages.uprev_build_targets(build_targets, overlay_type, chroot,
                                            output_dir)
  except packages.Error as e:
    # Handle module errors nicely, let everything else bubble up.
    cros_build_lib.Die(e)

  for path in uprevved:
    output_proto.modified_ebuilds.add().path = path


def _UprevVersionedPackageResponse(_input_proto, output_proto, _config):
  """Add fake paths to a successful uprev versioned package response."""
  uprev_response = output_proto.responses.add()
  uprev_response.modified_ebuilds.add().path = '/uprev/response/path'


@faux.success(_UprevVersionedPackageResponse)
@faux.empty_error
@validate.require('versions')
@validate.require('package_info.package_name', 'package_info.category')
@validate.validation_complete
def UprevVersionedPackage(input_proto, output_proto, _config):
  """Uprev a versioned package.

  See go/pupr-generator for details about this endpoint.
  """
  chroot = controller_util.ParseChroot(input_proto.chroot)
  build_targets = controller_util.ParseBuildTargets(input_proto.build_targets)
  package = controller_util.PackageInfoToCPV(input_proto.package_info)
  refs = []
  for ref in input_proto.versions:
    refs.append(GitRef(path=ref.repository, ref=ref.ref, revision=ref.revision))

  try:
    result = packages.uprev_versioned_package(package, build_targets, refs,
                                              chroot)
  except packages.Error as e:
    # Handle module errors nicely, let everything else bubble up.
    cros_build_lib.Die(e)

  if not result.uprevved:
    # No uprevs executed, skip the output population.
    return

  for modified in result.modified:
    uprev_response = output_proto.responses.add()
    uprev_response.version = modified.new_version
    for path in modified.files:
      uprev_response.modified_ebuilds.add().path = path


def _GetBestVisibleResponse(_input_proto, output_proto, _config):
  """Add fake paths to a successful GetBestVisible response."""
  package_info = common_pb2.PackageInfo(
      category='category',
      package_name='name',
      version='1.01',
  )
  output_proto.package_info.CopyFrom(package_info)


@faux.success(_GetBestVisibleResponse)
@faux.empty_error
@validate.require('atom')
@validate.validation_complete
def GetBestVisible(input_proto, output_proto, _config):
  """Returns the best visible PackageInfo for the indicated atom."""
  build_target = None
  if input_proto.build_target.name:
    build_target = controller_util.ParseBuildTarget(input_proto.build_target)

  cpv = packages.get_best_visible(input_proto.atom, build_target=build_target)
  package_info = common_pb2.PackageInfo()
  controller_util.CPVToPackageInfo(cpv, package_info)
  output_proto.package_info.CopyFrom(package_info)


def _ChromeVersionResponse(_input_proto, output_proto, _config):
  """Add a fake chrome version to a successful response."""
  output_proto.version = '78.0.3900.0'


@faux.success(_ChromeVersionResponse)
@faux.empty_error
@validate.require('build_target.name')
@validate.validation_complete
def GetChromeVersion(input_proto, output_proto, _config):
  """Returns the chrome version."""
  build_target = controller_util.ParseBuildTarget(input_proto.build_target)
  chrome_version = packages.determine_chrome_version(build_target)
  if chrome_version:
    output_proto.version = chrome_version


def _GetTargetVersionsResponse(_input_proto, output_proto, _config):
  """Add fake target version fields to a successful response."""
  output_proto.android_version = '5812377'
  output_proto.android_branch_version = 'git_nyc-mr1-arc'
  output_proto.android_target_version = 'cheets'
  output_proto.chrome_version = '78.0.3900.0'
  output_proto.platform_version = '12438.0.0'
  output_proto.milestone_version = '78'
  output_proto.full_version = 'R78-12438.0.0'


@faux.success(_GetTargetVersionsResponse)
@faux.empty_error
@validate.require('build_target.name')
@validate.validation_complete
def GetTargetVersions(input_proto, output_proto, _config):
  """Returns the target versions."""
  build_target = controller_util.ParseBuildTarget(input_proto.build_target)
  # Android version.
  android_version = packages.determine_android_version([build_target])
  logging.info('Found android version: %s', android_version)
  if android_version:
    output_proto.android_version = android_version
  # Android branch version.
  android_branch_version = packages.determine_android_branch(build_target)
  logging.info('Found android branch version: %s', android_branch_version)
  if android_branch_version:
    output_proto.android_branch_version = android_branch_version
  # Android target version.
  android_target_version = packages.determine_android_target(build_target)
  logging.info('Found android target version: %s', android_target_version)
  if android_target_version:
    output_proto.android_target_version = android_target_version

  # TODO(crbug/1019770): Investigate cases where builds_chrome is true but
  # chrome_version is None.
  builds_chrome = packages.builds(constants.CHROME_CP, build_target)
  if builds_chrome:
    # Chrome version fetch.
    chrome_version = packages.determine_chrome_version(build_target)
    logging.info('Found chrome version: %s', chrome_version)
    if chrome_version:
      output_proto.chrome_version = chrome_version

  # The ChromeOS version info.
  output_proto.platform_version = packages.determine_platform_version()
  output_proto.milestone_version = packages.determine_milestone_version()
  output_proto.full_version = packages.determine_full_version()


def _HasPrebuiltSuccess(_input_proto, output_proto, _config):
  """The mock success case for HasChromePrebuilt."""
  output_proto.has_prebuilt = True


@faux.success(_HasPrebuiltSuccess)
@faux.empty_error
@validate.require('build_target.name')
@validate.validation_complete
def HasChromePrebuilt(input_proto, output_proto, _config):
  """Checks if the most recent version of Chrome has a prebuilt."""
  build_target = controller_util.ParseBuildTarget(input_proto.build_target)
  useflags = 'chrome_internal' if input_proto.chrome else None
  exists = packages.has_prebuilt(constants.CHROME_CP, build_target=build_target,
                                 useflags=useflags)

  output_proto.has_prebuilt = exists


@faux.success(_HasPrebuiltSuccess)
@faux.empty_error
@validate.require('build_target.name', 'package_info.category',
                  'package_info.package_name')
@validate.validation_complete
def HasPrebuilt(input_proto, output_proto, _config):
  """Checks if the most recent version of Chrome has a prebuilt."""
  build_target = controller_util.ParseBuildTarget(input_proto.build_target)
  package = controller_util.PackageInfoToCPV(input_proto.package_info).cp
  useflags = 'chrome_internal' if input_proto.chrome else None
  exists = packages.has_prebuilt(
      package, build_target=build_target, useflags=useflags)

  output_proto.has_prebuilt = exists


def _BuildsChromeSuccess(_input_proto, output_proto, _config):
  """Mock success case for BuildsChrome."""
  output_proto.builds_chrome = True


@faux.success(_BuildsChromeSuccess)
@faux.empty_error
@validate.require('build_target.name')
@validate.validation_complete
def BuildsChrome(input_proto, output_proto, _config):
  """Check if the board builds chrome."""
  build_target = controller_util.ParseBuildTarget(input_proto.build_target)
  cpvs = [controller_util.PackageInfoToCPV(pi) for pi in input_proto.packages]
  builds_chrome = packages.builds(constants.CHROME_CP, build_target, cpvs)
  output_proto.builds_chrome = builds_chrome
