# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for Artifacts operations."""

from __future__ import print_function

import mock
import os

from chromite.api.controller import artifacts
from chromite.api.gen.chromite.api import artifacts_pb2
from chromite.cbuildbot import commands
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import osutils


class BundleTestCase(cros_test_lib.MockTestCase):
  """Basic setup for all artifacts unittests."""

  def setUp(self):
    self.input_proto = artifacts_pb2.BundleRequest()
    self.input_proto.build_target.name = 'target'
    self.input_proto.output_dir = '/tmp/artifacts'
    self.output_proto = artifacts_pb2.BundleResponse()

    self.PatchObject(constants, 'SOURCE_ROOT', new='/cros')


class BundleImageZipTest(BundleTestCase):
  """Unittests for BundleImageZip."""

  def testBundleImageZip(self):
    """BundleImageZip calls cbuildbot/commands with correct args."""
    build_image_zip = self.PatchObject(
        commands, 'BuildImageZip', return_value='image.zip')
    self.PatchObject(os.path, 'exists', return_value=True)
    artifacts.BundleImageZip(self.input_proto, self.output_proto)
    self.assertEqual(
        [artifact.path for artifact in self.output_proto.artifacts],
        ['/tmp/artifacts/image.zip'])
    self.assertEqual(
        build_image_zip.call_args_list,
        [mock.call('/tmp/artifacts', '/cros/src/build/images/target/latest')])

  def testBundleImageZipNoImageDir(self):
    """BundleImageZip dies when image dir does not exist."""
    self.PatchObject(os.path, 'exists', return_value=False)
    with self.assertRaises(cros_build_lib.DieSystemExit):
      artifacts.BundleImageZip(self.input_proto, self.output_proto)


class BundleAutotestFilesTest(BundleTestCase):
  """Unittests for BundleAutotestFiles."""

  def testBundleAutotestFiles(self):
    """BundleAutotestFiles calls cbuildbot/commands with correct args."""
    build_autotest_tarballs = self.PatchObject(
        commands,
        'BuildAutotestTarballsForHWTest',
        return_value=[
            '/tmp/artifacts/autotest-a.tar.gz',
            '/tmp/artifacts/autotest-b.tar.gz',
        ])
    artifacts.BundleAutotestFiles(self.input_proto, self.output_proto)
    self.assertItemsEqual([
        artifact.path for artifact in self.output_proto.artifacts
    ], ['/tmp/artifacts/autotest-a.tar.gz', '/tmp/artifacts/autotest-b.tar.gz'])
    self.assertEqual(build_autotest_tarballs.call_args_list, [
        mock.call('/cros', '/cros/chroot/build/target/usr/local/build',
                  '/tmp/artifacts')
    ])


class BundleTastFilesTest(BundleTestCase):
  """Unittests for BundleTastFiles."""

  def testBundleTastFiles(self):
    """BundleTastFiles calls cbuildbot/commands with correct args."""
    build_tast_bundle_tarball = self.PatchObject(
        commands,
        'BuildTastBundleTarball',
        return_value='/tmp/artifacts/tast.tar.gz')
    artifacts.BundleTastFiles(self.input_proto, self.output_proto)
    self.assertEqual(
        [artifact.path for artifact in self.output_proto.artifacts],
        ['/tmp/artifacts/tast.tar.gz'])
    self.assertEqual(build_tast_bundle_tarball.call_args_list, [
        mock.call('/cros', '/cros/chroot/build/target/build', '/tmp/artifacts')
    ])

  def testBundleTastFilesNoLogs(self):
    """BundleTasteFiles dies when no tast files found."""
    self.PatchObject(commands, 'BuildTastBundleTarball',
                     return_value=None)
    with self.assertRaises(cros_build_lib.DieSystemExit):
      artifacts.BundleTastFiles(self.input_proto, self.output_proto)


class BundlePinnedGuestImagesTest(BundleTestCase):
  """Unittests for BundlePinnedGuestImages."""

  def testBundlePinnedGuestImages(self):
    """BundlePinnedGuestImages calls cbuildbot/commands with correct args."""
    build_pinned_guest_images_tarball = self.PatchObject(
        commands,
        'BuildPinnedGuestImagesTarball',
        return_value='pinned-guest-images.tar.gz')
    artifacts.BundlePinnedGuestImages(self.input_proto, self.output_proto)
    self.assertEqual(
        [artifact.path for artifact in self.output_proto.artifacts],
        ['/tmp/artifacts/pinned-guest-images.tar.gz'])
    self.assertEqual(build_pinned_guest_images_tarball.call_args_list,
                     [mock.call('/cros', 'target', '/tmp/artifacts')])

  def testBundlePinnedGuestImagesNoLogs(self):
    """BundlePinnedGuestImages dies when no pinned images found."""
    self.PatchObject(commands, 'BuildPinnedGuestImagesTarball',
                     return_value=None)
    with self.assertRaises(cros_build_lib.DieSystemExit):
      artifacts.BundlePinnedGuestImages(self.input_proto, self.output_proto)


class BundleFirmwareTest(BundleTestCase):
  """Unittests for BundleFirmware."""

  def testBundleFirmware(self):
    """BundleFirmware calls cbuildbot/commands with correct args."""
    build_firmware_archive = self.PatchObject(
        commands, 'BuildFirmwareArchive', return_value='firmware.tar.gz')
    artifacts.BundleFirmware(self.input_proto, self.output_proto)
    self.assertEqual(
        [artifact.path for artifact in self.output_proto.artifacts],
        ['/tmp/artifacts/firmware.tar.gz'])
    self.assertEqual(build_firmware_archive.call_args_list,
                     [mock.call('/cros', 'target', '/tmp/artifacts')])

  def testBundleFirmwareNoLogs(self):
    """BundleFirmware dies when no firmware found."""
    self.PatchObject(commands, 'BuildFirmwareArchive', return_value=None)
    with self.assertRaises(cros_build_lib.DieSystemExit):
      artifacts.BundleFirmware(self.input_proto, self.output_proto)


class BundleEbuildLogsTest(BundleTestCase):
  """Unittests for BundleEbuildLogs."""

  def testBundleEbuildLogs(self):
    """BundleEbuildLogs calls cbuildbot/commands with correct args."""
    build_ebuild_logs_tarball = self.PatchObject(
        commands, 'BuildEbuildLogsTarball', return_value='ebuild-logs.tar.gz')
    artifacts.BundleEbuildLogs(self.input_proto, self.output_proto)
    self.assertEqual(
        [artifact.path for artifact in self.output_proto.artifacts],
        ['/tmp/artifacts/ebuild-logs.tar.gz'])
    self.assertEqual(
        build_ebuild_logs_tarball.call_args_list,
        [mock.call('/cros/chroot/build', 'target', '/tmp/artifacts')])

  def testBundleEbuildLogsNoLogs(self):
    """BundleEbuildLogs dies when no logs found."""
    self.PatchObject(commands, 'BuildEbuildLogsTarball', return_value=None)
    with self.assertRaises(cros_build_lib.DieSystemExit):
      artifacts.BundleEbuildLogs(self.input_proto, self.output_proto)


class BundleTestUpdatePayloadsTest(cros_test_lib.MockTempDirTestCase):
  """Unittests for BundleTestUpdatePayloads."""

  def setUp(self):
    self.source_root = os.path.join(self.tempdir, 'cros')
    osutils.SafeMakedirs(self.source_root)

    self.archive_root = os.path.join(self.tempdir, 'output')
    osutils.SafeMakedirs(self.archive_root)

    self.target = 'target'
    self.image_root = os.path.join(self.source_root,
                                   'src/build/images/target/latest')

    self.input_proto = artifacts_pb2.BundleRequest()
    self.input_proto.build_target.name = self.target
    self.input_proto.output_dir = self.archive_root
    self.output_proto = artifacts_pb2.BundleResponse()

    self.PatchObject(constants, 'SOURCE_ROOT', new=self.source_root)

    def MockGeneratePayloads(image_path, archive_dir, **kwargs):
      assert kwargs
      osutils.WriteFile(os.path.join(archive_dir, 'payload.bin'), image_path)

    self.generate_payloads = self.PatchObject(
        commands, 'GeneratePayloads', side_effect=MockGeneratePayloads)

    def MockGenerateQuickProvisionPayloads(image_path, archive_dir):
      osutils.WriteFile(os.path.join(archive_dir, 'payload-qp.bin'), image_path)

    self.generate_quick_provision_payloads = self.PatchObject(
        commands,
        'GenerateQuickProvisionPayloads',
        side_effect=MockGenerateQuickProvisionPayloads)

  def testBundleTestUpdatePayloads(self):
    """BundleTestUpdatePayloads calls cbuildbot/commands with correct args."""
    image_path = os.path.join(self.image_root, constants.BASE_IMAGE_BIN)
    osutils.WriteFile(image_path, 'image!', makedirs=True)

    artifacts.BundleTestUpdatePayloads(self.input_proto, self.output_proto)

    actual = [
        os.path.relpath(artifact.path, self.archive_root)
        for artifact in self.output_proto.artifacts
    ]
    expected = ['payload.bin', 'payload-qp.bin']
    self.assertItemsEqual(actual, expected)

    actual = [
        os.path.relpath(path, self.archive_root)
        for path in osutils.DirectoryIterator(self.archive_root)
    ]
    self.assertItemsEqual(actual, expected)

    self.assertEqual(self.generate_payloads.call_args_list, [
        mock.call(image_path, mock.ANY, full=True, stateful=True, delta=True),
    ])

    self.assertEqual(self.generate_quick_provision_payloads.call_args_list,
                     [mock.call(image_path, mock.ANY)])

  def testBundleTestUpdatePayloadsNoImageDir(self):
    """BundleTestUpdatePayloads dies if no image dir is found."""
    # Intentionally do not write image directory.
    with self.assertRaises(cros_build_lib.DieSystemExit):
      artifacts.BundleTestUpdatePayloads(self.input_proto, self.output_proto)

  def testBundleTestUpdatePayloadsNoImage(self):
    """BundleTestUpdatePayloads dies if no usable image is found for target."""
    # Intentionally do not write image, but create the directory.
    osutils.SafeMakedirs(self.image_root)
    with self.assertRaises(cros_build_lib.DieSystemExit):
      artifacts.BundleTestUpdatePayloads(self.input_proto, self.output_proto)
