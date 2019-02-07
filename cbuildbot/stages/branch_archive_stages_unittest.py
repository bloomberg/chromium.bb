# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for workspace stages."""

from __future__ import print_function

import mock
import os

from chromite.cbuildbot.builders import workspace_builders_unittest
from chromite.cbuildbot import commands
from chromite.cbuildbot import manifest_version
from chromite.cbuildbot.stages import branch_archive_stages
from chromite.cbuildbot.stages import generic_stages
from chromite.cbuildbot.stages import workspace_stages_unittest
from chromite.lib import gs
from chromite.lib import gs_unittest
from chromite.lib import osutils

# pylint: disable=too-many-ancestors
# pylint: disable=protected-access


class FirmwareArchiveStageTest(workspace_stages_unittest.WorkspaceStageBase,
                               gs_unittest.AbstractGSContextTest):
  """Test the SyncStage."""

  def setUp(self):
    self.CreateMockOverlay('board', self.workspace)

    self.push_mock = self.PatchObject(
        commands, 'PushImages')
    self.upload_mock = self.PatchObject(
        generic_stages.ArchivingStageMixin, 'UploadArtifact')
    self.copy_mock = self.PatchObject(gs.GSContext, 'CopyInto')

    self.write_mock = self.PatchObject(osutils, 'WriteFile')

  def ConstructStage(self):
    self._run.attrs.version_info = manifest_version.VersionInfo(
        '10.0.0', chrome_branch='10')
    self._run.attrs.release_tag = 'infra-tag'

    return branch_archive_stages.FirmwareArchiveStage(
        self._run, self.buildstore, build_root=self.workspace, board='board')

  def testDefaults(self):
    """Tests sync command used by default."""
    self._Prepare(
        'test-firmwarebranch',
        site_config=workspace_builders_unittest.CreateMockSiteConfig())

    self.RunStage()

    # Validate properties.
    self.assertEqual(self.stage.dummy_firmware_config,
                     'board-firmware')
    self.assertEqual(self.stage.dummy_archive_url,
                     'gs://chromeos-image-archive/board-firmware/R1-1.2.3')
    self.assertEqual(self.stage.metadata_name,
                     'board_metadata.json')
    self.assertEqual(self.stage.firmware_name,
                     'board_firmware_from_source.tar.bz2')
    self.assertEqual(self.stage.firmware_version,
                     'R1-1.2.3')

    self.assertEqual(self.push_mock.call_args_list, [
        mock.call(
            profile=None,
            buildroot=self.workspace,
            sign_types=[u'firmware', u'accessory_rwsig'],
            dryrun=False,
            archive_url=('gs://chromeos-image-archive/'
                         'board-firmware/R1-1.2.3'),
            board='board'),
    ])

    self.assertEqual(self.upload_mock.call_args_list, [
        mock.call('board_firmware_from_source.tar.bz2', archive=False),
        mock.call('board_metadata.json', archive=False),
    ])

    self.assertEqual(self.copy_mock.call_args_list, [
        mock.call(
            os.path.join(
                self.build_root,
                'buildbot_archive/test-firmwarebranch/R10-infra-tag',
                'board_firmware_from_source.tar.bz2'),
            'gs://chromeos-image-archive/board-firmware/R1-1.2.3',
            filename='firmware_from_source.tar.bz2'),
        mock.call(
            os.path.join(
                self.build_root,
                'buildbot_archive/test-firmwarebranch/R10-infra-tag',
                'board_metadata.json'),
            'gs://chromeos-image-archive/board-firmware/R1-1.2.3',
            filename='metadata.json'),
    ])

    self.assertEqual(self.write_mock.call_args_list, [
        mock.call(
            os.path.join(
                self.build_root,
                'buildbot_archive/test-firmwarebranch/R10-infra-tag',
                'board_metadata.json'),
            # Metadata contents are complex, and mostly not defined in this
            # stage.
            mock.ANY,
            makedirs=True,
            atomic=True),
    ])

  def testDebug(self):
    """Tests sync command used by default."""
    self._Prepare(
        'test-firmwarebranch',
        site_config=workspace_builders_unittest.CreateMockSiteConfig(),
        extra_cmd_args=['--debug'])

    self.RunStage()

    # Validate properties.
    self.assertEqual(self.stage.dummy_firmware_config,
                     'board-firmware-tryjob')
    self.assertEqual(self.stage.dummy_archive_url,
                     ('gs://chromeos-image-archive/board-firmware-tryjob/'
                      'R1-1.2.3-bNone'))
    self.assertEqual(self.stage.metadata_name,
                     'board_metadata.json')
    self.assertEqual(self.stage.firmware_name,
                     'board_firmware_from_source.tar.bz2')
    self.assertEqual(self.stage.firmware_version,
                     'R1-1.2.3-bNone')

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

    self.assertEqual(self.upload_mock.call_args_list, [
        mock.call('board_firmware_from_source.tar.bz2', archive=False),
        mock.call('board_metadata.json', archive=False),
    ])

    self.assertEqual(self.copy_mock.call_args_list, [
        mock.call(
            os.path.join(
                self.build_root,
                'trybot_archive/test-firmwarebranch/R10-infra-tag',
                'board_firmware_from_source.tar.bz2'),
            'gs://chromeos-image-archive/board-firmware-tryjob/R1-1.2.3-bNone',
            filename='firmware_from_source.tar.bz2'),
        mock.call(
            os.path.join(
                self.build_root,
                'trybot_archive/test-firmwarebranch/R10-infra-tag',
                'board_metadata.json'),
            'gs://chromeos-image-archive/board-firmware-tryjob/R1-1.2.3-bNone',
            filename='metadata.json'),
    ])

    self.assertEqual(self.write_mock.call_args_list, [
        mock.call(
            os.path.join(
                self.build_root,
                'trybot_archive/test-firmwarebranch/R10-infra-tag',
                'board_metadata.json'),
            # Metadata contents are complex, and mostly not defined in this
            # stage.
            mock.ANY,
            makedirs=True,
            atomic=True),
    ])
