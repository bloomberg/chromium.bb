# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module that helps to triage Commit Queue failures."""

from __future__ import print_function

import logging
import os

from chromite.cbuildbot import cbuildbot_config
from chromite.cbuildbot import failures_lib
from chromite.cbuildbot import constants
from chromite.lib import git
from chromite.lib import patch as cros_patch
from chromite.lib import portage_util


def GetRelevantOverlaysForConfig(config, build_root):
  """Returns a list of overlays relevant to |config|.

  Args:
    config: A cbuildbot config name.
    build_root: Path to the build root.

  Returns:
    A set of overlays.
  """
  relevant_overlays = set()
  for board in config.boards:
    overlays = portage_util.FindOverlays(
      constants.BOTH_OVERLAYS, board, build_root)
    relevant_overlays.update(overlays)

  return relevant_overlays


def GetAffectedOverlays(change, manifest, all_overlays):
  """Get the set of overlays affected by a given change.

  Args:
    change: The GerritPatch instance to look at.
    manifest: A ManifestCheckout instance representing our build directory.
    all_overlays: The set of all valid overlays.

  Returns:
    The set of overlays affected by the specified |change|. If the change
    affected something other than an overlay, return None.
  """
  checkout = change.GetCheckout(manifest, strict=False)
  if checkout:
    git_repo = checkout.GetPath(absolute=True)

    # The whole git repo is an overlay. Return it.
    # Example: src/private-overlays/overlay-x86-zgb-private
    if git_repo in all_overlays:
      return set([git_repo])

    # Get the set of immediate subdirs affected by the change.
    # Example: src/overlays/overlay-x86-zgb
    subdirs = set([os.path.join(git_repo, path.split(os.path.sep)[0])
                   for path in change.GetDiffStatus(git_repo)])

    # If all of the subdirs are overlays, return them.
    if subdirs.issubset(all_overlays):
      return subdirs


class CategorizeChanges(object):
  """A collection of methods to help categorize GerritPatch changes."""

  @classmethod
  def ClassifyOverlayChanges(cls, changes, config, build_root, manifest):
    """Classifies overlay changes in |changes|.

    Args:
      changes: The list or set of GerritPatch instances.
      config: The cbuildbot config.
      build_root: Path to the build root.
      manifest: A ManifestCheckout instance representing our build directory.

    Returns:
      A (overlay_changes, irrelevant_overlay_changes) tuple; overlay_changes
      is a subset of |changes| that have modified one or more overlays, and
      irrelevant_overlay_changes is a subset of overlay_changes which are
      irrelevant to |config|.
    """
    visible_overlays = set(portage_util.FindOverlays(config.overlays, None,
                                                     build_root))
    # The overlays relevant to this build.
    relevant_overlays = GetRelevantOverlaysForConfig(config, build_root)

    overlay_changes = set()
    irrelevant_overlay_changes = set()
    for change in changes:
      affected_overlays = GetAffectedOverlays(change, manifest,
                                              visible_overlays)
      if affected_overlays is not None:
        # The change modifies an overlay.
        overlay_changes.add(change)
        if not any(x in relevant_overlays for x in affected_overlays):
          # The change touched an irrelevant overlay.
          irrelevant_overlay_changes.add(change)

    return overlay_changes, irrelevant_overlay_changes

  @classmethod
  def GetIrrelevantChanges(cls, changes, config, build_root, manifest):
    """Determine changes irrelavant to build |config|.

    This method determine a set of changes that are irrelevant to the
    build |config|. The general rule of thumb is that if we are unsure
    whether a change is relevant, consider it relevant.

    Args:
      changes: The list or set of GerritPatch instances.
      config: The cbuildbot config.
      build_root: Path to the build root.
      manifest: A ManifestCheckout instance representing our build directory.

    Returns:
      A subset of |changes| which are irrelevant to |config|.
    """
    untriaged_changes = set(changes)
    irrelevant_changes = set()

    # Handles overlay changes.
    # ClassifyOverlayChanges only handles overlays visible to this
    # build. For example, an external build may not be able to view
    # the internal overlays. In those cases, the changes have been
    # filtered out by the previous step.
    overlay_changes, irrelevant_overlay_changes = cls.ClassifyOverlayChanges(
        untriaged_changes, config, build_root, manifest)
    untriaged_changes -= overlay_changes
    irrelevant_changes |= irrelevant_overlay_changes

    return irrelevant_changes


class CalculateSuspects(object):
  """Diagnose the cause for a given set of failures."""

  @classmethod
  def GetBlamedChanges(cls, changes):
    """Returns the changes that have been manually blamed.

    Args:
      changes: List of GerritPatch changes.

    Returns:
      A list of |changes| that were marked verified: -1 or
      code-review: -2.
    """
    return [x for x in changes if
            any(x.HasApproval(f, v) for f, v in
                constants.DEFAULT_CQ_SHOULD_REJECT_FIELDS.iteritems())]

  @classmethod
  def _FindPackageBuildFailureSuspects(cls, changes, messages):
    """Figure out what CLs are at fault for a set of build failures.

    Args:
        changes: A list of cros_patch.GerritPatch instances to consider.
        messages: A list of build failure messages, of type
                  BuildFailureMessage.
    """
    suspects = set()
    for message in messages:
      suspects.update(message.FindPackageBuildFailureSuspects(changes))
    return suspects

  @classmethod
  def FilterChromiteChanges(cls, changes):
    """Returns a list of chromite changes in |changes|."""
    return [x for x in changes if x.project == constants.CHROMITE_PROJECT]

  @classmethod
  def _MatchesFailureType(cls, messages, fail_type, strict=True):
    """Returns True if all failures are instances of |fail_type|.

    Args:
      messages: A list of BuildFailureMessage or NoneType objects
        from the failed slaves.
      fail_type: The exception class to look for.
      strict: If False, treat NoneType message as a match.

    Returns:
      True if all objects in |messages| are non-None and all failures are
      instances of |fail_type|.
    """
    return ((not strict or all(messages)) and
            all(x.MatchesFailureType(fail_type) for x in messages if x))

  @classmethod
  def OnlyLabFailures(cls, messages, no_stat):
    """Determine if the cause of build failure was lab failure.

    Args:
      messages: A list of BuildFailureMessage or NoneType objects
        from the failed slaves.
      no_stat: A list of builders which failed prematurely without reporting
        status.

    Returns:
      True if the build failed purely due to lab failures.
    """
    # If any builder failed prematuely, lab failure was not the only cause.
    return (not no_stat and
            cls._MatchesFailureType(messages, failures_lib.TestLabFailure))

  @classmethod
  def OnlyInfraFailures(cls, messages, no_stat):
    """Determine if the cause of build failure was infrastructure failure.

    Args:
      messages: A list of BuildFailureMessage or NoneType objects
        from the failed slaves.
      no_stat: A list of builders which failed prematurely without reporting
        status.

    Returns:
      True if the build failed purely due to infrastructure failures.
    """
    # "Failed to report status" and "NoneType" messages are considered
    # infra failures.
    return ((not messages and no_stat) or
            cls._MatchesFailureType(
                messages, failures_lib.InfrastructureFailure, strict=False))

  @classmethod
  def FindSuspects(cls, changes, messages, infra_fail=False, lab_fail=False):
    """Find out what changes probably caused our failure.

    In cases where there were no internal failures, we can assume that the
    external failures are at fault. Otherwise, this function just defers to
    _FindPackageBuildFailureSuspects and FindPreviouslyFailedChanges as needed.
    If the failures don't match either case, just fail everything.

    Args:
      changes: A list of cros_patch.GerritPatch instances to consider.
      messages: A list of build failure messages, of type
        BuildFailureMessage or of type NoneType.
      infra_fail: The build failed purely due to infrastructure failures.
      lab_fail: The build failed purely due to test lab infrastructure
        failures.

    Returns:
       A set of changes as suspects.
    """
    bad_changes = cls.GetBlamedChanges(changes)
    if bad_changes:
      # If there are changes that have been set verified=-1 or
      # code-review=-2, these changes are the ONLY suspects of the
      # failed build.
      logging.warning('Detected that some changes have been blamed for '
                      'the build failure. Only these CLs will be rejected: %s',
                      cros_patch.GetChangesAsString(bad_changes))
      return set(bad_changes)
    elif lab_fail:
      logging.warning('Detected that the build failed purely due to HW '
                      'Test Lab failure(s). Will not reject any changes')
      return set()
    elif not lab_fail and infra_fail:
      # The non-lab infrastructure errors might have been caused
      # by chromite changes.
      logging.warning(
          'Detected that the build failed due to non-lab infrastructure '
          'issue(s). Will only reject chromite changes')
      return set(cls.FilterChromiteChanges(changes))

    if all(message and message.IsPackageBuildFailure()
           for message in messages):
      # If we are here, there are no None messages.
      suspects = cls._FindPackageBuildFailureSuspects(changes, messages)
    else:
      suspects = set(changes)

    return suspects

  @classmethod
  def GetResponsibleOverlays(cls, build_root, messages):
    """Get the set of overlays that could have caused failures.

    This loops through the set of builders that failed in a given run and
    finds what overlays could have been responsible for the failure.

    Args:
      build_root: Build root directory.
      messages: A list of build failure messages from supporting builders.
        These must be BuildFailureMessage objects or NoneType objects.

    Returns:
      The set of overlays that could have caused the failures. If we can't
      determine what overlays are responsible, returns None.
    """
    responsible_overlays = set()
    for message in messages:
      if message is None:
        return None
      bot_id = message.builder
      config = cbuildbot_config.config.get(bot_id)
      if not config:
        return None
      responsible_overlays.update(
          GetRelevantOverlaysForConfig(config, build_root))

    return responsible_overlays

  @classmethod
  def FilterOutInnocentChanges(cls, build_root, changes, messages):
    """Filter out innocent changes based on failure messages.

    Args:
      build_root: Build root directory.
      changes: GitRepoPatches that might be guilty.
      messages: A list of build failure messages from supporting builders.
        These must be BuildFailureMessage objects or NoneType objects.

    Returns:
      A list of the changes that we could not prove innocent.
    """
    # If there were no internal failures, only kick out external changes.
    # (Still, fail all changes if we received any None messages.)
    candidates = changes
    if all(messages) and not any(message.internal for message in messages):
      candidates = [change for change in changes if not change.internal]
    return cls.FilterOutInnocentOverlayChanges(build_root, candidates, messages)

  @classmethod
  def FilterOutInnocentOverlayChanges(cls, build_root, changes, messages):
    """Filter out innocent overlay changes based on failure messages.

    It is not possible to break a x86-generic builder via a change to an
    unrelated overlay (e.g. amd64-generic). Filter out changes that are
    known to be innocent.

    Args:
      build_root: Build root directory.
      changes: GitRepoPatches that might be guilty.
      messages: A list of build failure messages from supporting builders.
        These must be BuildFailureMessage objects or NoneType objects.

    Returns:
      A list of the changes that we could not prove innocent.
    """
    all_overlays = set(portage_util.FindOverlays(
        constants.BOTH_OVERLAYS, None, build_root))
    responsible_overlays = cls.GetResponsibleOverlays(build_root, messages)
    if responsible_overlays is None:
      return changes
    manifest = git.ManifestCheckout.Cached(build_root)
    candidates = []
    for change in changes:
      overlays = GetAffectedOverlays(change, manifest, all_overlays)
      if overlays is None or overlays.issubset(responsible_overlays):
        candidates.append(change)
    return candidates
