#!/usr/bin/python
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
A library to generate and store the manifests for cros builders to use.
"""

import logging
import os
import re
import tempfile
import time
from xml.dom import minidom

from chromite.buildbot import cbuildbot_config
from chromite.buildbot import constants
from chromite.buildbot import manifest_version
from chromite.lib import cros_build_lib
from chromite.lib import git


# Paladin constants for manifest names.
PALADIN_COMMIT_ELEMENT = 'pending_commit'
PALADIN_PROJECT_ATTR = 'project'
PALADIN_CHANGE_ID_ATTR = 'change_id'
PALADIN_COMMIT_ATTR = 'commit'

MANIFEST_ELEMENT = 'manifest'
PROJECT_ELEMENT = 'project'
PROJECT_NAME_ATTR = 'name'
PROJECT_REMOTE_ATTR = 'remote'
INTERNAL_REMOTE = 'cros-internal'


class PromoteCandidateException(Exception):
  """Exception thrown for failure to promote manifest candidate."""
  pass


class _LKGMCandidateInfo(manifest_version.VersionInfo):
  """Class to encapsualte the chrome os lkgm candidate info

  You can instantiate this class in two ways.
  1)using a version file, specifically chromeos_version.sh,
  which contains the version information.
  2) just passing in the 4 version components (major, minor, sp, patch and
    revision number),
  Args:
      You can instantiate this class in two ways.
  1)using a version file, specifically chromeos_version.sh,
  which contains the version information.
  2) passing in a string with the 3 version components + revision e.g. 41.0.0-r1
  Args:
    version_string: Optional 3 component version string to parse.  Contains:
        build_number: release build number.
        branch_build_number: current build number on a branch.
        patch_number: patch number.
        revision_number: version revision
    chrome_branch: If version_string specified, specify chrome_branch i.e. 13.
    version_file: version file location.
  """
  LKGM_RE = r'(\d+\.\d+\.\d+)(?:-rc(\d+))?'

  def __init__(self, version_string=None, chrome_branch=None, incr_type=None,
               version_file=None):
    self.revision_number = 1
    if version_string:
      match = re.search(self.LKGM_RE, version_string)
      assert match, 'LKGM did not re %s' % self.LKGM_RE
      super(_LKGMCandidateInfo, self).__init__(match.group(1), chrome_branch,
                                               incr_type=incr_type)
      if match.group(2):
        self.revision_number = int(match.group(2))

    else:
      super(_LKGMCandidateInfo, self).__init__(version_file=version_file,
                                               incr_type=incr_type)

  def VersionString(self):
    """returns the full version string of the lkgm candidate"""
    return '%s.%s.%s-rc%s' % (self.build_number, self.branch_build_number,
                              self.patch_number, self.revision_number)

  @classmethod
  def VersionCompare(cls, version_string):
    """Useful method to return a comparable version of a LKGM string."""
    lkgm = cls(version_string)
    return map(int, [lkgm.build_number, lkgm.branch_build_number,
                     lkgm.patch_number, lkgm.revision_number])

  def IncrementVersion(self, message=None, dry_run=False):
    """Increments the version by incrementing the revision #."""
    self.revision_number += 1
    return self.VersionString()


class LKGMManager(manifest_version.BuildSpecsManager):
  """A Class to manage lkgm candidates and their states.

  Vars:
    lkgm_subdir:  Subdirectory within manifest repo to store candidates.
  """
  # Max timeout before assuming other builders have failed.
  LONG_MAX_TIMEOUT_SECONDS = 1200

  # Max timeout before assuming other builders have failed for Chrome PFQ.
  # Longer as there is little to lose for Chrome PFQ waiting and arm
  # has been slower often.
  CHROME_LONG_MAX_TIMEOUT_SECONDS = 3 * 60 * 60
  # Max timeout before assuming other builders have failed.
  MAX_TIMEOUT_SECONDS = 300
  # Polling timeout for checking git repo for other build statuses.
  SLEEP_TIMEOUT = constants.SLEEP_TIMEOUT

  # Sub-directories for LKGM and Chrome LKGM's.
  LKGM_SUBDIR = 'LKGM-candidates'
  CHROME_PFQ_SUBDIR = 'chrome-LKGM-candidates'
  COMMIT_QUEUE_SUBDIR = 'paladin'

  # Set path in repository to keep latest approved LKGM manifest.
  LKGM_PATH = 'LKGM/lkgm.xml'

  def __init__(self, source_repo, manifest_repo, build_name, build_type,
               incr_type, force, branch, dry_run=True, master=False):
    """Initialize an LKGM Manager.

    Args:
      build_type:  Type of build.  Must be a pfq type.
    Other args see manifest_version.BuildSpecsManager.
    """
    super(LKGMManager, self).__init__(
        source_repo=source_repo, manifest_repo=manifest_repo,
        build_name=build_name, incr_type=incr_type, force=force,
        branch=branch, dry_run=dry_run, master=master)

    self.lkgm_path = os.path.join(self.manifest_dir, self.LKGM_PATH)
    self.compare_versions_fn = _LKGMCandidateInfo.VersionCompare
    self.build_type = build_type
    # Chrome PFQ and PFQ's exist at the same time and version separately so they
    # must have separate subdirs in the manifest-versions repository.
    if self.build_type == constants.CHROME_PFQ_TYPE:
      self.rel_working_dir = self.CHROME_PFQ_SUBDIR
    elif cbuildbot_config.IsCQType(self.build_type):
      self.rel_working_dir = self.COMMIT_QUEUE_SUBDIR
    else:
      assert cbuildbot_config.IsPFQType(self.build_type)
      self.rel_working_dir = self.LKGM_SUBDIR

  def _RunLambdaWithTimeout(self, function_to_run, use_long_timeout=False):
    """Runs function_to_run until it returns a value or timeout is reached."""
    function_success = False
    start_time = time.time()
    max_timeout = self.MAX_TIMEOUT_SECONDS
    if use_long_timeout:
      if self.build_type == constants.PFQ_TYPE:
        max_timeout = self.LONG_MAX_TIMEOUT_SECONDS
      else:
        max_timeout = self.CHROME_LONG_MAX_TIMEOUT_SECONDS

    # Monitor the repo until all builders report in or we've waited too long.
    while (time.time() - start_time) < max_timeout:
      function_success = function_to_run()
      if function_success:
        break
      else:
        time.sleep(self.SLEEP_TIMEOUT)

    return function_success

  def GetCurrentVersionInfo(self):
    """Returns the lkgm version info from the version file."""
    version_info = super(LKGMManager, self).GetCurrentVersionInfo()
    return _LKGMCandidateInfo(version_info.VersionString(),
                              chrome_branch=version_info.chrome_branch,
                              incr_type=self.incr_type)

  def _AddPatchesToManifest(self, manifest, patches):
    """Adds list of patches to given manifest specified by manifest_path."""
    manifest_dom = minidom.parse(manifest)
    for patch in patches:
      pending_commit = manifest_dom.createElement(PALADIN_COMMIT_ELEMENT)
      pending_commit.setAttribute(PALADIN_PROJECT_ATTR, patch.project)
      pending_commit.setAttribute(PALADIN_CHANGE_ID_ATTR, patch.change_id)
      pending_commit.setAttribute(PALADIN_COMMIT_ATTR, patch.commit)
      manifest_dom.documentElement.appendChild(pending_commit)

    with open(manifest, 'w+') as manifest_file:
      manifest_dom.writexml(manifest_file)

  @staticmethod
  def _FilterCrosInternalProjectsFromManifest(manifest):
    """Returns a path to a new manifest with internal repositories stripped.

    Args:
      manifest: Path to an existing manifest that may have internal
        repositories.

    Returns:
      Path to a new manifest that is a copy of the original without internal
        repositories or pending commits.
    """
    temp_fd, new_path = tempfile.mkstemp('external_manifest')
    manifest_dom = minidom.parse(manifest)
    manifest_node = manifest_dom.getElementsByTagName(MANIFEST_ELEMENT)[0]
    projects = manifest_dom.getElementsByTagName(PROJECT_ELEMENT)
    pending_commits = manifest_dom.getElementsByTagName(PALADIN_COMMIT_ELEMENT)

    internal_projects = set()
    for project_element in projects:
      if project_element.getAttribute(PROJECT_REMOTE_ATTR) == INTERNAL_REMOTE:
        project = project_element.getAttribute(PROJECT_NAME_ATTR)
        internal_projects.add(project)
        manifest_node.removeChild(project_element)

    for commit_element in pending_commits:
      if commit_element.getAttribute(
          PALADIN_PROJECT_ATTR) in internal_projects:
        manifest_node.removeChild(commit_element)

    with os.fdopen(temp_fd, 'w') as manifest_file:
      manifest_dom.writexml(manifest_file)

    return new_path

  def CreateNewCandidate(self, validation_pool=None,
                         retries=manifest_version.NUM_RETRIES):
    """Creates, syncs to, and returns the next candidate manifest.

      Args:
        validation_pool: Validation pool to apply to the manifest before
          publishing.
        retries: Number of retries for updating the status.
      Raises:
        GenerateBuildSpecException in case of failure to generate a buildspec
    """
    self.CheckoutSourceCode()

    # Refresh manifest logic from manifest_versions repository to grab the
    # LKGM to generate the blamelist.
    version_info = self.GetCurrentVersionInfo()
    self.RefreshManifestCheckout()
    self.InitializeManifestVariables(version_info)

    self._GenerateBlameListSinceLKGM()
    new_manifest = self.CreateManifest()
    # For the Commit Queue, apply the validation pool as part of checkout.
    if validation_pool:
      # If we have nothing that could apply from the validation pool and
      # we're not also a pfq type, we got nothing to do.
      assert self.cros_source.directory == validation_pool.build_root
      if (not validation_pool.ApplyPoolIntoRepo() and
          not cbuildbot_config.IsPFQType(self.build_type)):
        return None

      self._AddPatchesToManifest(new_manifest, validation_pool.changes)

    last_error = None
    for attempt in range(0, retries + 1):
      try:
        # Refresh manifest logic from manifest_versions repository.
        # Note we don't need to do this on our first attempt as we needed to
        # have done it to get the LKGM.
        if attempt != 0:
          self.RefreshManifestCheckout()
          self.InitializeManifestVariables(version_info)

        # If we don't have any valid changes to test, make sure the checkout
        # is at least different.
        if ((not validation_pool or not validation_pool.changes) and
            not self.force and self.HasCheckoutBeenBuilt()):
          return None

        # Check whether the latest spec available in manifest-versions is
        # newer than our current version number. If so, use it as the base
        # version number. Otherwise, we default to 'rc1'.
        if self.latest:
          latest = max(self.latest, version_info.VersionString(),
                       key=self.compare_versions_fn)
          version_info = _LKGMCandidateInfo(
              latest, chrome_branch=version_info.chrome_branch,
              incr_type=self.incr_type)

        git.CreatePushBranch(manifest_version.PUSH_BRANCH, self.manifest_dir,
                             sync=False)
        version = self.GetNextVersion(version_info)
        self.PublishManifest(new_manifest, version)
        self.current_version = version
        return self.GetLocalManifest(version)
      except cros_build_lib.RunCommandError as e:
        err_msg = 'Failed to generate LKGM Candidate. error: %s' % e
        logging.error(err_msg)
        last_error = err_msg
    else:
      raise manifest_version.GenerateBuildSpecException(last_error)

  def CreateFromManifest(self, manifest, retries=manifest_version.NUM_RETRIES):
    """Sets up an lkgm_manager from the given manifest.

    This method sets up an LKGM manager and publishes a new manifest to the
    manifest versions repo based on the passed in manifest but filtering
    internal repositories and changes out of it.
    Args:
      manifest: A manifest that possibly contains private changes/projects. It
        is named with the given version we want to create a new manifest from
        i.e R20-1920.0.1-rc7.xml where R20-1920.0.1-rc7 is the version.
      retries: Number of retries for updating the status.
    Raises:
      GenerateBuildSpecException in case of failure to check-in the new
        manifest because of a git error or the manifest is already checked-in.
    """
    last_error = None
    new_manifest = self._FilterCrosInternalProjectsFromManifest(manifest)
    version_info = self.GetCurrentVersionInfo()
    for _attempt in range(0, retries + 1):
      try:
        self.RefreshManifestCheckout()
        self.InitializeManifestVariables(version_info)

        git.CreatePushBranch(manifest_version.PUSH_BRANCH, self.manifest_dir,
                             sync=False)
        version = os.path.splitext(os.path.basename(manifest))[0]
        self.PublishManifest(new_manifest, version)
        self.SetInFlight(version)
        self.current_version = version
        return self.GetLocalManifest(version)
      except cros_build_lib.RunCommandError as e:
        err_msg = 'Failed to generate LKGM Candidate. error: %s' % e
        logging.error(err_msg)
        last_error = err_msg
    else:
      raise manifest_version.GenerateBuildSpecException(last_error)

  def GetLatestCandidate(self):
    """Gets and syncs to the next candiate manifest.
      Args:
        retries: Number of retries for updating the status
      Returns:
        Local path to manifest to build or None in case of no need to build.
      Raises:
        GenerateBuildSpecException in case of failure to generate a buildspec
    """
    def _AttemptToGetLatestCandidate():
      """Attempts to acquire latest candidate using manifest repo."""
      self.RefreshManifestCheckout()
      self.InitializeManifestVariables(self.GetCurrentVersionInfo())
      if self.latest_unprocessed:
        return self.latest_unprocessed
      else:
        logging.info('Found nothing new to build, trying again later.')
        logging.info('If this is a PFQ, then you should have forced the master'
                     ', which runs cbuildbot_master')
        return None

    # TODO(sosa):  We only really need the overlay for the version info but we
    # do a full checkout here because we have no way of refining it currently.
    self.CheckoutSourceCode()
    version_to_build = self._RunLambdaWithTimeout(_AttemptToGetLatestCandidate,
                                                  use_long_timeout=True)
    if version_to_build:
      logging.info('Starting build spec: %s', version_to_build)
      self.SetInFlight(version_to_build)
      self.current_version = version_to_build

      # Actually perform the sync.
      manifest = self.GetLocalManifest(version_to_build)
      self.cros_source.Sync(manifest)
      self._GenerateBlameListSinceLKGM()
      return manifest
    else:
      return None

  def GetBuildersStatus(self, builders_array):
    """Returns a build-names->status dictionary of build statuses."""
    builders_completed = set()
    builder_statuses = {}

    def _CheckStatusOfBuildersArray():
      """Helper function that iterates through current statuses."""
      for b in builders_array:
        cached_status = builder_statuses.get(b)
        if not cached_status or not cached_status.Completed():
          logging.debug("Checking for builder %s's status", b)
          builder_status = self.GetBuildStatus(b, self.current_version)
          builder_statuses[b] = builder_status
          if builder_status is None:
            logging.warn('No status found for builder %s.', b)
          elif builder_status.Passed():
            builders_completed.add(b)
            logging.info('Builder %s completed with status passed', b)
          elif builder_status.Failed():
            builders_completed.add(b)
            logging.info('Builder %s completed with status failed', b)


      if len(builders_completed) < len(builders_array):
        logging.info('Still waiting for the following builds to complete: %r',
                     sorted(set(builders_array).difference(builders_completed)))
        return None
      else:
        return 'Builds completed.'

    # Check for build completion until all builders report in.
    builds_succeeded = self._RunLambdaWithTimeout(_CheckStatusOfBuildersArray,
                                                  use_long_timeout=True)
    if not builds_succeeded:
      logging.error('Not all builds finished before MAX_TIMEOUT reached.')

    return builder_statuses

  def PromoteCandidate(self, retries=manifest_version.NUM_RETRIES):
    """Promotes the current LKGM candidate to be a real versioned LKGM."""
    assert self.current_version, 'No current manifest exists.'

    last_error = None
    path_to_candidate = self.GetLocalManifest(self.current_version)
    assert os.path.exists(path_to_candidate), 'Candidate not found locally.'

    # This may potentially fail for not being at TOT while pushing.
    for attempt in range(0, retries + 1):
      try:
        if attempt > 0:
          self.RefreshManifestCheckout()
        git.CreatePushBranch(manifest_version.PUSH_BRANCH,
                                        self.manifest_dir, sync=False)
        manifest_version.CreateSymlink(path_to_candidate, self.lkgm_path)
        cros_build_lib.RunCommand(['git', 'add', self.LKGM_PATH],
                                  cwd=self.manifest_dir)
        self.PushSpecChanges(
            'Automatic: %s promoting %s to LKGM' % (self.build_name,
                                                    self.current_version))
        return
      except cros_build_lib.RunCommandError as e:
        last_error = 'Failed to promote manifest. error: %s' % e
        logging.error(last_error)
        logging.error('Retrying to promote manifest:  Retry %d/%d', attempt + 1,
                      retries)

    else:
      raise PromoteCandidateException(last_error)

  def _ShouldGenerateBlameListSinceLKGM(self):
    """Returns True if we should generate the blamelist."""
    # We want to generate the blamelist only for valid pfq types and if we are
    # building on the master branch i.e. revving the build number.
    return (self.incr_type == 'build' and
            cbuildbot_config.IsPFQType(self.build_type) and
            self.build_type != constants.CHROME_PFQ_TYPE)

  def _GenerateBlameListSinceLKGM(self):
    """Prints out links to all CL's that have been committed since LKGM.

    Add buildbot trappings to print <a href='url'>text</a> in the waterfall for
    each CL committed since we last had a passing build.
    """
    if not self._ShouldGenerateBlameListSinceLKGM():
      logging.info('Not generating blamelist for lkgm as it is not appropriate '
                   'for this build type.')
      return
    # Suppress re-printing changes we tried ourselves on paladin
    # builders since they are redundant.
    only_print_chumps = self.build_type == constants.PALADIN_TYPE
    GenerateBlameList(self.cros_source, self.lkgm_path,
                      only_print_chumps=only_print_chumps)

  def GetLatestPassingSpec(self):
    """Get the last spec file that passed in the current branch."""
    raise NotImplementedError()


def GenerateBlameList(source_repo, lkgm_path, only_print_chumps=False):
  """Generate the blamelist since the specified manifest.

  Arguments:
    source_repo: Repository object for the source code.
    lkgm_path: Path to LKGM manifest.
    only_print_chumps: If True, only print changes that were chumped.
  """
  handler = git.Manifest(lkgm_path)
  reviewed_on_re = re.compile(r'\s*Reviewed-on:\s*(\S+)')
  author_re = re.compile(r'\s*Author:.*<(\S+)@\S+>\s*')
  committer_re = re.compile(r'\s*Commit:.*<(\S+)@\S+>\s*')
  for project in handler.projects.keys():
    rel_src_path = handler.projects[project].get('path')

    # If it's not part of our source tree, it doesn't affect our build.
    if not rel_src_path:
      continue

    # Additional case in case the repo has been removed from the manifest.
    src_path = source_repo.GetRelativePath(rel_src_path)
    if not os.path.exists(src_path):
      cros_build_lib.Info('Detected repo removed from manifest %s' % project)
      continue

    revision = handler.projects[project]['revision']
    result = cros_build_lib.RunCommand(['git', 'log', '--pretty=full',
                                        '%s..HEAD' % revision],
                                       print_cmd=False, redirect_stdout=True,
                                       cwd=src_path)
    current_author = None
    current_committer = None
    for line in result.output.splitlines():
      author_match = author_re.match(line)
      if author_match:
        current_author = author_match.group(1)

      committer_match = committer_re.match(line)
      if committer_match:
        current_committer = committer_match.group(1)

      review_match = reviewed_on_re.match(line)
      if review_match:
        review = review_match.group(1)
        _, _, change_number = review.rpartition('/')
        if current_committer != 'chrome-bot':
          cros_build_lib.PrintBuildbotLink(
              'CHUMP %s:%s' % (current_author, change_number),
              review)
        elif not only_print_chumps:
          cros_build_lib.PrintBuildbotLink(
              '%s:%s' % (current_author, change_number),
              review)
