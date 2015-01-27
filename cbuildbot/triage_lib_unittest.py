# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module that contains unittests for triage_lib module."""

from __future__ import print_function

import ConfigParser
import mock
import os
import unittest

from chromite.cbuildbot import constants
from chromite.cbuildbot import failures_lib
from chromite.cbuildbot import results_lib
from chromite.cbuildbot import triage_lib
from chromite.cbuildbot.stages import sync_stages_unittest
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import gerrit
from chromite.lib import osutils
from chromite.lib import patch as cros_patch
from chromite.lib import patch_unittest


# Some tests require the kernel, and fail with buildtools only repo.
KERNEL_AVAILABLE = os.path.exists(os.path.join(
    constants.SOURCE_ROOT, 'src', 'third_party', 'kernel'))


def GetFailedMessage(exceptions, stage='Build', internal=False,
                     bot='daisy_spring-paladin'):
  """Returns a BuildFailureMessage object."""
  tracebacks = []
  for ex in exceptions:
    tracebacks.append(results_lib.RecordedTraceback(stage, stage, ex,
                                                    str(ex)))
  reason = 'failure reason string'
  return failures_lib.BuildFailureMessage(
      'Stage %s failed' % stage, tracebacks, internal, reason, bot)


class TestFindSuspects(patch_unittest.MockPatchBase):
  """Tests CalculateSuspects."""

  def setUp(self):
    overlay = 'chromiumos/overlays/chromiumos-overlay'
    self.overlay_patch = self.GetPatches(project=overlay)
    chromite = 'chromiumos/chromite'
    self.chromite_patch = self.GetPatches(project=chromite)
    self.power_manager = 'chromiumos/platform2/power_manager'
    self.power_manager_pkg = 'chromeos-base/power_manager'
    self.power_manager_patch = self.GetPatches(project=self.power_manager)
    self.kernel = 'chromiumos/third_party/kernel'
    self.kernel_pkg = 'sys-kernel/chromeos-kernel'
    self.kernel_patch = self.GetPatches(project=self.kernel)
    self.secret = 'chromeos/secret'
    self.secret_patch = self.GetPatches(project=self.secret,
                                        remote=constants.INTERNAL_REMOTE)
    self.PatchObject(cros_patch.GitRepoPatch, 'GetCheckout')
    self.PatchObject(cros_patch.GitRepoPatch, 'GetDiffStatus')
    self.PatchObject(gerrit, 'GetGerritPatchInfoWithPatchQueries',
                     side_effect=lambda x: x)

  @staticmethod
  def _GetBuildFailure(pkg):
    """Create a PackageBuildFailure for the specified |pkg|.

    Args:
      pkg: Package that failed to build.
    """
    ex = cros_build_lib.RunCommandError('foo', cros_build_lib.CommandResult())
    return failures_lib.PackageBuildFailure(ex, 'bar', [pkg])

  def _AssertSuspects(self, patches, suspects, pkgs=(), exceptions=(),
                      internal=False, infra_fail=False, lab_fail=False):
    """Run _FindSuspects and verify its output.

    Args:
      patches: List of patches to look at.
      suspects: Expected list of suspects returned by _FindSuspects.
      pkgs: List of packages that failed with exceptions in the build.
      exceptions: List of other exceptions that occurred during the build.
      internal: Whether the failures occurred on an internal bot.
      infra_fail: Whether the build failed due to infrastructure issues.
      lab_fail: Whether the build failed due to lab infrastructure issues.
    """
    all_exceptions = list(exceptions) + [self._GetBuildFailure(x) for x in pkgs]
    message = GetFailedMessage(all_exceptions, internal=internal)
    results = triage_lib.CalculateSuspects.FindSuspects(
        patches, [message], lab_fail=lab_fail, infra_fail=infra_fail)
    self.assertEquals(set(suspects), results)

  @unittest.skipIf(not KERNEL_AVAILABLE, 'Full checkout is required.')
  def testFailSameProject(self):
    """Patches to the package that failed should be marked as failing."""
    suspects = [self.kernel_patch]
    patches = suspects + [self.power_manager_patch, self.secret_patch]
    self._AssertSuspects(patches, suspects, [self.kernel_pkg])

  @unittest.skipIf(not KERNEL_AVAILABLE, 'Full checkout is required.')
  def testFailSameProjectPlusOverlay(self):
    """Patches to the overlay should be marked as failing."""
    suspects = [self.overlay_patch, self.kernel_patch]
    patches = suspects + [self.power_manager_patch, self.secret_patch]
    self._AssertSuspects(patches, suspects, [self.kernel_pkg])

  def testFailUnknownPackage(self):
    """If no patches changed the package, all patches should fail."""
    changes = [self.overlay_patch, self.power_manager_patch, self.secret_patch]
    self._AssertSuspects(changes, changes, [self.kernel_pkg])

  def testFailUnknownException(self):
    """An unknown exception should cause all patches to fail."""
    changes = [self.kernel_patch, self.power_manager_patch, self.secret_patch]
    self._AssertSuspects(changes, changes, exceptions=[Exception('foo bar')])

  def testFailUnknownInternalException(self):
    """An unknown exception should cause all patches to fail."""
    suspects = [self.kernel_patch, self.power_manager_patch, self.secret_patch]
    self._AssertSuspects(suspects, suspects, exceptions=[Exception('foo bar')],
                         internal=True)

  def testFailUnknownCombo(self):
    """Unknown exceptions should cause all patches to fail.

    Even if there are also build failures that we can explain.
    """
    suspects = [self.kernel_patch, self.power_manager_patch, self.secret_patch]
    self._AssertSuspects(suspects, suspects, [self.kernel_pkg],
                         [Exception('foo bar')])

  def testFailNoExceptions(self):
    """If there are no exceptions, all patches should be failed."""
    suspects = [self.kernel_patch, self.power_manager_patch, self.secret_patch]
    self._AssertSuspects(suspects, suspects)

  def testLabFail(self):
    """If there are only lab failures, no suspect is chosen."""
    suspects = []
    changes = [self.kernel_patch, self.power_manager_patch]
    self._AssertSuspects(changes, suspects, lab_fail=True, infra_fail=True)

  def testInfraFail(self):
    """If there are only non-lab infra faliures, pick chromite changes."""
    suspects = [self.chromite_patch]
    changes = [self.kernel_patch, self.power_manager_patch] + suspects
    self._AssertSuspects(changes, suspects, lab_fail=False, infra_fail=True)

  def testManualBlame(self):
    """If there are changes that were manually blamed, pick those changes."""
    approvals1 = [{'type': 'VRIF', 'value': '-1', 'grantedOn': 1391733002},
                  {'type': 'CRVW', 'value': '2', 'grantedOn': 1391733002},
                  {'type': 'COMR', 'value': '1', 'grantedOn': 1391733002},]
    approvals2 = [{'type': 'VRIF', 'value': '1', 'grantedOn': 1391733002},
                  {'type': 'CRVW', 'value': '-2', 'grantedOn': 1391733002},
                  {'type': 'COMR', 'value': '1', 'grantedOn': 1391733002},]
    suspects = [self.MockPatch(approvals=approvals1),
                self.MockPatch(approvals=approvals2)]
    changes = [self.kernel_patch, self.chromite_patch] + suspects
    self._AssertSuspects(changes, suspects, lab_fail=False, infra_fail=False)
    self._AssertSuspects(changes, suspects, lab_fail=True, infra_fail=False)
    self._AssertSuspects(changes, suspects, lab_fail=True, infra_fail=True)
    self._AssertSuspects(changes, suspects, lab_fail=False, infra_fail=True)

  def _GetMessages(self, lab_fail=0, infra_fail=0, other_fail=0):
    """Returns a list of BuildFailureMessage objects."""
    messages = []
    messages.extend(
        [GetFailedMessage([failures_lib.TestLabFailure()])
         for _ in range(lab_fail)])
    messages.extend(
        [GetFailedMessage([failures_lib.InfrastructureFailure()])
         for _ in range(infra_fail)])
    messages.extend(
        [GetFailedMessage(Exception()) for _ in range(other_fail)])
    return messages

  def testOnlyLabFailures(self):
    """Tests the OnlyLabFailures function."""
    messages = self._GetMessages(lab_fail=2)
    no_stat = []
    self.assertTrue(
        triage_lib.CalculateSuspects.OnlyLabFailures(messages, no_stat))

    no_stat = ['foo', 'bar']
    # Some builders did not start. This is not a lab failure.
    self.assertFalse(
        triage_lib.CalculateSuspects.OnlyLabFailures(messages, no_stat))

    messages = self._GetMessages(lab_fail=1, infra_fail=1)
    no_stat = []
    # Non-lab infrastructure failures are present.
    self.assertFalse(
        triage_lib.CalculateSuspects.OnlyLabFailures(messages, no_stat))

  def testOnlyInfraFailures(self):
    """Tests the OnlyInfraFailures function."""
    messages = self._GetMessages(infra_fail=2)
    no_stat = []
    self.assertTrue(
        triage_lib.CalculateSuspects.OnlyInfraFailures(messages, no_stat))

    messages = self._GetMessages(lab_fail=2)
    no_stat = []
    # Lab failures are infrastructure failures.
    self.assertTrue(
        triage_lib.CalculateSuspects.OnlyInfraFailures(messages, no_stat))

    no_stat = ['orange']
    messages = []
    # 'Builders failed to report statuses' belong to infrastructure failures.
    self.assertTrue(
        triage_lib.CalculateSuspects.OnlyInfraFailures(messages, no_stat))

  def testSkipInnocentOverlayPatches(self):
    """Test that we don't blame innocent overlay patches."""
    changes = self.GetPatches(4)
    overlay_dir = os.path.join(constants.SOURCE_ROOT, 'src/overlays')
    m = mock.MagicMock()
    self.PatchObject(cros_patch.GitRepoPatch, 'GetCheckout', return_value=m)
    self.PatchObject(m, 'GetPath', return_value=overlay_dir)
    self.PatchObject(changes[0], 'GetDiffStatus',
                     return_value={'overlay-x86-generic/make.conf': 'M'})
    self.PatchObject(changes[1], 'GetDiffStatus',
                     return_value={'make.conf': 'M'})
    self.PatchObject(changes[2], 'GetDiffStatus',
                     return_value={'overlay-daisy/make.conf': 'M'})
    self.PatchObject(changes[3], 'GetDiffStatus',
                     return_value={'overlay-daisy_spring/make.conf': 'M'})

    message = GetFailedMessage([Exception()])
    candidates = (
        triage_lib.CalculateSuspects.FilterOutInnocentOverlayChanges(
            constants.SOURCE_ROOT, changes, [message]))
    self.assertEquals(candidates, changes[1:])


class TestGetFullyVerifiedChanges(patch_unittest.MockPatchBase):
  """Tests GetFullyVerifiedChanges() and related functions."""

  def setUp(self):
    self.build_root = '/foo/build/root'
    self.changes = self.GetPatches(how_many=5)

  def testChangesNoAllTested(self):
    """Tests that those changes are fully verified."""
    no_stat = failing = messages = []
    inflight = ['foo-paladin']
    changes_by_config = {'foo-paladin': []}

    verified = triage_lib.CalculateSuspects.GetFullyVerifiedChanges(
        self.changes, changes_by_config, failing, inflight, no_stat,
        messages, self.build_root)

    self.assertEquals(verified, set(self.changes))

  def testChangesNotVerified(self):
    """Tests that changes are not verified if builds failed prematurely."""
    failing = messages = []
    inflight = ['foo-paladin']
    no_stat = ['puppy-paladin']
    changes_by_config = {'foo-paladin': set(self.changes[:2]),
                         'bar-paladin': set(self.changes),
                         'puppy-paladin': set(self.changes[-2:])}

    verified = triage_lib.CalculateSuspects.GetFullyVerifiedChanges(
        self.changes, changes_by_config, failing, inflight, no_stat,
        messages, self.build_root)
    self.assertEquals(verified, set(self.changes[2:-2]))

  def testChangesNotVerifiedOnFailures(self):
    """Tests that changes are not verified if failures cannot be ignored."""
    messages = no_stat = inflight = []
    failing = ['cub-paladin']
    changes_by_config = {'bar-paladin': set(self.changes),
                         'cub-paladin': set(self.changes[:2])}

    self.PatchObject(
        triage_lib.CalculateSuspects, '_CanIgnoreFailures', return_value=False)
    verified = triage_lib.CalculateSuspects.GetFullyVerifiedChanges(
        self.changes, changes_by_config, failing, inflight, no_stat,
        messages, self.build_root)
    self.assertEquals(verified, set(self.changes[2:]))

  def testChangesVerifiedWhenFailuresCanBeIgnored(self):
    """Tests that changes are verified if failures can be ignored."""
    messages = no_stat = inflight = []
    failing = ['cub-paladin']
    changes_by_config = {'bar-paladin': set(self.changes),
                         'cub-paladin': set(self.changes[:2])}

    self.PatchObject(
        triage_lib.CalculateSuspects, '_CanIgnoreFailures', return_value=True)
    verified = triage_lib.CalculateSuspects.GetFullyVerifiedChanges(
        self.changes, changes_by_config, failing, inflight, no_stat,
        messages, self.build_root)
    self.assertEquals(verified, set(self.changes))

  def testCanIgnoreFailures(self):
    """Tests _CanIgnoreFailures()."""
    # pylint: disable=protected-access
    change = self.changes[0]
    messages = [GetFailedMessage([Exception()], stage='HWTest'),
                GetFailedMessage([Exception()], stage='VMTest'),]
    m = self.PatchObject(triage_lib, 'GetStagesToIgnoreForChange')

    m.return_value = ('HWTest',)
    self.assertFalse(triage_lib.CalculateSuspects._CanIgnoreFailures(
        messages, change, self.build_root))

    m.return_value = ('HWTest', 'VMTest', 'Foo')
    self.assertTrue(triage_lib.CalculateSuspects._CanIgnoreFailures(
        messages, change, self.build_root))

    m.return_value = None
    self.assertFalse(triage_lib.CalculateSuspects._CanIgnoreFailures(
        messages, change, self.build_root))


class IgnoredStagesTest(patch_unittest.MockPatchBase):
  """Tests for functions that calculate what stages to ignore."""

  def GetOption(self, path, section='GENERAL', option='ignored-stages'):
    # pylint: disable=protected-access
    return triage_lib._GetOptionFromConfigFile(path, section, option)

  def testBadConfigFile(self):
    """Test if we can handle an incorrectly formatted config file."""
    with osutils.TempDir(set_global=True) as tempdir:
      path = os.path.join(tempdir, 'foo.ini')
      osutils.WriteFile(path, 'foobar')
      self.assertRaises(ConfigParser.Error, self.GetOption, path)

  def testMissingConfigFile(self):
    """Test if we can handle a missing config file."""
    with osutils.TempDir(set_global=True) as tempdir:
      path = os.path.join(tempdir, 'foo.ini')
      self.assertEqual(None, self.GetOption(path))

  def testGoodConfigFile(self):
    """Test if we can handle a good config file."""
    with osutils.TempDir(set_global=True) as tempdir:
      path = os.path.join(tempdir, 'foo.ini')
      osutils.WriteFile(path, '[GENERAL]\nignored-stages: bar baz\n')
      ignored = self.GetOption(path)
      self.assertEqual('bar baz', ignored)


class ConfigFileTest(cros_test_lib.MockTestCase):
  """Tests for functions that read config information for a patch."""
  # pylint: disable=protected-access

  def _GetPatch(self, affected_files):
    return sync_stages_unittest.MockPatch(
        mock_diff_status={path: 'M' for path in affected_files})

  def testAffectedSubdir(self):
    p = self._GetPatch(['a', 'b', 'c'])
    self.assertEqual(triage_lib._GetCommonAffectedSubdir(p, '/a/b'),
                     '/a/b')

    p = self._GetPatch(['a/a', 'a/b', 'a/c'])
    self.assertEqual(triage_lib._GetCommonAffectedSubdir(p, '/a/b'),
                     '/a/b/a')

    p = self._GetPatch(['a/a', 'a/b', 'a/c'])
    self.assertEqual(triage_lib._GetCommonAffectedSubdir(p, '/a/b'),
                     '/a/b/a')

  def testGetConfigFile(self):
    p = self._GetPatch(['a/a', 'a/b', 'a/c'])
    self.PatchObject(os.path, 'isfile', return_value=True)
    self.assertEqual(triage_lib._GetConfigFileForChange(p, '/a/b'),
                     '/a/b/a/COMMIT-QUEUE.ini')
    self.assertEqual(triage_lib._GetConfigFileForChange(p, '/a/b/'),
                     '/a/b/a/COMMIT-QUEUE.ini')


    self.PatchObject(os.path, 'isfile', return_value=False)
    self.assertEqual(triage_lib._GetConfigFileForChange(p, '/a/b'),
                     '/a/b/COMMIT-QUEUE.ini')
    self.assertEqual(triage_lib._GetConfigFileForChange(p, '/a/b/'),
                     '/a/b/COMMIT-QUEUE.ini')
