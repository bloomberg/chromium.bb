#!/usr/bin/python
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
A library to generate and store the manifests for cros builders to use.
"""

import logging
import os
import re
import time
from xml.dom import minidom

from chromite.buildbot import constants
from chromite.buildbot import manifest_version
from chromite.lib import cros_build_lib as cros_lib


# Paladin constants for manifest names.
PALADIN_COMMIT_ELEMENT = 'pending_commit'
PALADIN_PROJECT_ATTR = 'project'
PALADIN_CHANGE_ID_ATTR = 'change_id'
PALADIN_COMMIT_ATTR = 'commit'


class PromoteCandidateException(Exception):
  """Exception thrown for failure to promote manifest candidate."""
  pass


def PrintLink(text, url):
  """Prints out a link to buildbot."""
  print '\n@@@STEP_LINK@%(text)s@%(url)s@@@' % { 'text': text, 'url': url }


def _SyncGitRepo(local_dir):
  """"Clone Given git repo
  Args:
    local_dir: location with repo that should be synced.
  """
  cros_lib.RunCommand(['git', 'remote', 'update'], cwd=local_dir)
  cros_lib.RunCommand(['git', 'rebase', 'origin/master'], cwd=local_dir)


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
  LKGM_RE = '(\d+\.\d+\.\d+)(?:-rc(\d+))?'

  def __init__(self, version_string=None, chrome_branch=None,
               version_file=None):
    self.revision_number = None
    if version_string:
      match = re.search(self.LKGM_RE, version_string)
      assert match, 'LKGM did not re %s' % self.LKGM_RE
      super(_LKGMCandidateInfo, self).__init__(match.group(1), chrome_branch,
                                               incr_type='branch')
      if match.group(2):
        self.revision_number = int(match.group(2))

    else:
      super(_LKGMCandidateInfo, self).__init__(version_file=version_file,
                                               incr_type='branch')
    if not self.revision_number:
      self.revision_number = 1

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
  # Max timeout before assuming other builders have failed for Chrome PFQ.
  # Longer as there is little to lose for Chrome PFQ waiting and arm
  # has been slower often.
  CHROME_LONG_MAX_TIMEOUT_SECONDS = 3600
  # Max timeout before assuming other builders have failed.
  LONG_MAX_TIMEOUT_SECONDS = 1200
  MAX_TIMEOUT_SECONDS = 300
  # Polling timeout for checking git repo for other build statuses.
  SLEEP_TIMEOUT = 30

  # Sub-directories for LKGM and Chrome LKGM's.
  LKGM_SUBDIR = 'LKGM-candidates'
  CHROME_PFQ_SUBDIR = 'chrome-LKGM-candidates'
  COMMIT_QUEUE_SUBDIR = 'paladin'

  # Set path in repository to keep latest approved LKGM manifest.
  LKGM_PATH = 'LKGM/lkgm.xml'

  @classmethod
  def GetAbsolutePathToLKGM(cls):
    """Returns the path to the LKGM file blessed by builders."""
    return os.path.join(cls._TMP_MANIFEST_DIR, cls.LKGM_PATH)

  @classmethod
  def GetLKGMVersion(cls):
    """Returns the full buildspec version the LKGM corresponds to."""
    realpath = os.path.realpath(cls.GetAbsolutePathToLKGM())
    version, _ = os.path.splitext(os.path.basename(realpath))
    return version

  def __init__(self, source_dir, checkout_repo, manifest_repo, branch,
               build_name, build_type, dry_run=True):
    """Initialize an LKGM Manager.

    Args:
      build_type:  Type of build.  Must be a pfq type.
    Other args see manifest_version.BuildSpecsManager.
    """
    super(LKGMManager, self).__init__(
        source_dir=source_dir, checkout_repo=checkout_repo,
        manifest_repo=manifest_repo, branch=branch, build_name=build_name,
        incr_type='branch', dry_run=dry_run)

    self.compare_versions_fn = _LKGMCandidateInfo.VersionCompare
    self.build_type = build_type
    if self.build_type == constants.CHROME_PFQ_TYPE:
      self.rel_working_dir = self.CHROME_PFQ_SUBDIR
    elif self.build_type == constants.COMMIT_QUEUE_TYPE:
      self.rel_working_dir = self.COMMIT_QUEUE_SUBDIR
    else:
      assert self.build_type, constants.PFQ_TYPE
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

  def _GetLatestCandidateByVersion(self, version_info):
    """Returns the latest lkgm candidate corresponding to the version file.
    Args:
      version_info: Info class for version information of cros.
    """
    if self.all:
      matched_lkgms = filter(
          lambda ver: ver.startswith(version_info.VersionString()), self.all)
      if matched_lkgms:
        return _LKGMCandidateInfo(sorted(matched_lkgms,
                                         key=self.compare_versions_fn)[-1])

    return _LKGMCandidateInfo(version_info.VersionString())

  def AddPatchesToManifest(self, manifest, patches):
    """Adds list of patches to given manifest specified by manifest_path."""
    manifest_dom = minidom.parse(manifest)
    for patch in patches:
      pending_commit = manifest_dom.createElement(PALADIN_COMMIT_ELEMENT)
      pending_commit.setAttribute(PALADIN_PROJECT_ATTR, patch.project)
      pending_commit.setAttribute(PALADIN_CHANGE_ID_ATTR, patch.id)
      pending_commit.setAttribute(PALADIN_COMMIT_ATTR, patch.commit)
      manifest_dom.documentElement.appendChild(pending_commit)

    with open(manifest, 'w+') as manifest_file:
      manifest_dom.writexml(manifest_file)

  def CreateNewCandidate(self, patches=None, retries=3):
    """Gets the version number of the next build spec to build.
      Args:
        patches: An array of GerritPatches that should be built with this
          manifest as part of a Commit Queue run.
        retries: Number of retries for updating the status.
      Returns:
        next_build: a string of the next build number for the builder to consume
                    or None in case of no need to build.
      Raises:
        GenerateBuildSpecException in case of failure to generate a buildspec
    """
    last_error = None
    for _ in range(0, retries + 1):
      try:
        version_info = self._GetCurrentVersionInfo()
        self._LoadSpecs(version_info)
        lkgm_info = self._GetLatestCandidateByVersion(version_info)
        self._PrepSpecChanges()
        self.current_version = self._CreateNewBuildSpec(lkgm_info)
        path_to_new_build_spec = self.GetLocalManifest(self.current_version)
        if patches: self.AddPatchesToManifest(path_to_new_build_spec, patches)
        if self.current_version:
          logging.debug('Using build spec: %s', self.current_version)
          commit_message = 'Automatic: Start %s %s' % (self.build_name,
                                                       self.current_version)
          self._SetInFlight()
          self._PushSpecChanges(commit_message)

        return path_to_new_build_spec

      except (cros_lib.RunCommandError,
              manifest_version.GitCommandException) as e:
        err_msg = 'Failed to generate LKGM Candidate. error: %s' % e
        logging.error(err_msg)
        last_error = err_msg
    else:
      raise manifest_version.GenerateBuildSpecException(last_error)

  def GetLatestCandidate(self, retries=5):
    """Gets the version number of the next build spec to build.
      Args:
        retries: Number of retries for updating the status
      Returns:
        Local path to manifest to build or None in case of no need to build.
      Raises:
        GenerateBuildSpecException in case of failure to generate a buildspec
    """
    def _AttemptToGetLatestCandidate():
      """Attempts to acquire latest candidate using manifest repo."""
      version_info = self._GetCurrentVersionInfo()
      self._LoadSpecs(version_info)
      version_to_use = None
      if self.latest_unprocessed:
        version_to_use = self.latest_unprocessed
      else:
        logging.info('Found nothing new to build, trying again later.')

      return version_to_use

    self.current_version = self._RunLambdaWithTimeout(
        _AttemptToGetLatestCandidate)
    if self.current_version:
      last_error = None
      for _ in range(0, retries + 1):
        try:
          logging.debug('Using build spec: %s', self.current_version)
          commit_message = 'Automatic: Start %s %s' % (self.build_name,
                                                       self.current_version)
          self._PrepSpecChanges()
          self._SetInFlight()
          self._PushSpecChanges(commit_message)
          break
        except (cros_lib.RunCommandError,
                manifest_version.GitCommandException) as e:
          err_msg = 'Failed to set LKGM Candidate inflight. error: %s' % e
          logging.error(err_msg)
          last_error = err_msg
      else:
        raise manifest_version.GenerateBuildSpecException(last_error)

    return self.GetLocalManifest(self.current_version)

  def GetBuildersStatus(self, builders_array, version_file):
    """Returns a build-names->status dictionary of build statuses."""
    builder_statuses = {}

    def _CheckStatusOfBuildersArray():
      """Helper function that iterates through current statuses."""
      num_complete = 0
      _SyncGitRepo(self._TMP_MANIFEST_DIR)
      version_info = _LKGMCandidateInfo(version_file=version_file)
      for builder in builders_array:
        if builder_statuses.get(builder) not in LKGMManager.STATUS_COMPLETED:
          logging.debug("Checking for builder %s's status" % builder)
          builder_statuses[builder] = self.GetBuildStatus(builder, version_info)
          if builder_statuses[builder] == LKGMManager.STATUS_PASSED:
            num_complete += 1
            logging.info('Builder %s completed with status passed', builder)
          elif builder_statuses[builder] == LKGMManager.STATUS_FAILED:
            num_complete += 1
            logging.info('Builder %s completed with status failed', builder)
          elif not builder_statuses[builder]:
            logging.debug('No status found for builder %s.' % builder)
        else:
          num_complete += 1

      if num_complete < len(builders_array):
        logging.info('Waiting for other builds to complete')
        return None
      else:
        return 'Builds completed.'

    # Check for build completion until all builders report in.
    builds_succeeded = self._RunLambdaWithTimeout(_CheckStatusOfBuildersArray,
                                                  use_long_timeout=True)
    if not builds_succeeded:
      logging.error('Not all builds finished before MAX_TIMEOUT reached.')

    return builder_statuses

  def PromoteCandidate(self, retries=5):
    """Promotes the current LKGM candidate to be a real versioned LKGM."""
    assert self.current_version, 'No current manifest exists.'

    last_error = None
    path_to_candidate = self.GetLocalManifest(self.current_version)
    path_to_lkgm = self.GetAbsolutePathToLKGM()
    assert os.path.exists(path_to_candidate), 'Candidate not found locally.'

    # This may potentially fail for not being at TOT while pushing.
    for index in range(0, retries + 1):
      try:
        self._PrepSpecChanges()
        manifest_version.CreateSymlink(path_to_candidate, path_to_lkgm)
        cros_lib.RunCommand(['git', 'add', self.LKGM_PATH],
                            cwd=self._TMP_MANIFEST_DIR)
        self._PushSpecChanges(
            'Automatic: %s promoting %s to LKGM' % (self.build_name,
                                                    self.current_version))
        return
      except (manifest_version.GitCommandException,
              cros_lib.RunCommandError) as e:
        last_error = 'Failed to promote manifest. error: %s' % e
        logging.error(last_error)
        logging.error('Retrying to promote manifest:  Retry %d/%d' %
                      (index + 1, retries))

    else:
      raise PromoteCandidateException(last_error)

  def GenerateBlameListSinceLKGM(self):
    """Prints out links to all CL's that have been committed since LKGM.

    Add buildbot trappings to print <a href='url'>text</a> in the waterfall for
    each CL committed since we last had a passing build.
    """
    handler = cros_lib.ManifestHandler.ParseManifest(
        self.GetAbsolutePathToLKGM())
    reviewed_on_re = re.compile('\s*Reviewed-on:\s*(\S+)')
    author_re = re.compile('\s*Author:.*<(\S+)@\S+>\s*')
    for project in handler.projects.keys():
      rel_src_path = handler.projects[project].get('path')

      # If it's not part of our source tree, it doesn't affect our build.
      if not rel_src_path:
        continue

      src_path = self.cros_source.GetRelativePath(rel_src_path)

      # Additional case in case the repo has been removed from the manifest.
      if not os.path.exists(src_path):
        cros_lib.Info('Detected repo removed from manifest %s' % project)
        continue

      revision = handler.projects[project]['revision']
      result = cros_lib.RunCommand(['git', 'log', '%s..HEAD' % revision],
                                   print_cmd=False, redirect_stdout=True,
                                   cwd=src_path)
      current_author = None
      for line in result.output.splitlines():
        author_match = author_re.match(line)
        if author_match:
          current_author = author_match.group(1)

        review_match = reviewed_on_re.match(line)
        if review_match:
          review = review_match.group(1)
          _, _, change_number = review.rpartition('/')
          PrintLink('%s:%s' % (current_author, change_number), review)
