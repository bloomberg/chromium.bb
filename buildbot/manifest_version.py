# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
A library to generate and store the manifests for cros builders to use.
"""

import fnmatch
import logging
import os
import re
import shutil
import tempfile
import time

from chromite.buildbot import constants, repository
from chromite.lib import cros_build_lib as cros_lib


PUSH_BRANCH = 'temp_auto_checkin_branch'
NUM_RETRIES = 20


class VersionUpdateException(Exception):
  """Exception gets thrown for failing to update the version file"""
  pass


class StatusUpdateException(Exception):
  """Exception gets thrown for failure to update the status"""
  pass


class GenerateBuildSpecException(Exception):
  """Exception gets thrown for failure to Generate a buildspec for the build"""
  pass


def _GitCleanAndCheckoutOrigin(git_repo):
  """Remove all local changes and checkout the latest origin.

  All local changes in the supplied repo will be removed. The branch will
  also be switched to a detached head pointing at the latest origin.

  Args:
    git_repo: Directory of git repository.
  """
  remote, push_branch = cros_lib.GetPushBranch(git_repo)
  cros_lib.RunCommand(['git', 'remote', 'update', remote], cwd=git_repo)
  cros_lib.RunCommand(['git', 'clean', '-d', '-f'], cwd=git_repo)
  cros_lib.RunCommand(['git', 'reset', '--hard', 'HEAD'], cwd=git_repo)
  cros_lib.RunCommand(['git', 'checkout', '%s/%s' % (remote, push_branch)],
                      cwd=git_repo)


def RefreshManifestCheckout(manifest_dir, manifest_repo):
  """Checks out manifest-versions into the manifest directory.

  If a repository is already present, it will be cleansed of any local
  changes and restored to its pristine state, checking out the origin.
  """
  reinitialize = True
  if os.path.exists(manifest_dir):
    result = cros_lib.RunCommand(['git', 'config', 'remote.origin.url'],
                                 cwd=manifest_dir, print_cmd=False,
                                 redirect_stdout=True, error_code_ok=True)
    if (result.returncode == 0 and
        result.output.rstrip() == manifest_repo):
      logging.info('Updating manifest-versions checkout.')
      try:
        _GitCleanAndCheckoutOrigin(manifest_dir)
      except cros_lib.RunCommandError:
        logging.warning('Could not update manifest-versions checkout.')
      else:
        reinitialize = False

  if reinitialize:
    logging.info('Cloning fresh manifest-versions checkout.')
    _RemoveDirs(manifest_dir)
    repository.CloneGitRepo(manifest_dir, manifest_repo)


def _PushGitChanges(git_repo, message, dry_run=True):
  """Push the final commit into the git repo.

  Args:
    git_repo: git repo to push
    message: Commit message
    dry_run: If true, don't actually push changes to the server
  """
  remote, push_branch = cros_lib.GetPushBranch(git_repo)
  cros_lib.RunCommand(['git', 'add', '-A'], cwd=git_repo)

  # It's possible that while we are running on dry_run, someone has already
  # committed our change.
  try:
    cros_lib.RunCommand(['git', 'commit', '-m', message], cwd=git_repo)
  except cros_lib.RunCommandError:
    if dry_run:
      return
    raise

  push_cmd = ['git', 'push', remote, '%s:%s' % (PUSH_BRANCH, push_branch)]
  if dry_run:
    push_cmd.extend(['--dry-run', '--force'])

  cros_lib.RunCommand(push_cmd, cwd=git_repo)


def _RemoveDirs(dir_name):
  """Remove directories recursively, if they exist"""
  if os.path.exists(dir_name):
    shutil.rmtree(dir_name)


def CreateSymlink(src_file, dest_file, remove_file=None):
  """Creates a relative symlink from src to dest with optional removal of file.

  More robust symlink creation that creates a relative symlink from src_file to
  dest_file.  Also if remove_file is set, removes symlink there.

  This is useful for multiple calls of CreateSymlink where you are using
  the dest_file location to store information about the status of the src_file.

  Args:
    src_file: source for the symlink
    dest_file: destination for the symlink
    remove_file: symlink that needs to be deleted for clearing the old state
  """
  dest_dir = os.path.dirname(dest_file)
  if os.path.lexists(dest_file): os.unlink(dest_file)
  if not os.path.exists(dest_dir): os.makedirs(dest_dir)

  rel_src_file = os.path.relpath(src_file, dest_dir)
  logging.debug('Linking %s to %s', rel_src_file, dest_file)
  os.symlink(rel_src_file, dest_file)

  if remove_file and os.path.lexists(remove_file):
    logging.debug('REMOVE: Removing  %s', remove_file)
    os.unlink(remove_file)


class VersionInfo(object):
  """Class to encapsulate the Chrome OS version info scheme.

  You can instantiate this class in two ways.
  1)using a version file, specifically chromeos_version.sh,
  which contains the version information.
  2) passing in a string with the 3 version components ()
  Args:
    version_string: Optional 3 component version string to parse.  Contains:
        build_number: release build number.
        branch_build_number: current build number on a branch.
        patch_number: patch number.
    chrome_branch: If version_string specified, specify chrome_branch i.e. 13.
    incr_type: How we should increment this version - build|branch|patch
    version_file: version file location.
  """
  # Pattern for matching build name format.  Includes chrome branch hack.
  VER_PATTERN = '(\d+).(\d+).(\d+)(?:-R(\d+))*'

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
    returns:
       None: on a non match
       value: for a matching key
    """
    regex = '.*(%s)\s*=\s*(\d+)$' % key

    match = re.match(regex, line)
    if match:
      return match.group(2)
    return None

  def IncrementVersion(self, message, dry_run):
    """Updates the version file by incrementing the patch component.
    Args:
      message:  Commit message to use when incrementing the version.
      dry_run: Git dry_run.
    """
    def IncrementOldValue(line, key, new_value):
      """Change key to new_value if found on line.  Returns True if changed."""
      old_value = self.FindValue(key, line)
      if old_value:
        temp_fh.write(line.replace(old_value, new_value, 1))
        return True
      else:
        return False

    if not self.version_file:
      raise VersionUpdateException('Cannot call IncrementVersion without '
                                   'an associated version_file')
    if not self.incr_type or self.incr_type not in ('build', 'branch'):
      raise VersionUpdateException('Need to specify the part of the version to'
                                   ' increment')

    if self.incr_type == 'build':
      self.build_number = str(int(self.build_number) + 1)
      self.branch_build_number = '0'
      self.patch_number = '0'
    elif self.patch_number == '0':
      self.branch_build_number = str(int(self.branch_build_number) + 1)
    else:
      self.patch_number = str(int(self.patch_number) + 1)

    temp_file = tempfile.mkstemp(suffix='mvp', prefix='tmp', dir=None,
                                 text=True)[1]
    with open(self.version_file, 'r') as source_version_fh:
      with open(temp_file, 'w') as temp_fh:
        for line in source_version_fh:
          if IncrementOldValue(line, 'CHROMEOS_BUILD', self.build_number):
            pass
          elif IncrementOldValue(line, 'CHROMEOS_BRANCH',
                                 self.branch_build_number):
            pass
          elif IncrementOldValue(line, 'CHROMEOS_PATCH', self.patch_number):
            pass
          else:
            temp_fh.write(line)

        temp_fh.close()

      source_version_fh.close()

    repo_dir = os.path.dirname(self.version_file)

    try:
      cros_lib.CreatePushBranch(PUSH_BRANCH, repo_dir)

      shutil.copyfile(temp_file, self.version_file)
      os.unlink(temp_file)

      _PushGitChanges(repo_dir, message, dry_run=dry_run)
    finally:
      # Update to the remote version that contains our changes. This is needed
      # to ensure that we don't build a release using a local commit.
      _GitCleanAndCheckoutOrigin(repo_dir)

    return self.VersionString()

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

  def DirPrefix(self):
    """Returns the sub directory suffix in manifest-versions"""
    return self.chrome_branch

  def BuildPrefix(self):
    """Returns the build prefix to match the buildspecs in  manifest-versions"""
    if self.incr_type == 'branch':
      if self.patch_number == '0':
        return self.build_number
      else:
        return '%s.%s' % (self.build_number, self.branch_build_number)
    # Default to build incr_type.
    return ''


class BuildSpecsManager(object):
  """A Class to manage buildspecs and their states."""
  # Various status builds can be in.
  STATUS_FAILED = 'fail'
  STATUS_PASSED = 'pass'
  STATUS_INFLIGHT = 'inflight'
  STATUS_COMPLETED = [STATUS_PASSED, STATUS_FAILED]

  # Max timeout before assuming other builders have failed.
  LONG_MAX_TIMEOUT_SECONDS = 1200

  def __init__(self, source_repo, manifest_repo, build_name,
               incr_type, force, dry_run=True):
    """Initializes a build specs manager.
    Args:
      source_repo: Repository object for the source code.
      manifest_repo:  Manifest repository for manifest versions / buildspecs.
      build_name: Identifier for the build.  Must match cbuildbot_config.
      incr_type: part of the version to increment. 'patch or branch'
      force: Create a new manifest even if there are no changes.
      dry_run: Whether we actually commit changes we make or not.
    """
    self.cros_source = source_repo
    buildroot = source_repo.directory
    if manifest_repo.startswith(constants.GERRIT_INT_SSH_URL):
      self.manifest_dir = os.path.join(buildroot, 'manifest-versions-internal')
    else:
      self.manifest_dir = os.path.join(buildroot, 'manifest-versions')

    self.manifest_repo = manifest_repo
    self.build_name = build_name
    self.incr_type = incr_type
    self.force = force
    self.dry_run = dry_run

    # Directories and specifications are set once we load the specs.
    self.all_specs_dir = None
    self.pass_dir = None
    self.fail_dir = None
    self.inflight_dir = None

    # Path to specs for builder.  Requires passing %(builder)s.
    self.specs_for_builder = None

    # Specs.
    self.latest = None
    self.latest_passed = None
    self.latest_processed = None
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
      directory: Directory of the buildspecs.
    """
    if os.path.exists(directory):
      match_string = version_info.BuildPrefix() + '*.xml'
      specs = fnmatch.filter(os.listdir(directory), match_string)
      return self._LatestSpecFromList([os.path.splitext(m)[0] for m in specs])

  def _GetSpecAge(self, version):
    cmd = ['git', 'log', '-1', '--format=%ct', '%s.xml' % version]
    result = cros_lib.RunCommand(cmd, cwd=self.all_specs_dir,
                                 redirect_stdout=True)
    return time.time() - int(result.output.strip())

  def RefreshManifestCheckout(self):
    """Checks out manifest versions into the manifest directory."""
    RefreshManifestCheckout(self.manifest_dir, self.manifest_repo)

  def InitializeManifestVariables(self, version_info):
    """Initializes manifest-related instance variables.

    Args:
      version_info: Info class for version information of cros.
    """
    working_dir = os.path.join(self.manifest_dir, self.rel_working_dir)
    dir_pfx = version_info.DirPrefix()
    self.specs_for_builder = os.path.join(working_dir, 'build-name',
                                          '%(builder)s')
    specs_for_build = self.specs_for_builder % {'builder': self.build_name}
    self.all_specs_dir = os.path.join(working_dir, 'buildspecs', dir_pfx)
    self.pass_dir = os.path.join(specs_for_build,
                                 BuildSpecsManager.STATUS_PASSED, dir_pfx)
    self.fail_dir = os.path.join(specs_for_build,
                                 BuildSpecsManager.STATUS_FAILED, dir_pfx)
    self.inflight_dir = os.path.join(specs_for_build,
                                     BuildSpecsManager.STATUS_INFLIGHT, dir_pfx)

    # Calculate latest build that passed or failed.
    dirs = (self.pass_dir, self.fail_dir)
    specs = [self._LatestSpecFromDir(version_info, d) for d in dirs]
    self.latest_processed = self._LatestSpecFromList(filter(None, specs))
    self.latest_passed = specs[0]

    # Calculate latest unprocessed spec (that is newer than
    # LONG_MAX_TIMEOUT_SECONDS). We only consider a spec unprocessed if we
    # have not finished a build with that spec yet.
    self.latest = self._LatestSpecFromDir(version_info, self.all_specs_dir)
    self.latest_unprocessed = None
    if (self.latest != self.latest_processed and
        self._GetSpecAge(self.latest) < self.LONG_MAX_TIMEOUT_SECONDS):
      self.latest_unprocessed = self.latest

  def GetCurrentVersionInfo(self):
    """Returns the current version info from the version file."""
    version_file_path = self.cros_source.GetRelativePath(constants.VERSION_FILE)
    return VersionInfo(version_file=version_file_path, incr_type=self.incr_type)

  def HasCheckoutBeenBuilt(self):
    """Checks to see if we've previously built this checkout.
    """
    if self.latest_passed and self.latest == self.latest_passed:
      latest_spec_file = '%s.xml' % os.path.join(
          self.all_specs_dir, self.latest_processed)
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
    self.cros_source.ExportManifest(new_manifest)
    return new_manifest

  def GetNextVersion(self, version_info):
    """Returns the next version string that should be built."""
    version = version_info.VersionString()
    if self.latest == version:
      message = ('Automatic: %s - Updating to a new version number from %s' % (
                 self.build_name, version))
      version = version_info.IncrementVersion(message, dry_run=self.dry_run)
      logging.debug('Incremented version number to  %s', version)

    return version

  def PublishManifest(self, manifest, version):
    """Publishes the manifest as the manifest for the version to others."""
    logging.info('Publishing build spec for: %s', version)
    commit_message = 'Automatic: Start %s %s' % (self.build_name, version)

    # Copy the manifest into the manifest repository.
    spec_file = '%s.xml' % os.path.join(self.all_specs_dir, version)
    if not os.path.exists(os.path.dirname(spec_file)):
      os.makedirs(os.path.dirname(spec_file))

    shutil.copyfile(manifest, spec_file)

    # Actually push the manifest.
    self.SetInFlight(version)
    self.PushSpecChanges(commit_message)

  def DidLastBuildSucceed(self):
    """Returns True if this is our first build or the last build succeeded."""
    return self.latest_processed == self.latest_passed

  def GetBuildStatus(self, builder, version_info):
    """Given a builder, version, verison_info returns the build status."""
    xml_name = self.current_version + '.xml'
    dir_pfx = version_info.DirPrefix()
    specs_for_build = self.specs_for_builder % {'builder': builder}
    pass_file = os.path.join(specs_for_build, self.STATUS_PASSED, dir_pfx,
                             xml_name)
    fail_file = os.path.join(specs_for_build, self.STATUS_FAILED, dir_pfx,
                             xml_name)
    inflight_file = os.path.join(specs_for_build, self.STATUS_INFLIGHT, dir_pfx,
                                 xml_name)

    if os.path.lexists(pass_file):
      return BuildSpecsManager.STATUS_PASSED
    elif os.path.lexists(fail_file):
      return BuildSpecsManager.STATUS_FAILED
    elif os.path.lexists(inflight_file):
      return BuildSpecsManager.STATUS_INFLIGHT
    else:
      return None

  def GetLocalManifest(self, version=None):
    """Return path to local copy of manifest given by version.

    Returns path of version.  By default if version is not set, returns the path
    of the current version.
    """
    if version:
      return os.path.join(self.all_specs_dir, version + '.xml')
    elif self.current_version:
      return os.path.join(self.all_specs_dir, self.current_version + '.xml')

    return None

  def BootstrapFromVersion(self, version):
    """Initializes spec data from release version and returns path to manifest.
    """
    version_info = self.GetCurrentVersionInfo()
    should_initialize_manifest_repo = True
    if version:
      # We need to first set up some variables. This is harmless even if we
      # don't have the manifests checked out yet.
      self.InitializeManifestVariables(version_info)
      # We don't need to reload the manifests repository if we already have the
      # manifest.
      if os.path.exists(self.GetLocalManifest(version)):
        should_initialize_manifest_repo = False

    if should_initialize_manifest_repo:
      self.RefreshManifestCheckout()
      self.InitializeManifestVariables(version_info)

    self.current_version = version
    return self.GetLocalManifest(self.current_version)

  def CheckoutSourceCode(self):
    """Syncs the cros source to the latest git hashes for the branch."""
    self.cros_source.Sync(repository.RepoRepository.DEFAULT_MANIFEST,
                          cleanup=False)

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

        cros_lib.CreatePushBranch(PUSH_BRANCH, self.manifest_dir, sync=False)
        if not self.latest_unprocessed:
          version = self.GetNextVersion(version_info)
          new_manifest = self.CreateManifest()
          self.PublishManifest(new_manifest, version)
        else:
          version = self.latest_unprocessed
          self.SetInFlight(version)
          self.PushSpecChanges('Automatic: Start %s %s' % (self.build_name,
                                                           version))

        self.current_version = version
        return self.GetLocalManifest(version)
      except cros_lib.RunCommandError as e:
        last_error = 'Failed to generate buildspec. error: %s' % e
        logging.error(last_error)
        logging.error('Retrying to generate buildspec:  Retry %d/%d', index + 1,
                      retries)
    else:
      # Cleanse any failed local changes and throw an exception.
      self.RefreshManifestCheckout()
      raise GenerateBuildSpecException(last_error)

  def SetInFlight(self, version):
    """Marks the buildspec as inflight by creating a symlink in inflight dir."""
    dest_file = '%s.xml' % os.path.join(self.inflight_dir, version)
    src_file = '%s.xml' % os.path.join(self.all_specs_dir, version)
    logging.debug('Setting build in flight  %s: %s', src_file, dest_file)
    CreateSymlink(src_file, dest_file)

  def _SetFailed(self):
    """Marks the buildspec as failed by creating a symlink in fail dir."""
    dest_file = '%s.xml' % os.path.join(self.fail_dir, self.current_version)
    src_file = '%s.xml' % os.path.join(self.all_specs_dir, self.current_version)
    remove_file = '%s.xml' % os.path.join(self.inflight_dir,
                                          self.current_version)
    logging.debug('Setting build to failed  %s: %s', src_file, dest_file)
    CreateSymlink(src_file, dest_file, remove_file)

  def _SetPassed(self):
    """Marks the buildspec as passed by creating a symlink in passed dir."""
    dest_file = '%s.xml' % os.path.join(self.pass_dir, self.current_version)
    src_file = '%s.xml' % os.path.join(self.all_specs_dir, self.current_version)
    remove_file = '%s.xml' % os.path.join(self.inflight_dir,
                                          self.current_version)
    logging.debug('Setting build to passed  %s: %s', src_file, dest_file)
    CreateSymlink(src_file, dest_file, remove_file)

  def PushSpecChanges(self, commit_message):
    """Pushes any changes you have in the manifest directory."""
    _PushGitChanges(self.manifest_dir, commit_message,
                    dry_run=self.dry_run)

  def UpdateStatus(self, success, retries=NUM_RETRIES):
    """Updates the status of the build for the current build spec.
    Args:
      success: True for success, False for failure
      retries: Number of retries for updating the status
    """
    last_error = None
    for index in range(0, retries + 1):
      try:
        self.RefreshManifestCheckout()
        cros_lib.CreatePushBranch(PUSH_BRANCH, self.manifest_dir, sync=False)
        status = self.STATUS_PASSED if success else self.STATUS_FAILED
        commit_message = ('Automatic checkin: status=%s build_version %s for '
                          '%s' % (status,
                                  self.current_version,
                                  self.build_name))
        if success:
          self._SetPassed()
        else:
          self._SetFailed()

        self.PushSpecChanges(commit_message)
      except cros_lib.RunCommandError as e:
        last_error = ('Failed to update the status for %s with the '
                      'following error %s' % (self.build_name,
                                              e.message))
        logging.error(last_error)
        logging.error('Retrying to generate buildspec:  Retry %d/%d', index + 1,
                      retries)
      else:
        return
    else:
      # Cleanse any failed local changes and throw an exception.
      self.RefreshManifestCheckout()
      raise StatusUpdateException(last_error)
