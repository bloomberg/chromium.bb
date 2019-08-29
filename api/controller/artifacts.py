# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Implements ArtifactService."""

from __future__ import print_function

import os

from chromite.api import controller
from chromite.api import faux
from chromite.api import validate
from chromite.api.controller import controller_util
from chromite.api.gen.chromite.api import toolchain_pb2
from chromite.cbuildbot import commands
from chromite.lib import build_target_util
from chromite.lib import chroot_lib
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import sysroot_lib
from chromite.service import artifacts


def _GetImageDir(build_root, target):
  """Return path containing images for the given build target.

  TODO(saklein) Expand image_lib.GetLatestImageLink to support this use case.

  Args:
    build_root (str): Path to checkout where build occurs.
    target (str): Name of the build target.

  Returns:
    Path to the directory containing target images.

  Raises:
    DieSystemExit: If the image dir does not exist.
  """
  image_dir = os.path.join(build_root, 'src/build/images', target, 'latest')
  if not os.path.exists(image_dir):
    cros_build_lib.Die('Expected to find image output for target %s at %s, '
                       'but path does not exist' % (target, image_dir))
  return image_dir


@faux.all_empty
@validate.require('build_target.name', 'output_dir')
@validate.exists('output_dir')
@validate.validation_complete
def BundleImageZip(input_proto, output_proto, _config):
  """Bundle image.zip.

  Args:
    input_proto (BundleRequest): The input proto.
    output_proto (BundleResponse): The output proto.
    _config (api_config.ApiConfig): The API call config.
  """
  target = input_proto.build_target.name
  output_dir = input_proto.output_dir
  image_dir = _GetImageDir(constants.SOURCE_ROOT, target)

  archive = artifacts.BundleImageZip(output_dir, image_dir)
  output_proto.artifacts.add().path = os.path.join(output_dir, archive)


@faux.all_empty
@validate.require('build_target.name', 'output_dir')
@validate.exists('output_dir')
@validate.validation_complete
def BundleTestUpdatePayloads(input_proto, output_proto, _config):
  """Generate minimal update payloads for the build target for testing.

  Args:
    input_proto (BundleRequest): The input proto.
    output_proto (BundleResponse): The output proto.
    _config (api_config.ApiConfig): The API call config.
  """
  target = input_proto.build_target.name
  output_dir = input_proto.output_dir
  build_root = constants.SOURCE_ROOT

  # Use the first available image to create the update payload.
  img_dir = _GetImageDir(build_root, target)
  img_types = [constants.IMAGE_TYPE_TEST, constants.IMAGE_TYPE_DEV,
               constants.IMAGE_TYPE_BASE]
  img_names = [constants.IMAGE_TYPE_TO_NAME[t] for t in img_types]
  img_paths = [os.path.join(img_dir, x) for x in img_names]
  valid_images = [x for x in img_paths if os.path.exists(x)]

  if not valid_images:
    cros_build_lib.Die(
        'Expected to find an image of type among %r for target "%s" '
        'at path %s.', img_types, target, img_dir)
  image = valid_images[0]

  payloads = artifacts.BundleTestUpdatePayloads(image, output_dir)
  for payload in payloads:
    output_proto.artifacts.add().path = payload


@faux.all_empty
@validate.require('output_dir')
@validate.exists('output_dir')
def BundleAutotestFiles(input_proto, output_proto, config):
  """Tar the autotest files for a build target.

  Args:
    input_proto (BundleRequest): The input proto.
    output_proto (BundleResponse): The output proto.
    config (api_config.ApiConfig): The API call config.
  """
  output_dir = input_proto.output_dir
  target = input_proto.build_target.name
  chroot = controller_util.ParseChroot(input_proto.chroot)

  if target:
    sysroot_path = os.path.join('/build', target)
  else:
    # New style call, use chroot and sysroot.
    sysroot_path = input_proto.sysroot.path
    if not sysroot_path:
      cros_build_lib.Die('sysroot.path is required.')

  sysroot = sysroot_lib.Sysroot(sysroot_path)

  # TODO(saklein): Switch to the validate_only decorator when legacy handling
  #   is removed.
  if config.validate_only:
    return controller.RETURN_CODE_VALID_INPUT

  if not sysroot.Exists(chroot=chroot):
    cros_build_lib.Die('Sysroot path must exist: %s', sysroot.path)

  try:
    # Note that this returns the full path to *multiple* tarballs.
    archives = artifacts.BundleAutotestFiles(chroot, sysroot, output_dir)
  except artifacts.Error as e:
    cros_build_lib.Die(e.message)

  for archive in archives.values():
    output_proto.artifacts.add().path = archive


@faux.all_empty
@validate.require('output_dir')
@validate.exists('output_dir')
def BundleTastFiles(input_proto, output_proto, config):
  """Tar the tast files for a build target.

  Args:
    input_proto (BundleRequest): The input proto.
    output_proto (BundleResponse): The output proto.
    config (api_config.ApiConfig): The API call config.
  """
  target = input_proto.build_target.name
  output_dir = input_proto.output_dir
  build_root = constants.SOURCE_ROOT

  chroot = controller_util.ParseChroot(input_proto.chroot)
  sysroot_path = input_proto.sysroot.path

  # TODO(saklein) Cleanup legacy handling after it has been switched over.
  if target:
    # Legacy handling.
    chroot = chroot_lib.Chroot(path=os.path.join(build_root, 'chroot'))
    sysroot_path = os.path.join('/build', target)

  # New handling - chroot & sysroot based.
  # TODO(saklein) Switch this to the require decorator when legacy is removed.
  if not sysroot_path:
    cros_build_lib.Die('sysroot.path is required.')

  # TODO(saklein): Switch to the validation_complete decorator when legacy
  #   handling is removed.
  if config.validate_only:
    return controller.RETURN_CODE_VALID_INPUT

  sysroot = sysroot_lib.Sysroot(sysroot_path)
  if not sysroot.Exists(chroot=chroot):
    cros_build_lib.Die('Sysroot must exist.')

  archive = artifacts.BundleTastFiles(chroot, sysroot, output_dir)

  if archive is None:
    cros_build_lib.Die(
        'Could not bundle Tast files. '
        'No Tast directories found for %s.', target)

  output_proto.artifacts.add().path = archive


@faux.all_empty
@validate.require('build_target.name', 'output_dir')
@validate.exists('output_dir')
@validate.validation_complete
def BundlePinnedGuestImages(input_proto, output_proto, _config):
  """Tar the pinned guest images for a build target.

  Args:
    input_proto (BundleRequest): The input proto.
    output_proto (BundleResponse): The output proto.
    _config (api_config.ApiConfig): The API call config.
  """
  target = input_proto.build_target.name
  output_dir = input_proto.output_dir
  build_root = constants.SOURCE_ROOT

  # TODO(crbug.com/954299): Replace with a chromite/service implementation.
  archive = commands.BuildPinnedGuestImagesTarball(build_root, target,
                                                   output_dir)

  if archive is None:
    logging.warning('Found no pinned guest images for %s.', target)
    return

  output_proto.artifacts.add().path = os.path.join(output_dir, archive)


@faux.all_empty
@validate.require('sysroot.path')
@validate.validation_complete
def FetchPinnedGuestImages(input_proto, output_proto, _config):
  """Get the pinned guest image information."""
  sysroot_path = input_proto.sysroot.path

  chroot = controller_util.ParseChroot(input_proto.chroot)
  sysroot = sysroot_lib.Sysroot(sysroot_path)

  if not chroot.exists():
    cros_build_lib.Die('Chroot does not exist: %s', chroot.path)
  elif not sysroot.Exists(chroot=chroot):
    cros_build_lib.Die('Sysroot does not exist: %s',
                       chroot.full_path(sysroot.path))

  pins = artifacts.FetchPinnedGuestImages(chroot, sysroot)

  for pin in pins:
    pinned_image = output_proto.pinned_images.add()
    pinned_image.filename = pin.filename
    pinned_image.uri = pin.uri


@faux.all_empty
@validate.require('output_dir', 'sysroot.path')
@validate.exists('output_dir')
@validate.validation_complete
def BundleFirmware(input_proto, output_proto, _config):
  """Tar the firmware images for a build target.

  Args:
    input_proto (BundleRequest): The input proto.
    output_proto (BundleResponse): The output proto.
    _config (api_config.ApiConfig): The API call config.
  """
  output_dir = input_proto.output_dir
  chroot = controller_util.ParseChroot(input_proto.chroot)
  sysroot_path = input_proto.sysroot.path
  sysroot = sysroot_lib.Sysroot(sysroot_path)

  if not chroot.exists():
    cros_build_lib.Die('Chroot does not exist: %s', chroot.path)
  elif not sysroot.Exists(chroot=chroot):
    cros_build_lib.Die('Sysroot does not exist: %s',
                       chroot.full_path(sysroot.path))

  archive = artifacts.BuildFirmwareArchive(chroot, sysroot, output_dir)

  if archive is None:
    cros_build_lib.Die(
        'Could not create firmware archive. No firmware found for %s.',
        sysroot_path)

  output_proto.artifacts.add().path = archive


@faux.all_empty
@validate.exists('output_dir')
def BundleEbuildLogs(input_proto, output_proto, config):
  """Tar the ebuild logs for a build target.

  Args:
    input_proto (BundleRequest): The input proto.
    output_proto (BundleResponse): The output proto.
    config (api_config.ApiConfig): The API call config.
  """
  output_dir = input_proto.output_dir
  sysroot_path = input_proto.sysroot.path
  chroot = controller_util.ParseChroot(input_proto.chroot)

  # TODO(mmortensen) Cleanup legacy handling after it has been switched over.
  target = input_proto.build_target.name
  if target:
    # Legacy handling.
    build_root = constants.SOURCE_ROOT
    chroot = chroot_lib.Chroot(path=os.path.join(build_root, 'chroot'))
    sysroot_path = os.path.join('/build', target)

  # TODO(saklein): Switch to validation_complete decorator after legacy
  #   handling has been cleaned up.
  if config.validate_only:
    return controller.RETURN_CODE_VALID_INPUT

  sysroot = sysroot_lib.Sysroot(sysroot_path)
  archive = artifacts.BundleEBuildLogsTarball(chroot, sysroot, output_dir)
  if archive is None:
    cros_build_lib.Die(
        'Could not create ebuild logs archive. No logs found for %s.',
        sysroot.path)
  output_proto.artifacts.add().path = os.path.join(output_dir, archive)


@faux.all_empty
@validate.exists('output_dir')
@validate.validation_complete
def BundleChromeOSConfig(input_proto, output_proto, _config):
  """Output the ChromeOS Config payload for a build target.

  Args:
    input_proto (BundleRequest): The input proto.
    output_proto (BundleResponse): The output proto.
    _config (api_config.ApiConfig): The API call config.
  """
  output_dir = input_proto.output_dir
  sysroot_path = input_proto.sysroot.path
  chroot = controller_util.ParseChroot(input_proto.chroot)

  # TODO(mmortensen) Cleanup legacy handling after it has been switched over.
  target = input_proto.build_target.name
  if target:
    # Legacy handling.
    build_root = constants.SOURCE_ROOT
    chroot = chroot_lib.Chroot(path=os.path.join(build_root, 'chroot'))
    sysroot_path = os.path.join('/build', target)

  sysroot = sysroot_lib.Sysroot(sysroot_path)
  chromeos_config = artifacts.BundleChromeOSConfig(chroot, sysroot, output_dir)
  if chromeos_config is None:
    cros_build_lib.Die(
        'Could not create ChromeOS Config payload. No config found for %s.',
        sysroot.path)
  output_proto.artifacts.add().path = os.path.join(output_dir, chromeos_config)


@faux.all_empty
@validate.require('output_dir', 'sysroot.build_target.name', 'sysroot.path')
@validate.exists('output_dir')
@validate.validation_complete
def BundleSimpleChromeArtifacts(input_proto, output_proto, _config):
  """Create the simple chrome artifacts."""
  # Required args.
  sysroot_path = input_proto.sysroot.path
  build_target_name = input_proto.sysroot.build_target.name
  output_dir = input_proto.output_dir

  # Optional args.
  chroot_path = input_proto.chroot.path or constants.DEFAULT_CHROOT_PATH
  cache_dir = input_proto.chroot.cache_dir

  # Build out the argument instances.
  build_target = build_target_util.BuildTarget(build_target_name)
  chroot = chroot_lib.Chroot(path=chroot_path, cache_dir=cache_dir)
  # Sysroot.path needs to be the fully qualified path, including the chroot.
  full_sysroot_path = os.path.join(chroot.path, sysroot_path.lstrip(os.sep))
  sysroot = sysroot_lib.Sysroot(full_sysroot_path)

  # Quick sanity check that the sysroot exists before we go on.
  if not sysroot.Exists():
    cros_build_lib.Die('The sysroot does not exist.')

  try:
    results = artifacts.BundleSimpleChromeArtifacts(chroot, sysroot,
                                                    build_target, output_dir)
  except artifacts.Error as e:
    cros_build_lib.Die('Error %s raised in BundleSimpleChromeArtifacts: %s',
                       type(e), e)

  for file_name in results:
    output_proto.artifacts.add().path = file_name


@faux.all_empty
@validate.require('chroot.path', 'test_results_dir', 'output_dir')
@validate.exists('output_dir')
@validate.validation_complete
def BundleVmFiles(input_proto, output_proto, _config):
  """Tar VM disk and memory files.

  Args:
    input_proto (SysrootBundleRequest): The input proto.
    output_proto (BundleResponse): The output proto.
    _config (api_config.ApiConfig): The API call config.
  """
  chroot = controller_util.ParseChroot(input_proto.chroot)
  test_results_dir = input_proto.test_results_dir
  output_dir = input_proto.output_dir

  archives = artifacts.BundleVmFiles(
      chroot, test_results_dir, output_dir)
  for archive in archives:
    output_proto.artifacts.add().path = archive


_VALID_ARTIFACT_TYPES = [toolchain_pb2.BENCHMARK_AFDO,
                         toolchain_pb2.ORDERFILE]
@faux.all_empty
@validate.require('build_target.name', 'output_dir')
@validate.is_in('artifact_type', _VALID_ARTIFACT_TYPES)
@validate.exists('output_dir')
@validate.validation_complete
def BundleAFDOGenerationArtifacts(input_proto, output_proto, _config):
  """Generic function for creating tarballs of both AFDO and orerfile.

  Args:
    input_proto (BundleChromeAFDORequest): The input proto.
    output_proto (BundleResponse): The output proto.
    _config (api_config.ApiConfig): The API call config.
  """

  # Required args.
  build_target = build_target_util.BuildTarget(input_proto.build_target.name)
  output_dir = input_proto.output_dir
  artifact_type = input_proto.artifact_type

  chroot = controller_util.ParseChroot(input_proto.chroot)

  try:
    is_orderfile = bool(artifact_type is toolchain_pb2.ORDERFILE)
    results = artifacts.BundleAFDOGenerationArtifacts(
        is_orderfile, chroot,
        build_target, output_dir)
  except artifacts.Error as e:
    cros_build_lib.Die('Error %s raised in BundleSimpleChromeArtifacts: %s',
                       type(e), e)

  for file_name in results:
    output_proto.artifacts.add().path = file_name


@faux.all_empty
@validate.exists('output_dir')
def ExportCpeReport(input_proto, output_proto, config):
  """Export a CPE report.

  Args:
    input_proto (BundleRequest): The input proto.
    output_proto (BundleResponse): The output proto.
    config (api_config.ApiConfig): The API call config.
  """
  chroot = controller_util.ParseChroot(input_proto.chroot)
  output_dir = input_proto.output_dir

  if input_proto.build_target.name:
    # Legacy handling - use the default sysroot path for the build target.
    build_target = controller_util.ParseBuildTarget(input_proto.build_target)
    sysroot = sysroot_lib.Sysroot(build_target.root)
  elif input_proto.sysroot.path:
    sysroot = sysroot_lib.Sysroot(input_proto.sysroot.path)
  else:
    # TODO(saklein): Switch to validate decorators once legacy handling can be
    #   cleaned up.
    cros_build_lib.Die('sysroot.path is required.')

  if config.validate_only:
    return controller.RETURN_CODE_VALID_INPUT

  cpe_result = artifacts.GenerateCpeReport(chroot, sysroot, output_dir)

  output_proto.artifacts.add().path = cpe_result.report
  output_proto.artifacts.add().path = cpe_result.warnings
