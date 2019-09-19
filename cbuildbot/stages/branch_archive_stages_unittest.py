# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for workspace stages."""

from __future__ import print_function

import os
import shutil

import mock

from chromite.cbuildbot.builders import workspace_builders_unittest
from chromite.cbuildbot import commands
from chromite.cbuildbot import manifest_version
from chromite.cbuildbot.stages import branch_archive_stages
from chromite.cbuildbot.stages import generic_stages
from chromite.cbuildbot.stages import workspace_stages_unittest
from chromite.lib import cros_build_lib
from chromite.lib import gs
from chromite.lib import gs_unittest
from chromite.lib import osutils
from chromite.lib import portage_util

# pylint: disable=too-many-ancestors
# pylint: disable=protected-access

class BranchArchiveStageTestBase(workspace_stages_unittest.WorkspaceStageBase,
                                 gs_unittest.AbstractGSContextTest):
  """Base class for test suites."""

  def setUp(self):
    self.CreateMockOverlay('board', self.workspace)

    self.upload_mock = self.PatchObject(
        generic_stages.ArchivingStageMixin, 'UploadArtifact')

    # It would be nice to include the prefix in the return path. How?
    tempdir_mock = self.PatchObject(osutils, 'TempDir')
    tempdir_mock.return_value.__enter__.return_value = '/tempdir'

    self.copy_mock = self.PatchObject(shutil, 'copyfile')
    self.mkdir_mock = self.PatchObject(os, 'mkdir')
    self.gs_copy_mock = self.PatchObject(gs.GSContext, 'CopyInto')

    self.push_mock = self.PatchObject(
        commands, 'PushImages')

    self.write_mock = self.PatchObject(osutils, 'WriteFile')

    self.read_overlay_file_mock = self.PatchObject(
        portage_util, 'ReadOverlayFile',
        return_value='{"extra_upload_urls":["gs://chromeos-extra-archive"]}')

  def ConstructStage(self):
    """Returns an instance of the stage to be tested.

    Note: Must be implemented in subclasses.
    """
    raise NotImplementedError(self, 'ConstructStage: Implement in your test')


class FirmwareArchiveStageTest(BranchArchiveStageTestBase):
  """Test FirmwareArchiveStage."""

  def ConstructStage(self):
    self._run.attrs.version_info = manifest_version.VersionInfo(
        '10.0.0', chrome_branch='10')
    self._run.attrs.release_tag = 'infra-tag'

    return branch_archive_stages.FirmwareArchiveStage(
        self._run, self.buildstore, build_root=self.workspace, board='board')

  def testProd(self):
    """Tests sync command used by default."""
    self.PatchObject(commands, 'BuildFirmwareArchive',
                     return_value='/tempdir/firmware_from_source.tar.bz2')
    self._Prepare(
        'test-firmwarebranch',
        site_config=workspace_builders_unittest.CreateMockSiteConfig())

    self.RunStage()

    # Validate properties.
    self.assertEqual(self.stage.dummy_config,
                     'board-firmware')
    self.assertEqual(self.stage.dummy_version,
                     'R1-1.2.3')
    self.assertEqual(self.stage.dummy_archive_url,
                     'gs://chromeos-image-archive/board-firmware/R1-1.2.3')

    self.assertEqual(self.copy_mock.call_args_list, [
        mock.call(
            '/tempdir/firmware_from_source.tar.bz2',
            '/tempdir/board/firmware_from_source.tar.bz2'),
        mock.call(
            '/tempdir/metadata.json',
            '/tempdir/board/metadata.json'),
    ])

    self.assertEqual(self.upload_mock.call_args_list, [
        mock.call('/tempdir/board/firmware_from_source.tar.bz2', archive=True),
        mock.call('/tempdir/board/metadata.json', archive=True),
    ])

    self.assertEqual(self.read_overlay_file_mock.call_args_list, 2 * [
        mock.call(
            'scripts/artifacts.json', buildroot=self.workspace, board='board'),
    ])

    self.assertEqual(self.gs_copy_mock.call_args_list, [
        mock.call(
            '/tempdir/firmware_from_source.tar.bz2',
            'gs://chromeos-image-archive/board-firmware/R1-1.2.3',
            recursive=True, parallel=True),
        mock.call(
            '/tempdir/firmware_from_source.tar.bz2',
            'gs://chromeos-extra-archive/board-firmware/R1-1.2.3',
            recursive=True, parallel=True),
        mock.call(
            '/tempdir/metadata.json',
            'gs://chromeos-image-archive/board-firmware/R1-1.2.3',
            recursive=True, parallel=True),
        mock.call(
            '/tempdir/metadata.json',
            'gs://chromeos-extra-archive/board-firmware/R1-1.2.3',
            recursive=True, parallel=True),
    ])

    self.assertEqual(self.push_mock.call_args_list, [
        mock.call(
            profile=None,
            buildroot=self.workspace,
            sign_types=['firmware', 'accessory_rwsig'],
            dryrun=False,
            archive_url=('gs://chromeos-image-archive/'
                         'board-firmware/R1-1.2.3'),
            board='board'),
    ])

  def testDebug(self):
    """Tests sync command used by default."""
    self.PatchObject(commands, 'BuildFirmwareArchive',
                     return_value='/tempdir/firmware_from_source.tar.bz2')
    self._Prepare(
        'test-firmwarebranch',
        site_config=workspace_builders_unittest.CreateMockSiteConfig(),
        extra_cmd_args=['--debug'])

    self.RunStage()

    # Validate properties.
    self.assertEqual(self.stage.dummy_config,
                     'board-firmware-tryjob')
    self.assertEqual(self.stage.dummy_version,
                     'R1-1.2.3-bNone')
    self.assertEqual(self.stage.dummy_archive_url,
                     'gs://chromeos-image-archive/board-firmware-tryjob/'
                     'R1-1.2.3-bNone')

    self.assertEqual(self.copy_mock.call_args_list, [
        mock.call(
            '/tempdir/firmware_from_source.tar.bz2',
            '/tempdir/board/firmware_from_source.tar.bz2'),
        mock.call(
            '/tempdir/metadata.json',
            '/tempdir/board/metadata.json'),
    ])

    self.assertEqual(self.upload_mock.call_args_list, [
        mock.call('/tempdir/board/firmware_from_source.tar.bz2', archive=True),
        mock.call('/tempdir/board/metadata.json', archive=True),
    ])

    self.assertEqual(self.read_overlay_file_mock.call_args_list, 2 * [
        mock.call(
            'scripts/artifacts.json', buildroot=self.workspace, board='board'),
    ])

    self.assertEqual(self.gs_copy_mock.call_args_list, [
        mock.call(
            '/tempdir/firmware_from_source.tar.bz2',
            'gs://chromeos-image-archive/board-firmware-tryjob/R1-1.2.3-bNone',
            recursive=True, parallel=True),
        mock.call(
            '/tempdir/firmware_from_source.tar.bz2',
            'gs://chromeos-extra-archive/board-firmware-tryjob/R1-1.2.3-bNone',
            recursive=True, parallel=True),
        mock.call(
            '/tempdir/metadata.json',
            'gs://chromeos-image-archive/board-firmware-tryjob/R1-1.2.3-bNone',
            recursive=True, parallel=True),
        mock.call(
            '/tempdir/metadata.json',
            'gs://chromeos-extra-archive/board-firmware-tryjob/R1-1.2.3-bNone',
            recursive=True, parallel=True),
    ])

    self.assertEqual(self.push_mock.call_args_list, [
        mock.call(
            profile=None,
            buildroot=self.workspace,
            sign_types=['firmware', 'accessory_rwsig'],
            dryrun=True,
            archive_url=('gs://chromeos-image-archive/'
                         'board-firmware-tryjob/R1-1.2.3-bNone'),
            board='board'),
    ])


class FactoryArchiveStageTest(BranchArchiveStageTestBase):
  """Test FactoryArchiveStage."""

  def setUp(self):
    self.build_image_mock = self.PatchObject(
        commands, 'BuildFactoryInstallImage',
        return_value='/factory_image')
    self.net_boot_mock = self.PatchObject(
        commands, 'MakeNetboot')
    self.factory_zip_mock = self.PatchObject(
        commands, 'BuildFactoryZip',
        return_value='/factory.zip')
    self.create_tar_mock = self.PatchObject(
        cros_build_lib, 'CreateTarball')

  def ConstructStage(self):
    self._run.attrs.version_info = manifest_version.VersionInfo(
        '10.0.0', chrome_branch='10')
    self._run.attrs.release_tag = 'infra-tag'

    return branch_archive_stages.FactoryArchiveStage(
        self._run, self.buildstore, build_root=self.workspace, board='board')

  def testProd(self):
    """Tests sync command used by default."""
    self._Prepare(
        'test-factorybranch',
        site_config=workspace_builders_unittest.CreateMockSiteConfig())

    self.RunStage()

    # Validate properties.
    self.assertEqual(self.stage.dummy_config,
                     'board-factory')
    self.assertEqual(self.stage.dummy_version,
                     'R1-1.2.3')
    self.assertEqual(self.stage.dummy_archive_url,
                     'gs://chromeos-image-archive/board-factory/R1-1.2.3')

    self.assertEqual(self.build_image_mock.call_args_list, [
        mock.call(
            self.workspace,
            'board',
            extra_env={
                'FEATURES': 'separatedebug',
                'USE': '-cros-debug chrome_internal',
            }),
    ])

    self.assertEqual(self.net_boot_mock.call_args_list, [
        mock.call(
            self.workspace,
            'board',
            '/factory_image'),
    ])

    self.assertEqual(self.factory_zip_mock.call_args_list, [
        mock.call(
            self.workspace,
            'board',
            '/tempdir',
            '/factory_image',
            'R1-1.2.3'),
    ])

    self.assertEqual(self.create_tar_mock.call_args_list, [
        mock.call(
            '/tempdir/chromiumos_test_image.tar.xz',
            inputs=['chromiumos_test_image.bin'],
            compression=1,
            cwd=os.path.join(self.workspace,
                             'src/build/images/board/latest')),
    ])

    self.assertEqual(self.copy_mock.call_args_list, [
        mock.call(
            '/factory.zip',
            '/tempdir/board/factory.zip'),
        mock.call(
            '/tempdir/chromiumos_test_image.tar.xz',
            '/tempdir/board/chromiumos_test_image.tar.xz'),
        mock.call(
            '/tempdir/metadata.json',
            '/tempdir/board/metadata.json'),
    ])

    self.assertEqual(self.upload_mock.call_args_list, [
        mock.call('/tempdir/board/factory.zip', archive=True),
        mock.call('/tempdir/board/chromiumos_test_image.tar.xz', archive=True),
        mock.call('/tempdir/board/metadata.json', archive=True),
    ])

    self.assertEqual(self.read_overlay_file_mock.call_args_list, 3 * [
        mock.call(
            'scripts/artifacts.json', buildroot=self.workspace, board='board')
    ])

    self.assertEqual(self.gs_copy_mock.call_args_list, [
        mock.call(
            '/factory.zip',
            'gs://chromeos-image-archive/board-factory/R1-1.2.3',
            recursive=True, parallel=True),
        mock.call(
            '/factory.zip',
            'gs://chromeos-extra-archive/board-factory/R1-1.2.3',
            recursive=True, parallel=True),
        mock.call(
            '/tempdir/chromiumos_test_image.tar.xz',
            'gs://chromeos-image-archive/board-factory/R1-1.2.3',
            recursive=True, parallel=True),
        mock.call(
            '/tempdir/chromiumos_test_image.tar.xz',
            'gs://chromeos-extra-archive/board-factory/R1-1.2.3',
            recursive=True, parallel=True),
        mock.call(
            '/tempdir/metadata.json',
            'gs://chromeos-image-archive/board-factory/R1-1.2.3',
            recursive=True, parallel=True),
        mock.call(
            '/tempdir/metadata.json',
            'gs://chromeos-extra-archive/board-factory/R1-1.2.3',
            recursive=True, parallel=True),
    ])

    self.assertEqual(self.push_mock.call_args_list, [
        mock.call(
            profile=None,
            buildroot=self.workspace,
            sign_types=['factory'],
            dryrun=False,
            archive_url=('gs://chromeos-image-archive/'
                         'board-factory/R1-1.2.3'),
            board='board'),
    ])

  def testDebug(self):
    """Tests sync command used by default."""
    self._Prepare(
        'test-factorybranch',
        site_config=workspace_builders_unittest.CreateMockSiteConfig(),
        extra_cmd_args=['--debug'])

    self.RunStage()

    # Validate properties.
    self.assertEqual(self.stage.dummy_config,
                     'board-factory-tryjob')
    self.assertEqual(self.stage.dummy_version,
                     'R1-1.2.3-bNone')
    self.assertEqual(self.stage.dummy_archive_url,
                     'gs://chromeos-image-archive/board-factory-tryjob/'
                     'R1-1.2.3-bNone')

    self.assertEqual(self.build_image_mock.call_args_list, [
        mock.call(
            self.workspace,
            'board',
            extra_env={
                'FEATURES': 'separatedebug',
                'USE': '-cros-debug chrome_internal',
            }),
    ])

    self.assertEqual(self.net_boot_mock.call_args_list, [
        mock.call(
            self.workspace,
            'board',
            '/factory_image'),
    ])

    self.assertEqual(self.factory_zip_mock.call_args_list, [
        mock.call(
            self.workspace,
            'board',
            '/tempdir',
            '/factory_image',
            'R1-1.2.3-bNone'),
    ])

    self.assertEqual(self.create_tar_mock.call_args_list, [
        mock.call(
            '/tempdir/chromiumos_test_image.tar.xz',
            inputs=['chromiumos_test_image.bin'],
            compression=1,
            cwd=os.path.join(self.workspace,
                             'src/build/images/board/latest')),
    ])


    self.assertEqual(self.copy_mock.call_args_list, [
        mock.call(
            '/factory.zip',
            '/tempdir/board/factory.zip'),
        mock.call(
            '/tempdir/chromiumos_test_image.tar.xz',
            '/tempdir/board/chromiumos_test_image.tar.xz'),
        mock.call(
            '/tempdir/metadata.json',
            '/tempdir/board/metadata.json'),
    ])

    self.assertEqual(self.upload_mock.call_args_list, [
        mock.call('/tempdir/board/factory.zip', archive=True),
        mock.call('/tempdir/board/chromiumos_test_image.tar.xz', archive=True),
        mock.call('/tempdir/board/metadata.json', archive=True),
    ])

    self.assertEqual(self.read_overlay_file_mock.call_args_list, 3 * [
        mock.call(
            'scripts/artifacts.json', buildroot=self.workspace, board='board'),
    ])

    self.assertEqual(self.gs_copy_mock.call_args_list, [
        mock.call(
            '/factory.zip',
            'gs://chromeos-image-archive/board-factory-tryjob/R1-1.2.3-bNone',
            recursive=True, parallel=True),
        mock.call(
            '/factory.zip',
            'gs://chromeos-extra-archive/board-factory-tryjob/R1-1.2.3-bNone',
            recursive=True, parallel=True),
        mock.call(
            '/tempdir/chromiumos_test_image.tar.xz',
            'gs://chromeos-image-archive/board-factory-tryjob/R1-1.2.3-bNone',
            recursive=True, parallel=True),
        mock.call(
            '/tempdir/chromiumos_test_image.tar.xz',
            'gs://chromeos-extra-archive/board-factory-tryjob/R1-1.2.3-bNone',
            recursive=True, parallel=True),
        mock.call(
            '/tempdir/metadata.json',
            'gs://chromeos-image-archive/board-factory-tryjob/R1-1.2.3-bNone',
            recursive=True, parallel=True),
        mock.call(
            '/tempdir/metadata.json',
            'gs://chromeos-extra-archive/board-factory-tryjob/R1-1.2.3-bNone',
            recursive=True, parallel=True),
    ])

    self.assertEqual(self.push_mock.call_args_list, [
        mock.call(
            profile=None,
            buildroot=self.workspace,
            sign_types=['factory'],
            dryrun=True,
            archive_url=('gs://chromeos-image-archive/'
                         'board-factory-tryjob/R1-1.2.3-bNone'),
            board='board'),
    ])
