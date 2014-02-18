# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A library to generate and store the manifests for cros builders to use.
"""

import cPickle
import fnmatch
import glob
import logging
import os
import re
import shutil
import tempfile

from chromite.buildbot import constants, repository
from chromite.lib import cros_build_lib
from chromite.lib import git
from chromite.lib import gs
from chromite.lib import osutils


BUILD_STATUS_URL = '%s/builder-status' % constants.MANIFEST_VERSIONS_GS_URL
PUSH_BRANCH = 'temp_auto_checkin_branch'
NUM_RETRIES = 20


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


def _PushGitChanges(git_repo, message, dry_run=True, push_to=None):
  """Push the final commit into the git repo.

  Args:
    git_repo: git repo to push
    message: Commit message
    dry_run: If true, don't actually push changes to the server
    push_to: A git.RemoteRef object specifying the remote branch to push to.
      Defaults to the tracking branch of the current branch.
  """
  push_branch = None
  if push_to is None:
    remote, push_branch = git.GetTrackingBranch(
        git_repo, for_checkout=False, for_push=True)
    push_to = git.RemoteRef(remote, push_branch)

  git.RunGit(git_repo, ['add', '-A'])

  # It's possible that while we are running on dry_run, someone has already
  # committed our change.
  try:
    git.RunGit(git_repo, ['commit', '-m', message])
  except cros_build_lib.RunCommandError:
    if dry_run:
      return
    raise

  git.GitPush(git_repo, PUSH_BRANCH, push_to, dryrun=dry_run, force=dry_run)


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
    """Updates the version file by incrementing the patch component.

    Args:
      message: Commit message to use when incrementing the version.
      dry_run: Git dry_run.
    """
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
    """Update the version file with our current version."""

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

  @classmethod
  def VersionCompare(cls, version_string):
    """Useful method to return a comparable version of a LKGM string."""
    info = cls(version_string)
    return map(int, [info.build_number, info.branch_build_number,
                     info.patch_number])

  def BuildPrefix(self):
    """Returns the build prefix to match the buildspecs in  manifest-versions"""
    if self.incr_type == 'branch':
      if self.patch_number == '0':
        return '%s.' % self.build_number
      else:
        return '%s.%s.' % (self.build_number, self.branch_build_number)
    # Default to build incr_type.
    return ''


class BuilderStatus(object):
  """Object representing the status of a build."""
  # Various statuses builds can be in.  These status values are retrieved from
  # Google Storage, which each builder writes to.  The MISSING status is used
  # for the status of any builder which has no value in Google Storage.
  STATUS_FAILED = 'fail'
  STATUS_PASSED = 'pass'
  STATUS_INFLIGHT = 'inflight'
  STATUS_MISSING = 'missing' # i.e. never started.
  COMPLETED_STATUSES = (STATUS_PASSED, STATUS_FAILED)

  MISSING_MESSAGE = ('Unknown run, it probably never started:'
                     ' %(builder)s, version %(version)s')

  def __init__(self, status, message, dashboard_url=None):
    self.status = status
    self.message = message
    self.dashboard_url = dashboard_url

  @staticmethod
  def GetMissingMessage(builder, version):
    """Return the MISSING message to use for given |builder| and |version|."""
    args = {'builder': builder, 'version': version}
    return BuilderStatus.MISSING_MESSAGE % args

  # Helper methods to make checking the status object easy.

  def Failed(self):
    """Returns True if the Builder failed."""
    return self.status == BuilderStatus.STATUS_FAILED

  def Passed(self):
    """Returns True if the Builder passed."""
    return self.status == BuilderStatus.STATUS_PASSED

  def Inflight(self):
    """Returns True if the Builder is still inflight."""
    return self.status == BuilderStatus.STATUS_INFLIGHT

  def Missing(self):
    """Returns True if the Builder is missing any status."""
    return self.status == BuilderStatus.STATUS_MISSING

  def Completed(self):
    """Returns True if the Builder has completed."""
    return self.status in BuilderStatus.COMPLETED_STATUSES

  @classmethod
  def GetCompletedStatus(cls, success):
    """Return the appropriate status constant for a completed build.

    Args:
      success: Whether the build was successful or not.
    """
    if success:
      return cls.STATUS_PASSED
    else:
      return cls.STATUS_FAILED

  def AsFlatDict(self):
    """Returns a flat json-able representation of this builder status.

    Returns:
      A dictionary of the form {'status' : status, 'message' : message,
      'dashboard_url' : dashboard_url} where all values are guaranteed
      to be strings. If dashboard_url is None, the key will be excluded.
    """
    flat_dict = {'status' : str(self.status),
                 'message' : str(self.message)}
    if self.dashboard_url is not None:
      flat_dict['dashboard_url'] = str(self.dashboard_url)
    return flat_dict


class BuildSpecsManager(object):
  """A Class to manage buildspecs and their states."""

  def __init__(self, source_repo, manifest_repo, build_name, incr_type, force,
               branch, manifest=constants.DEFAULT_MANIFEST, dry_run=True,
               master=False):
    """Initializes a build specs manager.

    Args:
      source_repo: Repository object for the source code.
      manifest_repo: Manifest repository for manifest versions / buildspecs.
      build_name: Identifier for the build.  Must match cbuildbot_config.
      incr_type: How we should increment this version - build|branch|patch
      force: Create a new manifest even if there are no changes.
      branch: Branch this builder is running on.
      manifest: Manifest to use for checkout. E.g. 'full' or 'buildtools'.
      dry_run: Whether we actually commit changes we make or not.
      master: Whether we are the master builder.
    """
    self.cros_source = source_repo
    buildroot = source_repo.directory
    if manifest_repo.startswith(constants.INTERNAL_GOB_URL):
      self.manifest_dir = os.path.join(buildroot, 'manifest-versions-internal')
    else:
      self.manifest_dir = os.path.join(buildroot, 'manifest-versions')

    self.manifest_repo = manifest_repo
    self.build_name = build_name
    self.incr_type = incr_type
    self.force = force
    self.branch = branch
    self.manifest = manifest
    self.dry_run = dry_run
    self.master = master

    # Directories and specifications are set once we load the specs.
    self.all_specs_dir = None
    self.pass_dir = None
    self.fail_dir = None

    # Path to specs for builder.  Requires passing %(builder)s.
    self.specs_for_builder = None

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
    self.specs_for_builder = os.path.join(working_dir, 'build-name',
                                          '%(builder)s')
    specs_for_build = self.specs_for_builder % {'builder': self.build_name}
    buildspecs = os.path.join(working_dir, 'buildspecs')

    # If version is specified, find out what Chrome branch it is on.
    if version is not None:
      dirs = glob.glob(os.path.join(buildspecs, '*', version + '.xml'))
      if len(dirs) == 0:
        return False
      assert len(dirs) <= 1, 'More than one spec found for %s' % version
      dir_pfx = os.path.basename(os.path.dirname(dirs[0]))
      version_info = VersionInfo(chrome_branch=dir_pfx, version_string=version)
    else:
      dir_pfx = version_info.chrome_branch

    self.all_specs_dir = os.path.join(buildspecs, dir_pfx)
    self.pass_dir = os.path.join(specs_for_build,
                                 BuilderStatus.STATUS_PASSED, dir_pfx)
    self.fail_dir = os.path.join(specs_for_build,
                                 BuilderStatus.STATUS_FAILED, dir_pfx)

    # Calculate the status of the latest build, and whether the build was
    # processed.
    if version is None:
      self.latest = self._LatestSpecFromDir(version_info, self.all_specs_dir)
      if self.latest is not None:
        self._latest_status = self.GetBuildStatus(self.build_name, self.latest)
        if self._latest_status.Missing():
          self.latest_unprocessed = self.latest

    return True

  def GetCurrentVersionInfo(self):
    """Returns the current version info from the version file."""
    version_file_path = self.cros_source.GetRelativePath(constants.VERSION_FILE)
    return VersionInfo(version_file=version_file_path, incr_type=self.incr_type)

  def HasCheckoutBeenBuilt(self):
    """Checks to see if we've previously built this checkout.
    """
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
    """Returns the path to a new manifest based on the current source checkout.
    """
    new_manifest = tempfile.mkstemp('manifest_versions.manifest')[1]
    osutils.WriteFile(new_manifest,
                      self.cros_source.ExportManifest(mark_revision=True))
    return new_manifest

  def GetNextVersion(self, version_info):
    """Returns the next version string that should be built."""
    version = version_info.VersionString()
    if self.latest == version:
      message = ('Automatic: %s - Updating to a new version number from %s' % (
                 self.build_name, version))
      version = version_info.IncrementVersion()
      version_info.UpdateVersionFile(message, dry_run=self.dry_run)
      assert version != self.latest
      cros_build_lib.Info('Incremented version number to  %s', version)

    return version

  def PublishManifest(self, manifest, version):
    """Publishes the manifest as the manifest for the version to others."""
    logging.info('Publishing build spec for: %s\n%s', version,
                 osutils.ReadFile(manifest))

    # Note: This commit message is used by master.cfg for figuring out when to
    #       trigger slave builders.
    commit_message = 'Automatic: Start %s %s %s' % (self.build_name,
                                                    self.branch, version)

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
      output = ctx.Cat(url).output
    except gs.GSNoSuchKey:
      msg = BuilderStatus.GetMissingMessage(builder, version)
      return BuilderStatus(BuilderStatus.STATUS_MISSING, msg)

    return BuilderStatus(**cPickle.loads(output))

  def GetLatestPassingSpec(self):
    """Get the last spec file that passed in the current branch."""
    version_info = self.GetCurrentVersionInfo()
    return self._LatestSpecFromDir(version_info, self.pass_dir)

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
    """Initializes spec data from release version and returns path to manifest.
    """
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

  def GetNextBuildSpec(self, retries=NUM_RETRIES):
    """Returns a path to the next manifest to build.

    Args:
      retries: Number of retries for updating the status.

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
          self.PublishManifest(new_manifest, version)
        else:
          version = self.latest_unprocessed

        self.SetInFlight(version)
        self.current_version = version
        return self.GetLocalManifest(version)
      except cros_build_lib.RunCommandError as e:
        last_error = 'Failed to generate buildspec. error: %s' % e
        logging.error(last_error)
        logging.error('Retrying to generate buildspec:  Retry %d/%d', index + 1,
                      retries)
    else:
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
      message: A validation_pool.ValidationFailedMessage object with details
               of builder failure, or None (default).
      fail_if_exists: If set, fail if the status already exists.
      dashboard_url: Optional url linking to builder dashboard for this build.
    """
    url = BuildSpecsManager._GetStatusUrl(self.build_name, version)

    # Pickle the dictionary needed to recreate a BuilderStatus object.
    # NOTE: It's important here not to use BuilderStatus.AsFlatDict to create
    # the pickle dictionary, because that would flatten non-flat fields (like
    # message) into strings.
    data = cPickle.dumps(dict(status=status, message=message,
                              dashboard_url=dashboard_url))

    # This HTTP header tells Google Storage to return the PreconditionFailed
    # error message if the file already exists.
    gs_version = 0 if fail_if_exists else None

    # Do the actual upload.
    ctx = gs.GSContext(dry_run=self.dry_run)
    ctx.Copy('-', url, input=data, version=gs_version)

  def UploadStatus(self, success, message=None, dashboard_url=None):
    """Uploads the status of the build for the current build spec.

    Args:
      success: True for success, False for failure
      message: A validation_pool.ValidationFailedMessage object with details
               of builder failure, or None (default).
      dashboard_url: Optional url linking to builder dashboard for this build.
    """
    status = BuilderStatus.GetCompletedStatus(success)
    self._UploadStatus(self.current_version, status, message=message,
                       dashboard_url=dashboard_url)

  def SetInFlight(self, version):
    """Marks the buildspec as inflight in Google Storage."""
    try:
      self._UploadStatus(version, BuilderStatus.STATUS_INFLIGHT,
                         fail_if_exists=True)
    except cros_build_lib.RunCommandError as e:
      if 'code=PreconditionFailed' in e.result.error:
        raise GenerateBuildSpecException('Builder already inflight')
      raise GenerateBuildSpecException(e)

  def _SetFailed(self):
    """Marks the buildspec as failed by creating a symlink in fail dir."""
    filename = '%s.xml' % self.current_version
    dest_file = os.path.join(self.fail_dir, filename)
    src_file = os.path.join(self.all_specs_dir, filename)
    logging.debug('Setting build to failed  %s: %s', src_file, dest_file)
    CreateSymlink(src_file, dest_file)

  def _SetPassed(self):
    """Marks the buildspec as passed by creating a symlink in passed dir."""
    dest_file = '%s.xml' % os.path.join(self.pass_dir, self.current_version)
    src_file = '%s.xml' % os.path.join(self.all_specs_dir, self.current_version)
    logging.debug('Setting build to passed  %s: %s', src_file, dest_file)
    CreateSymlink(src_file, dest_file)

  def PushSpecChanges(self, commit_message):
    """Pushes any changes you have in the manifest directory."""
    _PushGitChanges(self.manifest_dir, commit_message, dry_run=self.dry_run)

  def UpdateStatus(self, success, message=None, retries=NUM_RETRIES):
    """Updates the status of the build for the current build spec.

    Args:
      success: True for success, False for failure
      message: Message accompanied with change in status.
      retries: Number of retries for updating the status
    """
    last_error = None
    if message:
      logging.info('Updating status with message %s', message)
    for index in range(0, retries + 1):
      try:
        self.RefreshManifestCheckout()
        git.CreatePushBranch(PUSH_BRANCH, self.manifest_dir, sync=False)
        commit_message = ('Automatic checkin: status=%s build_version %s for '
                          '%s' % (BuilderStatus.GetCompletedStatus(success),
                                  self.current_version,
                                  self.build_name))
        if success:
          self._SetPassed()
        else:
          self._SetFailed()

        self.PushSpecChanges(commit_message)
      except cros_build_lib.RunCommandError as e:
        last_error = ('Failed to update the status for %s with the '
                      'following error %s' % (self.build_name,
                                              e.message))
        logging.error(last_error)
        logging.error('Retrying to generate buildspec:  Retry %d/%d', index + 1,
                      retries)
      else:
        # Upload status to Google Storage as well.
        self.UploadStatus(success, message=message)
        return
    else:
      # Cleanse any failed local changes and throw an exception.
      self.RefreshManifestCheckout()
      raise StatusUpdateException(last_error)
