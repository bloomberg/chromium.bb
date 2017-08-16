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
  """Test the TryjobCommand."""

  def setUp(self):
    self.cmd_mock = None

  def SetupCommandMock(self, cmd_args):
    """Sets up the `cros chroot` command mock."""
    self.cmd_mock = MockTryjobCommand(cmd_args)
    self.StartPatcher(self.cmd_mock)


class TryjobTestParsing(TryjobTest):
  """Test cros try command line parsing."""

  def setUp(self):
    self.expected = {
        'remote': True,
        'branch': None,
        'production': False,
        'yes': False,
        'gerrit_patches': [],
        'local_patches': [],
        'passthrough': None,
        'passthrough_raw': None,
        'build_configs': ['lumpy-paladin'],
    }

  def SetupCommandMock(self, cmd_args):
    """Sets up the `cros chroot` command mock."""
    self.cmd_mock = MockTryjobCommand(cmd_args)
    self.StartPatcher(self.cmd_mock)

  def testMinimalParsing(self):
    """Tests flow for an interactive session."""
    self.SetupCommandMock(['lumpy-paladin'])
    options = self.cmd_mock.inst.options

    self.assertDictContainsSubset(self.expected, vars(options))

  def testComplexParsing(self):
    """Tests flow for an interactive session."""
    self.SetupCommandMock([
        '--yes',
        '--latest-toolchain', '--nochromesdk',
        '--hwtest', '--notests', '--novmtests', '--noimagetests',
        '--local', '--buildroot', '/buildroot',
        '--gerrit-patches', '123', '-g', '*123', '-g', '123..456',
        '--local-patches', 'chromiumos/chromite:tryjob', '-p', 'other:other',
        'lumpy-paladin', 'lumpy-release',
        '--passthrough', 'foo', '--cbuild-arg', '--b-arg', 'bar',
    ])
    options = self.cmd_mock.inst.options

    self.expected.update({
        'remote': False,
        'branch': None,
        'yes': True,
        'gerrit_patches': ['123', '*123', '123..456'],
        'local_patches': ['chromiumos/chromite:tryjob', 'other:other'],
        'passthrough': [
            '--latest-toolchain', '--nochromesdk',
            '--hwtest', '--notests', '--novmtests', '--noimagetests',
        ],
        'passthrough_raw': ['foo', '--cbuild-arg', '--b-arg', 'bar'],
        'build_configs': ['lumpy-paladin', 'lumpy-release'],
    })

    self.assertDictContainsSubset(self.expected, vars(options))

  def testPayloadsParsing(self):
    """Tests flow for an interactive session."""
    self.SetupCommandMock([
        '--version', '9795.0.0', '--channel', 'canary', 'lumpy-payloads'
    ])
    options = self.cmd_mock.inst.options

    self.expected.update({
        'passthrough': ['--version', '9795.0.0', '--channel', 'canary'],
        'build_configs': ['lumpy-payloads'],
    })

    self.assertDictContainsSubset(self.expected, vars(options))

  def testDashDashParsing(self):
    """Tests flow for an interactive session."""
    self.SetupCommandMock([
        '--passthrough', 'foo', '--cbuild-arg', '--b-arg', 'bar',
        '--', 'lumpy-paladin', 'lumpy-release',
    ])
    options = self.cmd_mock.inst.options

    self.expected.update({
        'passthrough_raw': ['foo', '--cbuild-arg', '--b-arg', 'bar'],
        'build_configs': ['lumpy-paladin', 'lumpy-release'],
    })

    self.assertDictContainsSubset(self.expected, vars(options))


class TryjobTestCbuildbotArgs(TryjobTest):
  """Test cros_tryjob.CbuildbotArgs."""

  def helperOptionsToCbuildbotArgs(self, cmd_line_args):
    """Convert cros tryjob arguments -> cbuildbot arguments.

    Does not do all intermediate steps, only for testing CbuildbotArgs.
    """
    self.SetupCommandMock(cmd_line_args)
    options = self.cmd_mock.inst.options
    return cros_tryjob.CbuildbotArgs(options)

  def testCbuildbotArgsMinimal(self):
    result = self.helperOptionsToCbuildbotArgs([
        'foo-build'])
    self.assertEqual(result, [
        '--remote-trybot'])

  def testCbuildbotArgsSimple(self):
    result = self.helperOptionsToCbuildbotArgs([
        '-g', '123', 'foo-build',
    ])
    self.assertEqual(result, [
        '--remote-trybot', '-g', '123',
    ])

  def testCbuildbotArgsComplex(self):
    result = self.helperOptionsToCbuildbotArgs([
        '--yes',
        '--latest-toolchain', '--nochromesdk',
        '--hwtest', '--notests', '--novmtests', '--noimagetests',
        '--local', '--buildroot', '/buildroot',
        '--gerrit-patches', '123', '-g', '*123', '-g', '123..456',
        '--committer-email', 'foo@bar',
        '--version', '1.2.3', '--channel', 'chan',
        'lumpy-paladin', 'lumpy-release',
        '--passthrough', 'foo', '--cbuild-arg', '--b-arg', 'bar',
    ])
    self.assertEqual(result, [
        '--remote-trybot',
        '-g', '123', '-g', '*123', '-g', '123..456',
        '--latest-toolchain', '--nochromesdk',
        '--hwtest', '--notests', '--novmtests', '--noimagetests',
        '--version', '1.2.3', '--channel', 'chan',
        'foo', '--cbuild-arg', '--b-arg', 'bar'
    ])

  def testCbuildbotArgsProduction(self):
    result = self.helperOptionsToCbuildbotArgs([
        '--production', 'foo-build',
    ])
    self.assertEqual(result, [
        '--buildbot',
    ])
