# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module that helps to triage Commit Queue failures."""

from __future__ import print_function

import ConfigParser
import glob
import os
import pprint
import re

from chromite.cbuildbot import failures_lib
from chromite.cbuildbot import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import gerrit
from chromite.lib import git
from chromite.lib import osutils
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


def _GetAffectedImmediateSubdirs(change, git_repo):
  """Gets the set of immediate subdirs affected by |change|.

  Args:
    change: GitRepoPatch to examine.
    git_repo: Path to checkout of git repository.

  Returns:
    A set of absolute paths to modified subdirectories of |git_repo|.
  """
  return set([os.path.join(git_repo, path.split(os.path.sep)[0])
              for path in change.GetDiffStatus(git_repo)])


def _GetCommonAffectedSubdir(change, git_repo):
  """Gets the longest common path of changes in |change|.

  Args:
    change: GitRepoPatch to examine.
    git_repo: Path to checkout of git repository.

  Returns:
    An absolute path in |git_repo|.
  """
  affected_paths = [os.path.join(git_repo, path)
                    for path in change.GetDiffStatus(git_repo)]
  return cros_build_lib.GetCommonPathPrefix(affected_paths)


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
    subdirs = _GetAffectedImmediateSubdirs(change, git_repo)

    # If all of the subdirs are overlays, return them.
    if subdirs.issubset(all_overlays):
      return subdirs


def GetAffectedPackagesForOverlayChange(change, manifest, overlays):
  """Get the set of packages affected by the overlay |change|.

  Args:
    change: The GerritPatch instance that modifies an overlay.
    manifest: A ManifestCheckout instance representing our build directory.
    overlays: List of overlay paths.

  Returns:
    The set of packages affected by the specified |change|. E.g.
    {'chromeos-base/chromite-0.0.1-r1258'}. If the change affects
    something other than packages, return None.
  """
  checkout = change.GetCheckout(manifest, strict=False)
  if checkout:
    git_repo = checkout.GetPath(absolute=True)

  packages = set()
  for path in change.GetDiffStatus(git_repo):
    # Determine if path is in a package directory by walking up
    # directories and see if there is an ebuild in the directory.
    start_path = os.path.join(git_repo, path)
    ebuild_path = osutils.FindInPathParents(
        '*.ebuild', start_path, test_func=glob.glob, end_path=git_repo)
    if ebuild_path:
      # Convert git_repo/../*.ebuild to the real ebuild path.
      ebuild_path = glob.glob(ebuild_path)[0]
      # Double check that the ebuild is two-levels deep in an overlay
      # directory.
      if os.path.sep.join(ebuild_path.split(os.path.sep)[:-3]) in overlays:
        category, pkg_name, _ = portage_util.SplitEbuildPath(ebuild_path)
        packages.add('%s/%s' % (category, pkg_name))
        continue

    # If |change| affects anything other than packages, return None.
    return None

  return packages


def _GetOptionFromConfigFile(config_path, section, option):
  """Get |option| from |section| in |config_path|.

  Args:
    config_path: Filename to look at.
    section: Section header name.
    option: Option name.

  Returns:
    The value of the option.
  """
  parser = ConfigParser.SafeConfigParser()
  parser.read(config_path)
  if parser.has_option(section, option):
    return parser.get(section, option)


def _GetConfigFileForChange(change, checkout_path):
  """Gets the path of the config file for |change|.

  This function takes into account the files that are modified by |change| to
  determine the commit queue config file within |checkout_path| that should be
  used for this change. The config file used is the one in the common ancestor
  directory to all changed files, or the nearest parent directory. See
  http://chromium.org/chromium-os/build/bypassing-tests-on-a-per-project-basis

  Args:
    change: Change to examine, as a GitRepoPatch object.
    checkout_path: Full absolute path to a checkout of the repository that
                   |change| applies to.

  Returns:
    Path to the config file to be read for |change|. The returned path will
    be within |checkout_path|. If no config files in common subdirectories
    were found, a config file path in the root of the checkout will be
    returned, in which case the file is not guaranteed to exist.
  """
  current_dir = _GetCommonAffectedSubdir(change, checkout_path)
  while True:
    config_file = os.path.join(current_dir, constants.CQ_CONFIG_FILENAME)
    if os.path.isfile(config_file) or checkout_path.startswith(current_dir):
      return config_file
    assert current_dir not in ('/', '')
    current_dir = os.path.dirname(current_dir)


def GetOptionForChange(build_root, change, section, option):
  """Get |option| from |section| in the config file for |change|.

  Args:
    build_root: The root of the checkout.
    change: Change to examine, as a GitRepoPatch object.
    section: Section header name.
    option: Option name.

  Returns:
    The value of the option.
  """
  manifest = git.ManifestCheckout.Cached(build_root)
  checkout = change.GetCheckout(manifest)
  if checkout:
    dirname = checkout.GetPath(absolute=True)
    config_path = _GetConfigFileForChange(change, dirname)
    result = None
    try:
      result = _GetOptionFromConfigFile(config_path, section, option)
    except ConfigParser.Error:
      logging.error('%s has malformed config file', change, exc_info=True)
    return result


def GetStagesToIgnoreForChange(build_root, change):
  """Get a list of stages that the CQ should ignore for a given |change|.

  The list of stage name prefixes to ignore for each project is specified in a
  config file inside the project, named COMMIT-QUEUE.ini. The file would look
  like this:

  [GENERAL]
    ignored-stages: HWTest VMTest

  The CQ will submit changes to the given project even if the listed stages
  failed. These strings are stage name prefixes, meaning that "HWTest" would
  match any HWTest stage (e.g. "HWTest [bvt]" or "HWTest [foo]")

  Args:
    build_root: The root of the checkout.
    change: Change to examine, as a PatchQuery object.

  Returns:
    A list of stages to ignore for the given |change|.
  """
  result = GetOptionForChange(build_root, change, 'GENERAL', 'ignored-stages')
  return result.split() if result else []


def GetTestSubsystemForChange(build_root, change):
  """Get a list of subsystem that a given |change| affects.

  If subsystem is specified in the commit message, use that. Otherwise, look in
  appropriate COMMIT-QUEUE.ini. If subsystem is not specified anywhere,
  'subsystem:default' will be used.

  Based on the subsystems a given |change| affects, the CQ could tell whether a
  failure is potentially caused by this |change|. The CQ could then submit some
  changes in the face of unrelated failures.

  Args:
    build_root: The root of the checkout.
    change: Change to examine, as a PatchQuery object.

  Returns:
    A list of subsystem for the given |change|.
  """
  subsystems = []
  if change.commit_message:
    lines = cros_patch.GetOptionLinesFromCommitMessage(
        change.commit_message, 'subsystem:')
    if lines:
      subsystems = [x for x in re.split("[, ]", ' '.join(lines)) if x]
  if not subsystems:
    result = GetOptionForChange(build_root, change, 'GENERAL', 'subsystem')
    subsystems = result.split() if result else []
  return subsystems if subsystems else ['default']

class CategorizeChanges(object):
  """A collection of methods to help categorize GerritPatch changes.

  This class is mainly used on a build slave to categorize changes
  applied in the build.
  """

  @classmethod
  def ClassifyOverlayChanges(cls, changes, config, build_root, manifest,
                             packages_under_test):
    """Classifies overlay changes in |changes|.

    Args:
      changes: The list or set of GerritPatch instances.
      config: The cbuildbot config.
      build_root: Path to the build root.
      manifest: A ManifestCheckout instance representing our build directory.
      packages_under_test: A list of packages names included in the build
        without version/revision (e.g. ['chromeos-base/chromite']). If None,
        don't try to map overlay changes to packages.

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
          continue

        if packages_under_test:
          # If the change modifies packages that are not part of this
          # build, they are considered irrelevant too.
          packages = GetAffectedPackagesForOverlayChange(
              change, manifest, visible_overlays)
          if packages:
            logging.info('%s affects packages %s',
                         cros_patch.GetChangesAsString([change]),
                         ', '.join(packages))
            if not any(x in packages_under_test for x in packages):
              irrelevant_overlay_changes.add(change)

    return overlay_changes, irrelevant_overlay_changes

  @classmethod
  def ClassifyWorkOnChanges(cls, changes, config, build_root,
                            manifest, packages_under_test):
    """Classifies WorkOn package changes in |changes|.

    Args:
      changes: The list or set of GerritPatch instances.
      config: The cbuildbot config.
      build_root: Path to the build root.
      manifest: A ManifestCheckout instance representing our build directory.
      packages_under_test: A list of packages names included in the build.
        (e.g. ['chromeos-base/chromite-0.0.1-r1258']).

    Returns:
      A (workon_changes, irrelevant_workon_changes) tuple; workon_changes
      is a subset of |changes| that have modified workon packages, and
      irrelevant_workon_changes is a subset of workon_changes which are
      irrelevant to |config|.
    """
    workon_changes = set()
    irrelevant_workon_changes = set()

    workon_dict = portage_util.BuildFullWorkonPackageDictionary(
        build_root, config.overlays, manifest)

    pp = pprint.PrettyPrinter(indent=2)
    logging.info('(project, branch) to workon package mapping:\n %s',
                 pp.pformat(workon_dict))
    logging.info('packages under test\n: %s', pp.pformat(packages_under_test))

    for change in changes:
      packages = workon_dict.get((change.project, change.tracking_branch))
      if packages:
        # The CL modifies a workon package.
        workon_changes.add(change)
        if all(x not in packages_under_test for x in packages):
          irrelevant_workon_changes.add(change)

    return workon_changes, irrelevant_workon_changes

  @classmethod
  def _FilterProjectsInManifestByGroup(cls, manifest, groups):
    """Filters projects in |manifest| by |groups|.

    Args:
      manifest: A git.Manifest instance.
      groups: A list of groups to filter.

    Returns:
      A set of (project, branch) tuples where each tuple is asssociated
      with at least one group in |groups|.
    """
    results = set()
    for project, checkout_list in manifest.checkouts_by_name.iteritems():
      for checkout in checkout_list:
        if any(x in checkout['groups'] for x in groups):
          branch = git.StripRefs(checkout['tracking_branch'])
          results.add((project, branch))

    return results

  @classmethod
  def GetChangesToBuildTools(cls, changes, manifest):
    """Returns a changes associated with buildtools projects.

    Args:
      changes: The list or set of GerritPatch instances.
      manifest: A git.Manifest instance.

    Returns:
      A subset of |changes| to projects of "buildtools" group.
    """
    buildtool_set = cls._FilterProjectsInManifestByGroup(
        manifest, ['buildtools'])
    return set([x for x in changes if (x.project, x.tracking_branch)
                in buildtool_set])

  @classmethod
  def GetIrrelevantChanges(cls, changes, config, build_root, manifest,
                           packages_under_test):
    """Determine changes irrelavant to build |config|.

    This method determine a set of changes that are irrelevant to the
    build |config|. The general rule of thumb is that if we are unsure
    whether a change is relevant, consider it relevant.

    Args:
      changes: The list or set of GerritPatch instances.
      config: The cbuildbot config.
      build_root: Path to the build root.
      manifest: A ManifestCheckout instance representing our build directory.
      packages_under_test: A list of packages that were tested in this build.

    Returns:
      A subset of |changes| which are irrelevant to |config|.
    """
    untriaged_changes = set(changes)
    irrelevant_changes = set()

    # Changes that modify projects used in building are always relevant.
    untriaged_changes -= cls.GetChangesToBuildTools(changes, manifest)

    if packages_under_test is not None:
      # Strip the version of the package in packages_under_test.
      cpv_list = [portage_util.SplitCPV(x) for x in packages_under_test]
      packages_under_test = ['%s/%s' % (x.category, x.package) for x in
                             cpv_list]

    # Handles overlay changes.
    # ClassifyOverlayChanges only handles overlays visible to this
    # build. For example, an external build may not be able to view
    # the internal overlays. However, in that case, the internal changes
    # have already been filtered out in CommitQueueSyncStage, and are
    # not included in |changes|.
    overlay_changes, irrelevant_overlay_changes = cls.ClassifyOverlayChanges(
        untriaged_changes, config, build_root, manifest, packages_under_test)
    untriaged_changes -= overlay_changes
    irrelevant_changes |= irrelevant_overlay_changes

    # Handles workon package changes.
    if packages_under_test is not None:
      try:
        workon_changes, irrelevant_workon_changes = cls.ClassifyWorkOnChanges(
            untriaged_changes, config, build_root, manifest,
            packages_under_test)
      except Exception as e:
        # Ignore the exception if we cannot categorize workon
        # changes. We will conservatively assume the changes are
        # relevant.
        logging.warning('Unable to categorize cros workon changes: %s', e)
      else:
        untriaged_changes -= workon_changes
        irrelevant_changes |= irrelevant_workon_changes

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
    # Load the latest info about whether the changes were vetoed, in case they
    # were vetoed in the middle of a cbuildbot run. That said, be careful not to
    # return info about newer patchsets.
    reloaded_changes = gerrit.GetGerritPatchInfoWithPatchQueries(changes)
    return [x for x, y in zip(changes, reloaded_changes) if y.WasVetoed()]

  @classmethod
  def _FindPackageBuildFailureSuspects(cls, changes, messages, sanity):
    """Figure out what CLs are at fault for a set of build failures.

    Args:
        changes: A list of cros_patch.GerritPatch instances to consider.
        messages: A list of failure messages. We will only look at the ones of
                  type BuildFailureMessage.
        sanity: The sanity checker builder passed and the tree was open when
                the build started.
    """
    suspects = set()
    for message in messages:
      if message:
        suspects.update(
            message.FindPackageBuildFailureSuspects(changes, sanity))
      elif sanity:
        suspects.update(changes)
    return suspects

  @classmethod
  def FilterChangesForInfraFail(cls, changes):
    """Returns a list of changes responsible for infra failures."""
    # Chromite changes could cause infra failures.
    return [x for x in changes if x.project in constants.INFRA_PROJECTS]

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
  def FindSuspects(cls, changes, messages, infra_fail=False, lab_fail=False,
                   sanity=True):
    """Find out what changes probably caused our failure.

    In cases where there were no internal failures, we can assume that the
    external failures are at fault. Otherwise, this function just defers to
    _FindPackageBuildFailureSuspects and GetBlamedChanges as needed.
    If the failures don't match either case, just fail everything.

    Args:
      changes: A list of cros_patch.GerritPatch instances to consider.
      messages: A list of build failure messages, of type
        BuildFailureMessage or of type NoneType.
      infra_fail: The build failed purely due to infrastructure failures.
      lab_fail: The build failed purely due to test lab infrastructure
        failures.
      sanity: The sanity checker builder passed and the tree was open when
              the build started.

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
    elif infra_fail:
      # The non-lab infrastructure errors might have been caused
      # by chromite changes.
      logging.warning(
          'Detected that the build failed due to non-lab infrastructure '
          'issue(s). Will only reject chromite changes')
      return set(cls.FilterChangesForInfraFail(changes))

    return cls._FindPackageBuildFailureSuspects(changes, messages, sanity)

  @classmethod
  def _CanIgnoreFailures(cls, messages, change, build_root,
                         subsys_by_config):
    """Examine whether we can ignore the failures for |change|.

    First, examine the |messages| to see if we are allowed to ignore
    the failures base on the per-repository settings in COMMIT_QUEUE.ini.
    If not, then check whether only the HWTestStage failed, and the failed
    subsystems are unrelated to this change.

    Args:
      messages: A list of BuildFailureMessage from the failed slaves.
      change: A GerritPatch instance to examine.
      build_root: Build root directory.
      subsys_by_config: A dictionary of pass/fail HWTest subsystems indexed
        by the config names.

    Returns:
      A tuple, first element is True/False indicates whether we can ignore the
      failures; second element is the reason to ignore the failures, None if
      failures cannot be ignored.
    """
    # Some repositories may opt to ignore certain stage failures.
    failing_stages = set()
    if any(x.GetFailingStages() is None for x in messages):
      # If there are no tracebacks, that means that the builder
      # did not report its status properly. We don't know what
      # stages failed and cannot safely ignore any stage.
      return (False, None)

    for message in messages:
      failing_stages.update(message.GetFailingStages())
    ignored_stages = GetStagesToIgnoreForChange(build_root, change)
    if ignored_stages and failing_stages.issubset(ignored_stages):
      return (True, constants.STRATEGY_CQ_PARTIAL)

    # Among the failed stages, except the stages that can be ignored, only
    # HWTestStage fails, and subsystem logic has been used on all configs
    # failed at HWTest, and failed subsystems of each config are unrelated to
    # the current cl, we submit the cl.
    if not subsys_by_config:
      return (False, None)
    relevant_failing_stages = failing_stages - set(ignored_stages)
    if relevant_failing_stages == set(['HWTest']):
      configs_failed_hwtest = set([m.builder for m in messages
                                   if 'HWTest' in m.GetFailingStages()])
      subsys_result_dicts = ([v for k, v in subsys_by_config.iteritems()
                              if k in configs_failed_hwtest])
      if len(configs_failed_hwtest) != len(subsys_result_dicts):
        logging.warning('There exists malformed config names, which results in '
                        'mismatch between build configs. Cannot decide '
                        'whether change is innocent to failures. Will not '
                        'submit')
        return (False, None)
      # Empty dict indicates subsystem logic not used, make sure subsystem logic
      # is used in all the configs failed at hwtest.
      if all(subsys_result_dicts):
        all_fail_subsys, all_pass_subsys = set(), set()
        for v in subsys_result_dicts:
          all_fail_subsys |= set(v.get('fail_subsystems', []))
          all_pass_subsys |= set(v.get('pass_subsystems', []))
        all_pass_subsys -= all_fail_subsys
        cl_subsys = set(GetTestSubsystemForChange(build_root, change))
        if (not cl_subsys & all_fail_subsys) and (cl_subsys <= all_pass_subsys):
          return (True, constants.STRATEGY_CQ_PARTIAL_SUBSYSTEM)

    return (False, None)

  @classmethod
  def GetFullyVerifiedChanges(cls, changes, changes_by_config, subsys_by_config,
                              failing, inflight, no_stat, messages, build_root):
    """Examines build failures and returns a set of fully verified changes.

    A change is fully verified if all the build configs relevant to
    this change have either passed or failed in a manner that can be
    safely ignored by the change.

    Args:
      changes: A list of GerritPatch instances to examine.
      changes_by_config: A dictionary of relevant changes indexed by the
        config names.
      subsys_by_config: A dictionary of pass/fail HWTest subsystems indexed
        by the config names.
      failing: Names of the builders that failed.
      inflight: Names of the builders that timed out.
      no_stat: Set of builder names of slave builders that had status None.
      messages: A list of BuildFailureMessage or NoneType objects from
        the failed slaves.
      build_root: Build root directory.

    Returns:
      A dictionary mapping the fully verified changes to their string reasons
      for submission. (Should be None or constant with name STRATEGY_* from
      constants.py.)
    """
    changes = set(changes)
    no_stat = set(no_stat)
    failing = set(failing)
    inflight = set(inflight)

    fully_verified = dict()

    all_tested_changes = set()
    for tested_changes in changes_by_config.itervalues():
      all_tested_changes.update(tested_changes)

    untested_changes = changes - all_tested_changes
    if untested_changes:
      # Some board overlay changes were not tested by CQ at all.
      logging.info('These changes were not tested by any slaves, '
                   'so they will be submitted: %s',
                   cros_patch.GetChangesAsString(untested_changes))
      fully_verified.update({c: None for c in untested_changes})

    for change in all_tested_changes:
      # If all relevant configs associated with a change passed, the
      # change is fully verified.
      relevant_configs = [k for k, v in changes_by_config.iteritems() if
                          change in v]
      if any(x in set.union(no_stat, inflight) for x in relevant_configs):
        continue

      failed_configs = [x for x in relevant_configs if x in failing]
      if not failed_configs:
        logging.info('All the %s relevant config(s) for change %s passed, so '
                     'it will be submitted.', len(relevant_configs),
                     cros_patch.GetChangesAsString([change]))
        fully_verified.update({change: None})
      else:
        # Examine the failures and see if we can safely ignore them
        # for the change.
        failed_messages = [x for x in messages if x.builder in failed_configs]
        ignore_result = cls._CanIgnoreFailures(
            failed_messages, change, build_root, subsys_by_config)
        if ignore_result[0]:
          logging.info('All failures of relevant configs for change %s are '
                       'ignorable by this change, so it will be submitted.',
                       cros_patch.GetChangesAsString([change]))
          fully_verified.update({change: ignore_result[1]})

    return fully_verified
