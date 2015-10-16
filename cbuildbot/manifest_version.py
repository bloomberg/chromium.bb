# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A library to generate and store the manifests for cros builders to use."""

from __future__ import print_function

import cPickle
import datetime
import fnmatch
import glob
import os
import re
import shutil
import tempfile
from xml.dom import minidom

from chromite.cbuildbot import config_lib
from chromite.cbuildbot import constants
from chromite.cbuildbot import repository
from chromite.lib import cidb
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import git
from chromite.lib import gs
from chromite.lib import osutils
from chromite.lib import timeout_util


site_config = config_lib.GetConfig()


BUILD_STATUS_URL = (
    '%s/builder-status' % site_config.params.MANIFEST_VERSIONS_GS_URL)
PUSH_BRANCH = 'temp_auto_checkin_branch'
NUM_RETRIES = 20

MANIFEST_ELEMENT = 'manifest'
DEFAULT_ELEMENT = 'default'
PROJECT_ELEMENT = 'project'
REMOTE_ELEMENT = 'remote'
PROJECT_NAME_ATTR = 'name'
PROJECT_REMOTE_ATTR = 'remote'
PROJECT_GROUP_ATTR = 'groups'
REMOTE_NAME_ATTR = 'name'

PALADIN_COMMIT_ELEMENT = 'pending_commit'
PALADIN_PROJECT_ATTR = 'project'


class FilterManifestException(Exception):
  """Exception thrown when failing to filter the internal manifest."""


class VersionUpdateException(Exception):
  """Exception gets thrown for failing to update the version file"""


class StatusUpdateException(Exception):
  """Exception gets thrown for failure to update the status"""


class GenerateBuildSpecException(Exception):
  """Exception gets thrown for failure to Generate a buildspec for the build"""


class BuildSpecsValueError(Exception):
  """Exception gets thrown when a encountering invalid values."""


def RefreshManifestCheckout(manifest_dir, manifest_repo):
  """Checks out manifest-versions into the manifest directory.

  If a repository is already present, it will be cleansed of any local
  changes and restored to its pristine state, checking out the origin.
  """
  reinitialize = True
  if os.path.exists(manifest_dir):
    result = git.RunGit(manifest_dir, ['config', 'remote.origin.url'],
                        error_code_ok=True)
    if (result.returncode == 0 and
        result.output.rstrip() == manifest_repo):
      logging.info('Updating manifest-versions checkout.')
      try:
        git.RunGit(manifest_dir, ['gc', '--auto'])
        git.CleanAndCheckoutUpstream(manifest_dir)
      except cros_build_lib.RunCommandError:
        logging.warning('Could not update manifest-versions checkout.')
      else:
        reinitialize = False
  else:
    logging.info('No manifest-versions checkout exists at %s', manifest_dir)

  if reinitialize:
    logging.info('Cloning fresh manifest-versions checkout.')
    osutils.RmDir(manifest_dir, ignore_missing=True)
    repository.CloneGitRepo(manifest_dir, manifest_repo)


def _PushGitChanges(git_repo, message, dry_run=False, push_to=None):
  """Push the final commit into the git repo.

  Args:
    git_repo: git repo to push
    message: Commit message
    dry_run: If true, don't actually push changes to the server
    push_to: A git.RemoteRef object specifying the remote branch to push to.
      Defaults to the tracking branch of the current branch.
  """
  if push_to is None:
    # TODO(akeshet): Clean up git.GetTrackingBranch to always or never return a
    # tuple.
    # pylint: disable=unpacking-non-sequence
    push_to = git.GetTrackingBranch(
        git_repo, for_checkout=False, for_push=True)

  git.RunGit(git_repo, ['add', '-A'])

  # It's possible that while we are running on dry_run, someone has already
  # committed our change.
  try:
    git.RunGit(git_repo, ['commit', '-m', message])
  except cros_build_lib.RunCommandError:
    if dry_run:
      return
    raise

  git.GitPush(git_repo, PUSH_BRANCH, push_to, skip=dry_run)


def CreateSymlink(src_file, dest_file):
  """Creates a relative symlink from src to dest with optional removal of file.

  More robust symlink creation that creates a relative symlink from src_file to
  dest_file.

  This is useful for multiple calls of CreateSymlink where you are using
  the dest_file location to store information about the status of the src_file.

  Args:
    src_file: source for the symlink
    dest_file: destination for the symlink
  """
  dest_dir = os.path.dirname(dest_file)
  osutils.SafeUnlink(dest_file)
  osutils.SafeMakedirs(dest_dir)

  rel_src_file = os.path.relpath(src_file, dest_dir)
  logging.debug('Linking %s to %s', rel_src_file, dest_file)
  os.symlink(rel_src_file, dest_file)


class VersionInfo(object):
  """Class to encapsulate the Chrome OS version info scheme.

  You can instantiate this class in three ways.
  1) using a version file, specifically chromeos_version.sh,
     which contains the version information.
  2) passing in a string with the 3 version components.
  3) using a source repo and calling from_repo().

  Args:
    version_string: Optional 3 component version string to parse.  Contains:
        build_number: release build number.
        branch_build_number: current build number on a branch.
        patch_number: patch number.
    chrome_branch: If version_string specified, specify chrome_branch i.e. 13.
    incr_type: How we should increment this version -
        chrome_branch|build|branch|patch
    version_file: version file location.
  """
  # Pattern for matching build name format.  Includes chrome branch hack.
  VER_PATTERN = r'(\d+).(\d+).(\d+)(?:-R(\d+))*'
  KEY_VALUE_PATTERN = r'%s=(\d+)\s*$'
  VALID_INCR_TYPES = ('chrome_branch', 'build', 'branch', 'patch')

  def __init__(self, version_string=None, chrome_branch=None,
               incr_type='build', version_file=None):
    if version_file:
      self.version_file = version_file
      logging.debug('Using VERSION _FILE = %s', version_file)
      self._LoadFromFile()
    else:
      match = re.search(self.VER_PATTERN, version_string)
      self.build_number = match.group(1)
      self.branch_build_number = match.group(2)
      self.patch_number = match.group(3)
      self.chrome_branch = chrome_branch
      self.version_file = None

    self.incr_type = incr_type

  @classmethod
  def from_repo(cls, source_repo, **kwargs):
    kwargs['version_file'] = os.path.join(source_repo, constants.VERSION_FILE)
    return cls(**kwargs)

  def _LoadFromFile(self):
    """Read the version file and set the version components"""
    with open(self.version_file, 'r') as version_fh:
      for line in version_fh:
        if not line.strip():
          continue

        match = self.FindValue('CHROME_BRANCH', line)
        if match:
          self.chrome_branch = match
          logging.debug('Set the Chrome branch number to:%s',
                        self.chrome_branch)
          continue

        match = self.FindValue('CHROMEOS_BUILD', line)
        if match:
          self.build_number = match
          logging.debug('Set the build version to:%s', self.build_number)
          continue

        match = self.FindValue('CHROMEOS_BRANCH', line)
        if match:
          self.branch_build_number = match
          logging.debug('Set the branch version to:%s',
                        self.branch_build_number)
          continue

        match = self.FindValue('CHROMEOS_PATCH', line)
        if match:
          self.patch_number = match
          logging.debug('Set the patch version to:%s', self.patch_number)
          continue

    logging.debug(self.VersionString())

  def FindValue(self, key, line):
    """Given the key find the value from the line, if it finds key = value

    Args:
      key: key to look for
      line: string to search

    Returns:
      None: on a non match
      value: for a matching key
    """
    match = re.search(self.KEY_VALUE_PATTERN % (key,), line)
    return match.group(1) if match else None

  def IncrementVersion(self):
    """Updates the version file by incrementing the patch component."""
    if not self.incr_type or self.incr_type not in self.VALID_INCR_TYPES:
      raise VersionUpdateException('Need to specify the part of the version to'
                                   ' increment')

    if self.incr_type == 'chrome_branch':
      self.chrome_branch = str(int(self.chrome_branch) + 1)

    # Increment build_number for 'chrome_branch' incr_type to avoid
    # crbug.com/213075.
    if self.incr_type in ('build', 'chrome_branch'):
      self.build_number = str(int(self.build_number) + 1)
      self.branch_build_number = '0'
      self.patch_number = '0'
    elif self.incr_type == 'branch' and self.patch_number == '0':
      self.branch_build_number = str(int(self.branch_build_number) + 1)
    else:
      self.patch_number = str(int(self.patch_number) + 1)

    return self.VersionString()

  def UpdateVersionFile(self, message, dry_run, push_to=None):
    """Update the version file with our current version.

    Args:
      message: Commit message.
      dry_run: Git dryrun.
      push_to: A git.RemoteRef object.
    """

    if not self.version_file:
      raise VersionUpdateException('Cannot call UpdateVersionFile without '
                                   'an associated version_file')

    components = (('CHROMEOS_BUILD', self.build_number),
                  ('CHROMEOS_BRANCH', self.branch_build_number),
                  ('CHROMEOS_PATCH', self.patch_number),
                  ('CHROME_BRANCH', self.chrome_branch))

    with tempfile.NamedTemporaryFile(prefix='mvp') as temp_fh:
      with open(self.version_file, 'r') as source_version_fh:
        for line in source_version_fh:
          for key, value in components:
            line = re.sub(self.KEY_VALUE_PATTERN % (key,),
                          '%s=%s\n' % (key, value), line)
          temp_fh.write(line)

      temp_fh.flush()

      repo_dir = os.path.dirname(self.version_file)

      try:
        git.CreateBranch(repo_dir, PUSH_BRANCH)
        shutil.copyfile(temp_fh.name, self.version_file)
        _PushGitChanges(repo_dir, message, dry_run=dry_run, push_to=push_to)
      finally:
        # Update to the remote version that contains our changes. This is needed
        # to ensure that we don't build a release using a local commit.
        git.CleanAndCheckoutUpstream(repo_dir)

  def VersionString(self):
    """returns the version string"""
    return '%s.%s.%s' % (self.build_number, self.branch_build_number,
                         self.patch_number)

  def VersionComponents(self):
    """Return an array of ints of the version fields for comparing."""
    return map(int, [self.build_number, self.branch_build_number,
                     self.patch_number])

  @classmethod
  def VersionCompare(cls, version_string):
    """Useful method to return a comparable version of a LKGM string."""
    return cls(version_string).VersionComponents()

  def __cmp__(self, other):
    sinfo = self.VersionComponents()
    oinfo = other.VersionComponents()

    for s, o in zip(sinfo, oinfo):
      if s != o:
        return -1 if s < o else 1
    return 0

  __hash__ = None

  def BuildPrefix(self):
    """Returns the build prefix to match the buildspecs in  manifest-versions"""
    if self.incr_type == 'branch':
      if self.patch_number == '0':
        return '%s.' % self.build_number
      else:
        return '%s.%s.' % (self.build_number, self.branch_build_number)
    # Default to build incr_type.
    return ''

  def __str__(self):
    return '%s(%s)' % (self.__class__, self.VersionString())


class BuilderStatus(object):
  """Object representing the status of a build."""

  def __init__(self, status, message, dashboard_url=None):
    """Constructor for BuilderStatus.

    Args:
      status: Status string (should be one of STATUS_FAILED, STATUS_PASSED,
              STATUS_INFLIGHT, or STATUS_MISSING).
      message: A failures_lib.BuildFailureMessage object with details
               of builder failure. Or, None.
      dashboard_url: Optional url linking to builder dashboard for this build.
    """
    self.status = status
    self.message = message
    self.dashboard_url = dashboard_url

  # Helper methods to make checking the status object easy.

  def Failed(self):
    """Returns True if the Builder failed."""
    return self.status == constants.BUILDER_STATUS_FAILED

  def Passed(self):
    """Returns True if the Builder passed."""
    return self.status == constants.BUILDER_STATUS_PASSED

  def Inflight(self):
    """Returns True if the Builder is still inflight."""
    return self.status == constants.BUILDER_STATUS_INFLIGHT

  def Missing(self):
    """Returns True if the Builder is missing any status."""
    return self.status == constants.BUILDER_STATUS_MISSING

  def Completed(self):
    """Returns True if the Builder has completed."""
    return self.status in constants.BUILDER_COMPLETED_STATUSES

  @classmethod
  def GetCompletedStatus(cls, success):
    """Return the appropriate status constant for a completed build.

    Args:
      success: Whether the build was successful or not.
    """
    if success:
      return constants.BUILDER_STATUS_PASSED
    else:
      return constants.BUILDER_STATUS_FAILED

  def AsFlatDict(self):
    """Returns a flat json-able representation of this builder status.

    Returns:
      A dictionary of the form {'status' : status, 'message' : message,
      'dashboard_url' : dashboard_url} where all values are guaranteed
      to be strings. If dashboard_url is None, the key will be excluded.
    """
    flat_dict = {'status' : str(self.status),
                 'message' : str(self.message),
                 'reason' : str(None if self.message is None
                                else self.message.reason)}
    if self.dashboard_url is not None:
      flat_dict['dashboard_url'] = str(self.dashboard_url)
    return flat_dict

  def AsPickledDict(self):
    """Returns a pickled dictionary representation of this builder status."""
    return cPickle.dumps(dict(status=self.status, message=self.message,
                              dashboard_url=self.dashboard_url))


class SlaveStatus(object):
  """A Class to easily interpret data from CIDB regarding slave status.

  This is intended for short lived instances used to determine if the loop on
  getting the builders statuses should continue or break out.  The main function
  is ShouldWait() with everything else pretty much a helper function for it.
  """

  BUILDER_START_TIMEOUT = 5

  def __init__(self, status, start_time, builders_array, previous_completed):
    """Initializes a slave status object.

    Args:
      status: Dict of the slave status from CIDB.
      start_time: datetime.datetime object of when the build started.
      builders_array: List of the expected builders.
      previous_completed: Set of builders that have finished already.
    """
    self.status = status
    self.start_time = start_time
    self.builders_array = builders_array
    self.previous_completed = previous_completed
    self.completed = []

  def GetMissing(self):
    """Returns the missing builders.

    Returns:
      A list of the missing builders
    """
    return list(set(self.builders_array) - set(self.status.keys()))

  def GetCompleted(self):
    """Returns the builders that have completed.

    Returns:
      A list of the completed builders.
    """
    if not self.completed:
      self.completed = [b for b, s in self.status.iteritems()
                        if s in constants.BUILDER_COMPLETED_STATUSES and
                        b in self.builders_array]

    # Logging of the newly complete builders.
    for builder in sorted(set(self.completed) - self.previous_completed):
      logging.info('Build config %s completed with status "%s".',
                   builder, self.status[builder])
    self.previous_completed.update(set(self.completed))
    return self.completed

  def Completed(self):
    """Returns a bool if all builders have completed successfully.

    Returns:
      A bool of True if all builders successfully completed, False otherwise.
    """
    return len(self.GetCompleted()) == len(self.builders_array)

  def ShouldFailForBuilderStartTimeout(self, current_time):
    """Decides if we should fail if a builder hasn't started within 5 mins.

    If a builder hasn't started within BUILDER_START_TIMEOUT and the rest of the
    builders have finished, let the caller know that we should fail.

    Args:
      current_time: A datetime.datetime object letting us know the current time.

    Returns:
      A bool saying True that we should fail, False otherwise.
    """
    # Check that we're at least past the start timeout.
    builder_start_deadline = datetime.timedelta(
        minutes=self.BUILDER_START_TIMEOUT)
    past_deadline = current_time - self.start_time > builder_start_deadline

    # Check that aside from the missing builders the rest have completed.
    other_builders_completed = (
        (len(self.GetMissing()) + len(self.GetCompleted())) ==
        len(self.builders_array))

    # Check that we have missing builders and logging who they are.
    builders_are_missing = False
    for builder in self.GetMissing():
      logging.error('No status found for build config %s.', builder)
      builders_are_missing = True

    return past_deadline and other_builders_completed and builders_are_missing

  def ShouldWait(self):
    """Decides if we should continue to wait for the builders to finish.

    This will be the retry function for timeout_util.WaitForSuccess, basically
    this function will return False if all builders finished or we see a
    problem with the builders.  Otherwise we'll return True to continue polling
    for the builders statuses.

    Returns:
      A bool of True if we should continue to wait and False if we should not.
    """
    # Check if all builders completed.
    if self.Completed():
      return False

    current_time = datetime.datetime.now()

    # Guess there are some builders building, check if there is a problem.
    if self.ShouldFailForBuilderStartTimeout(current_time):
      logging.error('Ending build since at least one builder has not started '
                    'within 5 mins.')
      return False

    # We got here which means no problems, we should still wait.
    logging.info('Still waiting for the following builds to complete: %r',
                 sorted(set(self.builders_array).difference(
                     self.GetCompleted())))
    return True


class BuildSpecsManager(object):
  """A Class to manage buildspecs and their states."""

  SLEEP_TIMEOUT = 1 * 60

  def __init__(self, source_repo, manifest_repo, build_names, incr_type, force,
               branch, manifest=constants.DEFAULT_MANIFEST, dry_run=True,
               master=False):
    """Initializes a build specs manager.

    Args:
      source_repo: Repository object for the source code.
      manifest_repo: Manifest repository for manifest versions / buildspecs.
      build_names: Identifiers for the build. Must match SiteConfig
          entries. If multiple identifiers are provided, the first item in the
          list must be an identifier for the group.
      incr_type: How we should increment this version - build|branch|patch
      force: Create a new manifest even if there are no changes.
      branch: Branch this builder is running on.
      manifest: Manifest to use for checkout. E.g. 'full' or 'buildtools'.
      dry_run: Whether we actually commit changes we make or not.
      master: Whether we are the master builder.
    """
    self.cros_source = source_repo
    buildroot = source_repo.directory
    if manifest_repo.startswith(site_config.params.INTERNAL_GOB_URL):
      self.manifest_dir = os.path.join(buildroot, 'manifest-versions-internal')
    else:
      self.manifest_dir = os.path.join(buildroot, 'manifest-versions')

    self.manifest_repo = manifest_repo
    self.build_names = build_names
    self.incr_type = incr_type
    self.force = force
    self.branch = branch
    self.manifest = manifest
    self.dry_run = dry_run
    self.master = master

    # Directories and specifications are set once we load the specs.
    self.buildspecs_dir = None
    self.all_specs_dir = None
    self.pass_dirs = None
    self.fail_dirs = None

    # Specs.
    self.latest = None
    self._latest_status = None
    self.latest_unprocessed = None
    self.compare_versions_fn = VersionInfo.VersionCompare

    self.current_version = None
    self.rel_working_dir = ''

  def _LatestSpecFromList(self, specs):
    """Find the latest spec in a list of specs.

    Args:
      specs: List of specs.

    Returns:
      The latest spec if specs is non-empty.
      None otherwise.
    """
    if specs:
      return max(specs, key=self.compare_versions_fn)

  def _LatestSpecFromDir(self, version_info, directory):
    """Returns the latest buildspec that match '*.xml' in a directory.

    Args:
      version_info: A VersionInfo object which will provide a build prefix
                    to match for.
      directory: Directory of the buildspecs.
    """
    if os.path.exists(directory):
      match_string = version_info.BuildPrefix() + '*.xml'
      specs = fnmatch.filter(os.listdir(directory), match_string)
      return self._LatestSpecFromList([os.path.splitext(m)[0] for m in specs])

  def RefreshManifestCheckout(self):
    """Checks out manifest versions into the manifest directory."""
    RefreshManifestCheckout(self.manifest_dir, self.manifest_repo)

  def InitializeManifestVariables(self, version_info=None, version=None):
    """Initializes manifest-related instance variables.

    Args:
      version_info: Info class for version information of cros. If None,
                    version must be specified instead.
      version: Requested version. If None, build the latest version.

    Returns:
      Whether the requested version was found.
    """
    assert version_info or version, 'version or version_info must be specified'
    working_dir = os.path.join(self.manifest_dir, self.rel_working_dir)
    specs_for_builder = os.path.join(working_dir, 'build-name', '%(builder)s')
    self.buildspecs_dir = os.path.join(working_dir, 'buildspecs')

    # If version is specified, find out what Chrome branch it is on.
    if version is not None:
      dirs = glob.glob(os.path.join(self.buildspecs_dir, '*', version + '.xml'))
      if len(dirs) == 0:
        return False
      assert len(dirs) <= 1, 'More than one spec found for %s' % version
      dir_pfx = os.path.basename(os.path.dirname(dirs[0]))
      version_info = VersionInfo(chrome_branch=dir_pfx, version_string=version)
    else:
      dir_pfx = version_info.chrome_branch

    self.all_specs_dir = os.path.join(self.buildspecs_dir, dir_pfx)
    self.pass_dirs, self.fail_dirs = [], []
    for build_name in self.build_names:
      specs_for_build = specs_for_builder % {'builder': build_name}
      self.pass_dirs.append(
          os.path.join(specs_for_build, constants.BUILDER_STATUS_PASSED,
                       dir_pfx))
      self.fail_dirs.append(
          os.path.join(specs_for_build, constants.BUILDER_STATUS_FAILED,
                       dir_pfx))

    # Calculate the status of the latest build, and whether the build was
    # processed.
    if version is None:
      self.latest = self._LatestSpecFromDir(version_info, self.all_specs_dir)
      if self.latest is not None:
        self._latest_status = self.GetBuildStatus(self.build_names[0],
                                                  self.latest)
        if self._latest_status.Missing():
          self.latest_unprocessed = self.latest

    return True

  def GetBuildSpecFilePath(self, milestone, platform):
    """Get the file path given milestone and platform versions.

    Args:
      milestone: a string representing milestone, e.g. '44'
      platform: a string representing platform version, e.g. '7072.0.0-rc4'

    Returns:
      A string, representing the path to its spec file.
    """
    return os.path.join(self.buildspecs_dir, milestone, platform + '.xml')

  def GetCurrentVersionInfo(self):
    """Returns the current version info from the version file."""
    version_file_path = self.cros_source.GetRelativePath(constants.VERSION_FILE)
    return VersionInfo(version_file=version_file_path, incr_type=self.incr_type)

  def HasCheckoutBeenBuilt(self):
    """Checks to see if we've previously built this checkout."""
    if self._latest_status and self._latest_status.Passed():
      latest_spec_file = '%s.xml' % os.path.join(
          self.all_specs_dir, self.latest)
      # We've built this checkout before if the manifest isn't different than
      # the last one we've built.
      return not self.cros_source.IsManifestDifferent(latest_spec_file)
    else:
      # We've never built this manifest before so this checkout is always new.
      return False

  def CreateManifest(self):
    """Returns the path to a new manifest based on the current checkout."""
    new_manifest = tempfile.mkstemp('manifest_versions.manifest')[1]
    osutils.WriteFile(new_manifest,
                      self.cros_source.ExportManifest(mark_revision=True))
    return new_manifest

  def GetNextVersion(self, version_info):
    """Returns the next version string that should be built."""
    version = version_info.VersionString()
    if self.latest == version:
      message = ('Automatic: %s - Updating to a new version number from %s' %
                 (self.build_names[0], version))
      version = version_info.IncrementVersion()
      version_info.UpdateVersionFile(message, dry_run=self.dry_run)
      assert version != self.latest
      logging.info('Incremented version number to  %s', version)

    return version

  def PublishManifest(self, manifest, version, build_id=None):
    """Publishes the manifest as the manifest for the version to others.

    Args:
      manifest: Path to manifest file to publish.
      version: Manifest version string, e.g. 6102.0.0-rc4
      build_id: Optional integer giving build_id of the build that is
                publishing this manifest. If specified and non-negative,
                build_id will be included in the commit message.
    """
    # Note: This commit message is used by master.cfg for figuring out when to
    #       trigger slave builders.
    commit_message = 'Automatic: Start %s %s %s' % (self.build_names[0],
                                                    self.branch, version)
    if build_id is not None and build_id >= 0:
      commit_message += '\nCrOS-Build-Id: %s' % build_id

    logging.info('Publishing build spec for: %s', version)
    logging.info('Publishing with commit message: %s', commit_message)
    logging.debug('Manifest contents below.\n%s', osutils.ReadFile(manifest))

    # Copy the manifest into the manifest repository.
    spec_file = '%s.xml' % os.path.join(self.all_specs_dir, version)
    osutils.SafeMakedirs(os.path.dirname(spec_file))

    shutil.copyfile(manifest, spec_file)

    # Actually push the manifest.
    self.PushSpecChanges(commit_message)

  def DidLastBuildFail(self):
    """Returns True if the last build failed."""
    return self._latest_status and self._latest_status.Failed()

  @staticmethod
  def GetBuildStatus(builder, version, retries=NUM_RETRIES):
    """Returns a BuilderStatus instance for the given the builder.

    Args:
      builder: Builder to look at.
      version: Version string.
      retries: Number of retries for getting the status.

    Returns:
      A BuilderStatus instance containing the builder status and any optional
      message associated with the status passed by the builder.  If no status
      is found for this builder then the returned BuilderStatus object will
      have status STATUS_MISSING.
    """
    url = BuildSpecsManager._GetStatusUrl(builder, version)
    ctx = gs.GSContext(retries=retries)
    try:
      output = ctx.Cat(url)
    except gs.GSNoSuchKey:
      return BuilderStatus(constants.BUILDER_STATUS_MISSING, None)

    return BuildSpecsManager._UnpickleBuildStatus(output)

  @staticmethod
  def GetSlaveStatusesFromCIDB(master_build_id):
    """Get statuses of slaves associated with |master_build_id|.

    Args:
      master_build_id: Master build id to check.

    Returns:
      A dictionary mapping the slave name to a status in
      BuildStatus.ALL_STATUSES.
    """
    status_dict = dict()
    db = cidb.CIDBConnectionFactory.GetCIDBConnectionForBuilder()
    assert db, 'No database connection to use.'
    status_list = db.GetSlaveStatuses(master_build_id)
    for d in status_list:
      status_dict[d['build_config']] = d['status']
    return status_dict

  def GetBuildersStatus(self, master_build_id, builders_array, timeout=3 * 60):
    """Get the statuses of the slave builders of the master.

    This function checks the status of slaves in |builders_array|. It
    queries CIDB for all builds associated with the |master_build_id|,
    then filters out builds that are not in |builders_array| (e.g.,
    slaves that are not important).

    Args:
      master_build_id: Master build id to check.
      builders_array: A list of the names of build configs to check.
      timeout: Number of seconds to wait for the results.

    Returns:
      A build_config name-> status dictionary of build statuses.
    """
    builders_completed = set()
    start_time = datetime.datetime.now()

    def _PrintRemainingTime(remaining):
      logging.info('%s until timeout...', remaining)

    # Check for build completion until all builders report in.
    builds_timed_out = False
    try:
      timeout_util.WaitForSuccess(
          lambda statuses: statuses.ShouldWait(),
          lambda: SlaveStatus(self.GetSlaveStatusesFromCIDB(master_build_id),
                              start_time, builders_array, builders_completed),
          timeout,
          period=self.SLEEP_TIMEOUT,
          side_effect_func=_PrintRemainingTime)
    except timeout_util.TimeoutError:
      builds_timed_out = True

    # Actually fetch the BuildStatus pickles from Google Storage.
    builder_statuses = {}
    for builder in builders_array:
      logging.debug("Checking for builder %s's status", builder)
      builder_status = self.GetBuildStatus(builder, self.current_version)
      builder_statuses[builder] = builder_status

    if builds_timed_out:
      logging.error('Not all builds finished before timeout (%d minutes)'
                    ' reached.', int((timeout / 60) + 0.5))

    return builder_statuses

  @staticmethod
  def _UnpickleBuildStatus(pickle_string):
    """Returns a BuilderStatus instance from a pickled string."""
    try:
      status_dict = cPickle.loads(pickle_string)
    except (cPickle.UnpicklingError, AttributeError, EOFError,
            ImportError, IndexError, TypeError) as e:
      # The above exceptions are listed as possible unpickling exceptions
      # by http://docs.python.org/2/library/pickle
      # In addition to the exceptions listed in the doc, we've also observed
      # TypeError in the wild.
      logging.warning('Failed with %r to unpickle status file.', e)
      return BuilderStatus(constants.BUILDER_STATUS_FAILED, message=None)

    return BuilderStatus(**status_dict)

  def GetLatestPassingSpec(self):
    """Get the last spec file that passed in the current branch."""
    version_info = self.GetCurrentVersionInfo()
    return self._LatestSpecFromDir(version_info, self.pass_dirs[0])

  def GetLocalManifest(self, version=None):
    """Return path to local copy of manifest given by version.

    Returns:
      Path of |version|.  By default if version is not set, returns the path
      of the current version.
    """
    if not self.all_specs_dir:
      raise BuildSpecsValueError('GetLocalManifest failed, BuildSpecsManager '
                                 'instance not yet initialized by call to '
                                 'InitializeManifestVariables.')
    if version:
      return os.path.join(self.all_specs_dir, version + '.xml')
    elif self.current_version:
      return os.path.join(self.all_specs_dir, self.current_version + '.xml')

    return None

  def BootstrapFromVersion(self, version):
    """Initialize a manifest from a release version returning the path to it."""
    # Only refresh the manifest checkout if needed.
    if not self.InitializeManifestVariables(version=version):
      self.RefreshManifestCheckout()
      if not self.InitializeManifestVariables(version=version):
        raise BuildSpecsValueError('Failure in BootstrapFromVersion. '
                                   'InitializeManifestVariables failed after '
                                   'RefreshManifestCheckout for version '
                                   '%s.' % version)

    # Return the current manifest.
    self.current_version = version
    return self.GetLocalManifest(self.current_version)

  def CheckoutSourceCode(self):
    """Syncs the cros source to the latest git hashes for the branch."""
    self.cros_source.Sync(self.manifest)

  def GetNextBuildSpec(self, retries=NUM_RETRIES, build_id=None):
    """Returns a path to the next manifest to build.

    Args:
      retries: Number of retries for updating the status.
      build_id: Optional integer cidb id of this build, which will be used to
                annotate the manifest-version commit if one is created.

    Raises:
      GenerateBuildSpecException in case of failure to generate a buildspec
    """
    last_error = None
    for index in range(0, retries + 1):
      try:
        self.CheckoutSourceCode()

        version_info = self.GetCurrentVersionInfo()
        self.RefreshManifestCheckout()
        self.InitializeManifestVariables(version_info)

        if not self.force and self.HasCheckoutBeenBuilt():
          return None

        # If we're the master, always create a new build spec. Otherwise,
        # only create a new build spec if we've already built the existing
        # spec.
        if self.master or not self.latest_unprocessed:
          git.CreatePushBranch(PUSH_BRANCH, self.manifest_dir, sync=False)
          version = self.GetNextVersion(version_info)
          new_manifest = self.CreateManifest()
          self.PublishManifest(new_manifest, version, build_id=build_id)
        else:
          version = self.latest_unprocessed

        self.current_version = version
        return self.GetLocalManifest(version)
      except cros_build_lib.RunCommandError as e:
        last_error = 'Failed to generate buildspec. error: %s' % e
        logging.error(last_error)
        logging.error('Retrying to generate buildspec:  Retry %d/%d', index + 1,
                      retries)

    # Cleanse any failed local changes and throw an exception.
    self.RefreshManifestCheckout()
    raise GenerateBuildSpecException(last_error)

  @staticmethod
  def _GetStatusUrl(builder, version):
    """Get the status URL in Google Storage for a given builder / version."""
    return os.path.join(BUILD_STATUS_URL, version, builder)

  def _UploadStatus(self, version, status, message=None, fail_if_exists=False,
                    dashboard_url=None):
    """Upload build status to Google Storage.

    Args:
      version: Version number to use. Must be a string.
      status: Status string.
      message: A failures_lib.BuildFailureMessage object with details
               of builder failure, or None (default).
      fail_if_exists: If set, fail if the status already exists.
      dashboard_url: Optional url linking to builder dashboard for this build.
    """
    data = BuilderStatus(status, message, dashboard_url).AsPickledDict()

    gs_version = None
    # This HTTP header tells Google Storage to return the PreconditionFailed
    # error message if the file already exists. Unfortunately, with new versions
    # of gsutil, PreconditionFailed is sometimes returned erroneously, so we've
    # replaced this check with # an Exists check below instead.
    # TODO(davidjames): Revert CL:223267 when Google Storage is fixed.
    #if fail_if_exists:
    #  gs_version = 0

    logging.info('Recording status %s for %s', status, self.build_names)
    for build_name in self.build_names:
      url = BuildSpecsManager._GetStatusUrl(build_name, version)

      ctx = gs.GSContext(dry_run=self.dry_run)
      # Check if the file already exists.
      if fail_if_exists and not self.dry_run and ctx.Exists(url):
        raise GenerateBuildSpecException('Builder already inflight')
      # Do the actual upload.
      ctx.Copy('-', url, input=data, version=gs_version)

  def UploadStatus(self, success, message=None, dashboard_url=None):
    """Uploads the status of the build for the current build spec.

    Args:
      success: True for success, False for failure
      message: A failures_lib.BuildFailureMessage object with details
               of builder failure, or None (default).
      dashboard_url: Optional url linking to builder dashboard for this build.
    """
    status = BuilderStatus.GetCompletedStatus(success)
    self._UploadStatus(self.current_version, status, message=message,
                       dashboard_url=dashboard_url)

  def SetInFlight(self, version, dashboard_url=None):
    """Marks the buildspec as inflight in Google Storage."""
    try:
      self._UploadStatus(version, constants.BUILDER_STATUS_INFLIGHT,
                         fail_if_exists=True,
                         dashboard_url=dashboard_url)
    except gs.GSContextPreconditionFailed:
      raise GenerateBuildSpecException('Builder already inflight')
    except gs.GSContextException as e:
      raise GenerateBuildSpecException(e)

  def _SetPassSymlinks(self, success_map):
    """Marks the buildspec as passed by creating a symlink in passed dir.

    Args:
      success_map: Map of config names to whether they succeeded.
    """
    src_file = '%s.xml' % os.path.join(self.all_specs_dir, self.current_version)
    for i, build_name in enumerate(self.build_names):
      if success_map[build_name]:
        sym_dir = self.pass_dirs[i]
      else:
        sym_dir = self.fail_dirs[i]
      dest_file = '%s.xml' % os.path.join(sym_dir, self.current_version)
      status = BuilderStatus.GetCompletedStatus(success_map[build_name])
      logging.debug('Build %s: %s -> %s', status, src_file, dest_file)
      CreateSymlink(src_file, dest_file)

  def PushSpecChanges(self, commit_message):
    """Pushes any changes you have in the manifest directory."""
    _PushGitChanges(self.manifest_dir, commit_message, dry_run=self.dry_run)

  def UpdateStatus(self, success_map, message=None, retries=NUM_RETRIES,
                   dashboard_url=None):
    """Updates the status of the build for the current build spec.

    Args:
      success_map: Map of config names to whether they succeeded.
      message: Message accompanied with change in status.
      retries: Number of retries for updating the status
      dashboard_url: Optional url linking to builder dashboard for this build.
    """
    last_error = None
    if message:
      logging.info('Updating status with message %s', message)
    for index in range(0, retries + 1):
      try:
        self.RefreshManifestCheckout()
        git.CreatePushBranch(PUSH_BRANCH, self.manifest_dir, sync=False)
        success = all(success_map.values())
        commit_message = ('Automatic checkin: status=%s build_version %s for '
                          '%s' % (BuilderStatus.GetCompletedStatus(success),
                                  self.current_version,
                                  self.build_names[0]))

        self._SetPassSymlinks(success_map)

        self.PushSpecChanges(commit_message)
      except cros_build_lib.RunCommandError as e:
        last_error = ('Failed to update the status for %s with the '
                      'following error %s' % (self.build_names[0],
                                              e.message))
        logging.error(last_error)
        logging.error('Retrying to generate buildspec:  Retry %d/%d', index + 1,
                      retries)
      else:
        # Upload status to Google Storage as well.
        self.UploadStatus(success, message=message, dashboard_url=dashboard_url)
        return

    # Cleanse any failed local changes and throw an exception.
    self.RefreshManifestCheckout()
    raise StatusUpdateException(last_error)


def _GetDefaultRemote(manifest_dom):
  """Returns the default remote in a manifest (if any).

  Args:
    manifest_dom: DOM Document object representing the manifest.

  Returns:
    Default remote if one exists, None otherwise.
  """
  default_nodes = manifest_dom.getElementsByTagName(DEFAULT_ELEMENT)
  if default_nodes:
    if len(default_nodes) > 1:
      raise FilterManifestException(
          'More than one <default> element found in manifest')
    return default_nodes[0].getAttribute(PROJECT_REMOTE_ATTR)
  return None


def _GetGroups(project_element):
  """Returns the default remote in a manifest (if any).

  Args:
    project_element: DOM Document object representing a project.

  Returns:
    List of names of the groups the project belongs too.
  """
  group = project_element.getAttribute(PROJECT_GROUP_ATTR)
  if not group:
    return []

  return [s.strip() for s in group.split(',')]


def FilterManifest(manifest, whitelisted_remotes=None, whitelisted_groups=None):
  """Returns a path to a new manifest with whitelists enforced.

  Args:
    manifest: Path to an existing manifest that should be filtered.
    whitelisted_remotes: Tuple of remotes to allow in the generated manifest.
      Only projects with those remotes will be included in the external
      manifest. (None means all remotes are acceptable)
    whitelisted_groups: Tuple of groups to allow in the generated manifest.
      (None means all groups are acceptable)

  Returns:
    Path to a new manifest that is a filtered copy of the original.
  """
  temp_fd, new_path = tempfile.mkstemp('external_manifest')
  manifest_dom = minidom.parse(manifest)
  manifest_node = manifest_dom.getElementsByTagName(MANIFEST_ELEMENT)[0]
  remotes = manifest_dom.getElementsByTagName(REMOTE_ELEMENT)
  projects = manifest_dom.getElementsByTagName(PROJECT_ELEMENT)
  pending_commits = manifest_dom.getElementsByTagName(PALADIN_COMMIT_ELEMENT)

  default_remote = _GetDefaultRemote(manifest_dom)

  # Remove remotes that don't match our whitelist.
  for remote_element in remotes:
    name = remote_element.getAttribute(REMOTE_NAME_ATTR)
    if (name is not None and
        whitelisted_remotes and
        name not in whitelisted_remotes):
      manifest_node.removeChild(remote_element)

  filtered_projects = set()
  for project_element in projects:
    project_remote = project_element.getAttribute(PROJECT_REMOTE_ATTR)
    project = project_element.getAttribute(PROJECT_NAME_ATTR)
    if not project_remote:
      if not default_remote:
        # This should not happen for a valid manifest. Either each
        # project must have a remote specified or there should
        # be manifest default we could use.
        raise FilterManifestException(
            'Project %s has unspecified remote with no default' % project)
      project_remote = default_remote

    groups = _GetGroups(project_element)

    filter_remote = (whitelisted_remotes and
                     project_remote not in whitelisted_remotes)

    filter_group = (whitelisted_groups and
                    not any([g in groups for g in whitelisted_groups]))

    if filter_remote or filter_group:
      filtered_projects.add(project)
      manifest_node.removeChild(project_element)

  for commit_element in pending_commits:
    if commit_element.getAttribute(
        PALADIN_PROJECT_ATTR) in filtered_projects:
      manifest_node.removeChild(commit_element)

  with os.fdopen(temp_fd, 'w') as manifest_file:
    # Filter out empty lines.
    filtered_manifest_noempty = filter(
        str.strip, manifest_dom.toxml('utf-8').splitlines())
    manifest_file.write(os.linesep.join(filtered_manifest_noempty))

  return new_path
