# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Implements ArtifactService."""

from __future__ import print_function

import os

from chromite.api import validate
from chromite.api.controller import controller_util
from chromite.cbuildbot import commands
from chromite.cbuildbot.stages import vm_test_stages
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


def BundleImageZip(input_proto, output_proto):
  """Bundle image.zip.

  Args:
    input_proto (BundleRequest): The input proto.
    output_proto (BundleResponse): The output proto.
  """
  target = input_proto.build_target.name
  output_dir = input_proto.output_dir
  image_dir = _GetImageDir(constants.SOURCE_ROOT, target)
  archive = commands.BuildImageZip(output_dir, image_dir)
  output_proto.artifacts.add().path = os.path.join(output_dir, archive)


def BundleTestUpdatePayloads(input_proto, output_proto):
  """Generate minimal update payloads for the build target for testing.

  Args:
    input_proto (BundleRequest): The input proto.
    output_proto (BundleResponse): The output proto.
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


def BundleAutotestFiles(input_proto, output_proto):
  """Tar the autotest files for a build target.

  Args:
    input_proto (BundleRequest): The input proto.
    output_proto (BundleResponse): The output proto.
  """
  output_dir = input_proto.output_dir
  if not output_dir:
    cros_build_lib.Die('output_dir is required.')

  target = input_proto.build_target.name
  if target:
    # Legacy call, build out sysroot path from default source root and the
    # build target.
    target = input_proto.build_target.name
    build_root = constants.SOURCE_ROOT
    sysroot_path = os.path.join(build_root, constants.DEFAULT_CHROOT_DIR,
                                'build', target)
    sysroot = sysroot_lib.Sysroot(sysroot_path)
  else:
    # New style call, use chroot and sysroot.
    chroot = controller_util.ParseChroot(input_proto.chroot)

    sysroot_path = input_proto.sysroot.path
    if not sysroot_path:
      cros_build_lib.Die('sysroot.path is required.')

    # Since we're staying outside the chroot, prepend the chroot path to the
    # sysroot path so we have a valid full path to the sysroot.
    sysroot = sysroot_lib.Sysroot(os.path.join(chroot.path,
                                               sysroot_path.lstrip(os.sep)))

  if not sysroot.Exists():
    cros_build_lib.Die('Sysroot path must exist: %s', sysroot.path)

  try:
    # Note that this returns the full path to *multiple* tarballs.
    archives = artifacts.BundleAutotestFiles(sysroot, output_dir)
  except artifacts.Error as e:
    cros_build_lib.Die(e.message)

  for archive in archives.values():
    output_proto.artifacts.add().path = archive


@validate.require('output_dir')
def BundleTastFiles(input_proto, output_proto):
  """Tar the tast files for a build target.

  Args:
    input_proto (BundleRequest): The input proto.
    output_proto (BundleResponse): The output proto.
  """
  target = input_proto.build_target.name
  output_dir = input_proto.output_dir
  build_root = constants.SOURCE_ROOT

  chroot = controller_util.ParseChroot(input_proto.chroot)
  sysroot_path = input_proto.sysroot.path

  # TODO(saklein) Cleanup legacy handling after it has been switched over.
  if target:
    # Legacy handling.
    chroot.path = os.path.join(build_root, 'chroot')
    sysroot_path = os.path.join('/build', target)

  # New handling - chroot & sysroot based.
  # TODO(saklein) Switch this to the require decorator when legacy is removed.
  if not sysroot_path:
    cros_build_lib.Die('sysroot.path is required.')

  sysroot = sysroot_lib.Sysroot(sysroot_path)
  if not sysroot.Exists(chroot):
    cros_build_lib.Die('Sysroot must exist.')

  archive = artifacts.BundleTastFiles(chroot, sysroot, output_dir)

  if archive is None:
    cros_build_lib.Die(
        'Could not bundle Tast files. '
        'No Tast directories found for %s.', target)

  output_proto.artifacts.add().path = archive


def BundlePinnedGuestImages(input_proto, output_proto):
  """Tar the pinned guest images for a build target.

  Args:
    input_proto (BundleRequest): The input proto.
    output_proto (BundleResponse): The output proto.
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


def FetchPinnedGuestImages(input_proto, output_proto):
  """Get the pinned guest image information."""
  sysroot_path = input_proto.sysroot.path
  if not sysroot_path:
    cros_build_lib.Die('sysroot.path is required.')

  chroot = controller_util.ParseChroot(input_proto.chroot)
  sysroot = sysroot_lib.Sysroot(sysroot_path)

  if not chroot.exists():
    cros_build_lib.Die('Chroot does not exist: %s', chroot.path)

  if not sysroot.Exists(chroot=chroot):
    cros_build_lib.Die('Sysroot does not exist: %s',
                       chroot.full_path(sysroot.path))

  pins = artifacts.FetchPinnedGuestImages(chroot, sysroot)

  for pin in pins:
    pinned_image = output_proto.pinned_images.add()
    pinned_image.filename = pin.filename
    pinned_image.uri = pin.uri


@validate.require('output_dir', 'sysroot.path')
def BundleFirmware(input_proto, output_proto):
  """Tar the firmware images for a build target.

  Args:
    input_proto (BundleRequest): The input proto.
    output_proto (BundleResponse): The output proto.
  """
  output_dir = input_proto.output_dir
  chroot = controller_util.ParseChroot(input_proto.chroot)
  sysroot_path = input_proto.sysroot.path
  sysroot = sysroot_lib.Sysroot(sysroot_path)
  archive = artifacts.BuildFirmwareArchive(chroot, sysroot, output_dir)

  if archive is None:
    cros_build_lib.Die(
        'Could not create firmware archive. No firmware found for %s.',
        sysroot_path)

  output_proto.artifacts.add().path = os.path.join(output_dir, archive)


def BundleEbuildLogs(input_proto, output_proto):
  """Tar the ebuild logs for a build target.

  Args:
    input_proto (BundleRequest): The input proto.
    output_proto (BundleResponse): The output proto.
  """
  target = input_proto.build_target.name
  output_dir = input_proto.output_dir

  # commands.BuildEbuildLogsTarball conflates "buildroot" with sysroot.
  # Adjust accordingly...
  build_root = os.path.join(constants.SOURCE_ROOT, 'chroot', 'build')

  # TODO(crbug.com/954303): Replace with a chromite/service implementation.
  archive = commands.BuildEbuildLogsTarball(build_root, target, output_dir)

  if archive is None:
    cros_build_lib.Die(
        'Could not create ebuild logs archive. No logs found for %s.', target)

  output_proto.artifacts.add().path = os.path.join(output_dir, archive)


def BundleSimpleChromeArtifacts(input_proto, output_proto):
  """Create the simple chrome artifacts."""
  # Required args.
  sysroot_path = input_proto.sysroot.path
  build_target_name = input_proto.sysroot.build_target.name
  output_dir = input_proto.output_dir

  if not build_target_name:
    cros_build_lib.Die('build_target.name is required')
  if not output_dir:
    cros_build_lib.Die('output_dir is required.')
  if not os.path.exists(output_dir):
    cros_build_lib.Die('output_dir (%s) does not exist.', output_dir)
  if not sysroot_path:
    cros_build_lib.Die('sysroot.path is required.')

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


@validate.require('chroot.path', 'sysroot.path', 'test_results_dir',
                  'output_dir')
def BundleVmFiles(input_proto, output_proto):
  """Tar VM disk and memory files.

  Args:
    input_proto (SysrootBundleRequest): The input proto.
    output_proto (BundleResponse): The output proto.
  """
  chroot = input_proto.chroot.path
  sysroot = input_proto.sysroot.path.lstrip(os.sep)
  test_results_dir = input_proto.test_results_dir.lstrip(os.sep)
  output_dir = input_proto.output_dir

  # TODO(crbug.com/954344): Replace with a chromite/service implementation.
  image_dir = os.path.join(chroot, sysroot, test_results_dir)
  archives = vm_test_stages.ArchiveVMFilesFromImageDir(image_dir, output_dir)
  for archive in archives:
    output_proto.artifacts.add().path = archive

def BundleOrderfileGenerationArtifacts(input_proto, output_proto):
  """Create tarballs of all the artifacts of orderfile_generate builder.

  Args:
    input_proto (BundleRequest): The input proto.
    output_proto (BundleResponse): The output proto.
  """
  # Required args.
  build_target_name = input_proto.build_target.name
  output_dir = input_proto.output_dir
  chrome_version = input_proto.chrome_version

  if not build_target_name:
    cros_build_lib.Die('build_target.name is required.')
  if not chrome_version:
    cros_build_lib.Die('chrome_version is required.')
  if not output_dir:
    cros_build_lib.Die('output_dir is required.')
  elif not os.path.isdir(output_dir):
    cros_build_lib.Die('output_dir does not exist.')

  chroot = controller_util.ParseChroot(input_proto.chroot)

  try:
    results = artifacts.BundleOrderfileGenerationArtifacts(
        chroot, input_proto.build_target, chrome_version, output_dir)
  except artifacts.Error as e:
    cros_build_lib.Die('Error %s raised in BundleSimpleChromeArtifacts: %s',
                       type(e), e)

  for file_name in results:
    output_proto.artifacts.add().path = file_name
