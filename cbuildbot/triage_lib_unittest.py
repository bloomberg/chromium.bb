#!/usr/bin/python
# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module that contains unittests for triage_lib module."""

from __future__ import print_function

import os
import sys
import unittest

import constants
sys.path.insert(0, constants.SOURCE_ROOT)

from chromite.cbuildbot import failures_lib
from chromite.cbuildbot import results_lib
from chromite.cbuildbot import triage_lib
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import patch as cros_patch
from chromite.lib import patch_unittest

import mock


# Some tests require the kernel, and fail with buildtools only repo.
KERNEL_AVAILABLE = os.path.exists(os.path.join(
    constants.SOURCE_ROOT, 'src', 'third_party', 'kernel'))


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

  @staticmethod
  def _GetBuildFailure(pkg):
    """Create a PackageBuildFailure for the specified |pkg|.

    Args:
      pkg: Package that failed to build.
    """
    ex = cros_build_lib.RunCommandError('foo', cros_build_lib.CommandResult())
    return failures_lib.PackageBuildFailure(ex, 'bar', [pkg])

  @staticmethod
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
    message = self.GetFailedMessage(all_exceptions, internal=internal)
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
        [self.GetFailedMessage([failures_lib.TestLabFailure()])
         for _ in range(lab_fail)])
    messages.extend(
        [self.GetFailedMessage([failures_lib.InfrastructureFailure()])
         for _ in range(infra_fail)])
    messages.extend(
        [self.GetFailedMessage(Exception())
         for _ in range(other_fail)])
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

    message = self.GetFailedMessage([Exception()])
    candidates = \
        triage_lib.CalculateSuspects.FilterOutInnocentOverlayChanges(
            constants.SOURCE_ROOT, changes, [message])
    self.assertEquals(candidates, changes[1:])


if __name__ == '__main__':
  cros_test_lib.main()
