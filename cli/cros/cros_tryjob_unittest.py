# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests the `cros chroot` command."""

from __future__ import print_function

from chromite.cli import command_unittest
from chromite.cli.cros import cros_tryjob
from chromite.lib import cros_test_lib


class MockTryjobCommand(command_unittest.MockCommand):
  """Mock out the `cros tryjob` command."""
  TARGET = 'chromite.cli.cros.cros_tryjob.TryjobCommand'
  TARGET_CLASS = cros_tryjob.TryjobCommand
  COMMAND = 'tryjob'


class TryjobTest(cros_test_lib.MockTestCase):
  """Test the ChrootCommand."""

  def setUp(self):
    self.cmd_mock = None

  def SetupCommandMock(self, cmd_args):
    """Sets up the `cros chroot` command mock."""
    self.cmd_mock = MockTryjobCommand(cmd_args)
    self.StartPatcher(self.cmd_mock)

  def testMinimalParsing(self):
    """Tests flow for an interactive session."""
    self.SetupCommandMock(['lumpy-paladin'])
    options = self.cmd_mock.inst.options

    self.assertEqual(options.build_configs, ['lumpy-paladin'])
    self.assertTrue(options.remote)
    self.assertIsNone(options.branch)
    self.assertFalse(options.hwtest)
    self.assertFalse(options.production)
    self.assertEqual(options.passthrough, [])
    self.assertFalse(options.yes)
    self.assertEqual(options.gerrit_patches, [])
    self.assertEqual(options.local_patches, [])

  def testComplexParsing(self):
    """Tests flow for an interactive session."""
    self.SetupCommandMock([
        '--yes', '--hwtest',
        '--local', '--buildroot', '/buildroot',
        '--gerrit-patches', '123', '-g', '*123', '-g', '123..456',
        '--local-patches', 'chromiumos/chromite:tryjob', '-p', 'other:other',
        'lumpy-paladin', 'lumpy-release',
        '--passthrough', 'foo', '--cbuild-arg', '--b-arg', 'bar',
    ])
    options = self.cmd_mock.inst.options

    self.assertEqual(options.build_configs, ['lumpy-paladin', 'lumpy-release'])
    self.assertFalse(options.remote)
    self.assertEqual(options.buildroot, '/buildroot')
    self.assertIsNone(options.branch)
    self.assertTrue(options.hwtest)
    self.assertFalse(options.production)
    self.assertEqual(options.passthrough,
                     ['foo', '--cbuild-arg', '--b-arg', 'bar'])
    self.assertTrue(options.yes)
    self.assertEqual(options.gerrit_patches, ['123', '*123', '123..456'])
    self.assertEqual(options.local_patches,
                     ['chromiumos/chromite:tryjob', 'other:other'])

  def testPayloadsParsing(self):
    """Tests flow for an interactive session."""
    self.SetupCommandMock([
        '--version', '9795.0.0', '--channel', 'canary', 'lumpy-payloads'
    ])
    options = self.cmd_mock.inst.options

    self.assertEqual(options.build_configs, ['lumpy-payloads'])
    self.assertTrue(options.remote)
    self.assertIsNone(options.branch)
    self.assertFalse(options.hwtest)
    self.assertFalse(options.production)
    self.assertEqual(options.passthrough, [])
    self.assertFalse(options.yes)
    self.assertEqual(options.gerrit_patches, [])
    self.assertEqual(options.local_patches, [])

  def testDashDashParsing(self):
    """Tests flow for an interactive session."""
    self.SetupCommandMock([
        '--passthrough', 'foo', '--cbuild-arg', '--b-arg', 'bar',
        '--', 'lumpy-paladin', 'lumpy-release',
    ])
    options = self.cmd_mock.inst.options

    self.assertEqual(options.build_configs, ['lumpy-paladin', 'lumpy-release'])
    self.assertTrue(options.remote)
    self.assertIsNone(options.branch)
    self.assertFalse(options.hwtest)
    self.assertFalse(options.production)
    self.assertEqual(options.passthrough,
                     ['foo', '--cbuild-arg', '--b-arg', 'bar'])
    self.assertFalse(options.yes)
    self.assertEqual(options.gerrit_patches, [])
    self.assertEqual(options.local_patches, [])
