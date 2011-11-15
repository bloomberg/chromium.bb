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

from chromite.buildbot import constants
from chromite.buildbot import repository
from chromite.lib import cros_build_lib as cros_lib

logging_format = '%(asctime)s - %(filename)s - %(levelname)-8s: %(message)s'
date_format = '%Y/%m/%d %H:%M:%S'
logging.basicConfig(level=logging.DEBUG, format=logging_format,
                    datefmt=date_format)

PUSH_BRANCH = 'temp_auto_checkin_branch'

class VersionUpdateException(Exception):
  """Exception gets thrown for failing to update the version file"""
  pass


class GitCommandException(Exception):
  """Exception gets thrown for a git command that fails to execute."""
  pass


class StatusUpdateException(Exception):
  """Exception gets thrown for failure to update the status"""
  pass


class GenerateBuildSpecException(Exception):
  """Exception gets thrown for failure to Generate a buildspec for the build"""
  pass


def _GitCleanDirectory(directory):
  """"Clean git repo chanages.

  raises: GitCommandException: when fails to clean.
  """
  try:
    cros_lib.RunCommand(['git', 'clean', '-d', '-f'], cwd=directory)
    cros_lib.RunCommand(['git', 'reset', '--hard', 'HEAD'], cwd=directory)
  except cros_lib.RunCommandError, e:
    err_msg = 'Failed to clean git "%s" %s' % (directory, e.message)
    logging.error(err_msg)
    raise GitCommandException(err_msg)


def PrepForChanges(git_repo, dry_run):
  """Prepare a git/repo repository for making changes. It should
     have no files modified when you call this.
  Args:
    git_repo: git repo to push
    dry_run: Run but we are not planning on pushing changes for real.
  raises: GitCommandException
  """
  _GitCleanDirectory(git_repo)
  try:
    if repository.InARepoRepository(git_repo):
      cros_lib.RunCommand(['repo', 'abandon', PUSH_BRANCH, '.'],
                          cwd=git_repo, error_ok=True)
      cros_lib.RunCommand(['repo', 'sync', '.'], cwd=git_repo)
      cros_lib.RunCommand(['repo', 'start', PUSH_BRANCH, '.'], cwd=git_repo)
    else:
      # Attempt the equivalent of repo abandon for retries.  Master always
      # exists for manifest_version git repos.
      try:
        cros_lib.RunCommand(['git', 'checkout', 'master'], cwd=git_repo)
        if not dry_run:
          cros_lib.RunCommand(['git', 'branch', '-D', PUSH_BRANCH],
                              cwd=git_repo)
      except:
        pass

      remote, branch = cros_lib.GetPushBranch('master', cwd=git_repo)
      cros_lib.RunCommand(['git', 'remote', 'update'], cwd=git_repo)
      if not cros_lib.DoesLocalBranchExist(git_repo, PUSH_BRANCH):
        cros_lib.RunCommand(['git', 'checkout', '-b', PUSH_BRANCH, '-t',
                             '/'.join([remote, branch])], cwd=git_repo)
      else:
        # If the branch exists, we must be dry run.   Checkout to branch.
        assert dry_run, 'Failed to previously delete push branch.'
        cros_lib.RunCommand(['git', 'checkout', PUSH_BRANCH], cwd=git_repo)

    cros_lib.RunCommand(['git', 'config', 'push.default', 'tracking'],
                        cwd=git_repo)

    # TODO Test fix for chromium-os:16249
    # repository.FixExternalRepoPushUrls(git_repo)
    cros_lib.RunCommand(['git',
                         'config',
                         'url.ssh://gerrit.chromium.org:29418.insteadof',
                         'http://git.chromium.org'], cwd=git_repo)

  except cros_lib.RunCommandError, e:
    err_msg = 'Failed to prep for edit in %s with %s' % (git_repo, e.message)
    logging.error(err_msg)
    git_status = cros_lib.RunCommand(['git', 'status'], cwd=git_repo)
    logging.error('Current repo %s status: %s', git_repo, git_status)
    _GitCleanDirectory(git_repo)
    raise GitCommandException(err_msg)


def _PushGitChanges(git_repo, message, dry_run=True):
  """Do the final commit into the git repo
  Args:
    git_repo: git repo to push
    message: Commit message
    dry_run: If true, don't actually push changes to the server
  raises: GitCommandException
  """
  try:
    remote, push_branch = cros_lib.GetPushBranch(PUSH_BRANCH, cwd=git_repo)
    cros_lib.RunCommand(['git', 'add', '-A'], cwd=git_repo)
    cros_lib.RunCommand(['git', 'commit', '-am', message], cwd=git_repo)
    push_cmd = ['git', 'push', remote, '%s:%s' % (PUSH_BRANCH, push_branch)]
    if dry_run: push_cmd.append('--dry-run')
    cros_lib.RunCommand(push_cmd, cwd=git_repo)
  except cros_lib.RunCommandError, e:
    err_msg = 'Failed to commit to %s' % e.message
    logging.error(err_msg)
    git_status = cros_lib.RunCommand(['git', 'status'], cwd=git_repo)
    logging.error('Current repo %s status:\n%s', git_repo, git_status)
    _GitCleanDirectory(git_repo)
    raise GitCommandException(err_msg)
  finally:
    if repository.InARepoRepository(git_repo):
      # Needed for chromeos version file.  Otherwise on increment, we leave
      # local commit behind in tree.
      cros_lib.RunCommand(['repo', 'abandon', PUSH_BRANCH], cwd=git_repo,
                          error_ok=True)


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
    # TODO(sosa): Remove default.
    self.chrome_branch = '14'
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
    if not self.incr_type:
      raise VersionUpdateException('Need to specify the part of the version to'
                                   ' increment')

    if self.incr_type == 'build':
      self.build_number = str(int(self.build_number) + 1)
      self.branch_build_number = '0'
      self.patch_number = '0'

    if self.incr_type == 'branch':
      self.branch_build_number = str(int(self.branch_build_number) + 1)
      self.patch_number = '0'

    if self.incr_type == 'patch':
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

    PrepForChanges(repo_dir, dry_run)

    shutil.copyfile(temp_file, self.version_file)
    os.unlink(temp_file)

    _PushGitChanges(repo_dir, message, dry_run=dry_run)

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
    if self.incr_type == 'patch':
      return '%s.%s' % (self.build_number, self.branch_build_number)

    if self.incr_type == 'branch':
      return self.build_number

    # Default to build incr_type.
    return ''


class BuildSpecsManager(object):
  """A Class to manage buildspecs and their states."""
  _TMP_MANIFEST_DIR = '/tmp/manifests'
  # Various status builds can be in.
  STATUS_FAILED = 'fail'
  STATUS_PASSED = 'pass'
  STATUS_INFLIGHT = 'inflight'
  STATUS_COMPLETED = [STATUS_PASSED, STATUS_FAILED]

  @classmethod
  def GetManifestDir(cls):
    """Get the directory where specs are checked out to."""
    return cls._TMP_MANIFEST_DIR

  def __init__(self, source_dir, checkout_repo, manifest_repo, branch,
               build_name, incr_type, dry_run=True):
    """Initializes a build specs manager.
    Args:
      source_dir: Directory to which we checkout out source code.
      checkout_repo:  Checkout repository for cros.
      manifest_repo:  Manifest repository for manifest versions / buildspecs.
        branch: The branch.
      build_name: Identifier for the build.  Must match cbuildbot_config.
      incr_type: part of the version to increment. 'patch or branch'
      dry_run: Whether we actually commit changes we make or not.
    """
    self.cros_source = repository.RepoRepository(
        checkout_repo, source_dir, branch=branch)
    self.manifest_repo = manifest_repo
    self.branch = branch
    self.build_name = build_name
    self.incr_type = incr_type
    self.dry_run = dry_run

    # Directories and specifications are set once we load the specs.
    self.all_specs_dir = None
    self.pass_dir = None
    self.fail_dir = None
    self.inflight_dir = None

    # A list of versions this builder has successfully built.
    self.passed = None

    # Path to specs for builder.  Requires passing %(builder)s.
    self.specs_for_builder = None

    # Specs.
    self.all = None
    self.unprocessed = None
    self.latest = None
    self.latest_unprocessed = None
    self.compare_versions_fn = VersionInfo.VersionCompare

    self.current_version = None
    self.rel_working_dir = ''

  def _GetMatchingSpecs(self, version_info, directory):
    """Returns the sorted list of buildspecs that match '*.xml in a directory.'
    Args:
      version_info: Info class for version information of cros.
      directory: Directory of the buildspecs.
    """
    matched_manifests = []
    if os.path.exists(directory):
      all_manifests = os.listdir(directory)
      match_string = version_info.BuildPrefix() + '*.xml'
      matched_manifests = fnmatch.filter(all_manifests, match_string)
      matched_manifests = [os.path.splitext(m)[0] for m in matched_manifests]

    return sorted(matched_manifests, key=self.compare_versions_fn)

  def _LoadSpecs(self, version_info, version=None):
    """Loads the specifications from the working directory.
    Args:
      version_info: Info class for version information of cros.
      version: Checks to see if versioned manifest exists, if it does, does
        not re-check out repository.
    """
    working_dir = os.path.join(self._TMP_MANIFEST_DIR, self.rel_working_dir)
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

    # Conservatively grab the latest manifest versions repository.
    # Note:  This is key to some of the Git push logic for non-repos for
    # local developers.  Also note that we don't need to do this if we already
    # have the version checked out that we are being forced to use.
    if not version or not os.path.exists(self.GetLocalManifest(version)):
      _RemoveDirs(self._TMP_MANIFEST_DIR)
      repository.CloneGitRepo(self._TMP_MANIFEST_DIR, self.manifest_repo)

    # Build lists of specs.
    self.all = self._GetMatchingSpecs(version_info, self.all_specs_dir)

    # Build list of unprocessed specs.
    self.passed = self._GetMatchingSpecs(version_info, self.pass_dir)
    failed = self._GetMatchingSpecs(version_info, self.fail_dir)
    inflight = self._GetMatchingSpecs(version_info, self.inflight_dir)
    processed = sorted(set(self.passed + failed + inflight),
                            key=self.compare_versions_fn)
    self.unprocessed = sorted(set(self.all).difference(set(processed)),
                              key=self.compare_versions_fn)

    if self.all: self.latest = self.all[-1]
    latest_processed = None
    if processed:
      latest_processed = processed[-1]
      logging.debug('Last processed build for %s is %s' % (
          self.build_name, latest_processed))

      # Remove unprocessed candidates that are older than the latest processed.
      to_be_removed = []
      for build in self.unprocessed:
        build1 = self.compare_versions_fn(build)
        build2 = self.compare_versions_fn(latest_processed)

        if build1 > build2:
          logging.debug('Still need to build %s' % build)
        else:
          to_be_removed.append(build)

      for build in to_be_removed:
        self.unprocessed.remove(build)

    if self.unprocessed: self.latest_unprocessed = self.unprocessed[-1]

  def _GetCurrentVersionInfo(self, sync=True):
    """Returns the current version info from the version file.
    Args:
    """
    if sync: self.cros_source.Sync(repository.RepoRepository.DEFAULT_MANIFEST)
    version_file_path = self.cros_source.GetRelativePath(constants.VERSION_FILE)
    return VersionInfo(version_file=version_file_path,
                       incr_type=self.incr_type)

  def _CreateNewBuildSpec(self, version_info):
    """Generates a new buildspec for the builders to consume.

    Checks to see, if there are new changes that need to be built from the
    last time another buildspec was created. Updates the version number in
    version number file. If there are no new changes returns None.  Otherwise
    returns the version string for the new spec.

    Args:
      version_info: Info class for version information of cros.
    Returns:
      next build number: on new changes or
      None: on no new changes
    """
    if self.latest:
      latest_spec_file = '%s.xml' % os.path.join(self.all_specs_dir,
                                                 self.latest)
      if not self.cros_source.IsManifestDifferent(latest_spec_file):
        return None

    version = version_info.VersionString()
    if version in self.all:
      message = ('Automatic: %s - Updating to a new version number from %s' % (
                 self.build_name, version))
      version = version_info.IncrementVersion(message, dry_run=self.dry_run)
      logging.debug('Incremented version number to  %s', version)
      self.cros_source.Sync(repository.RepoRepository.DEFAULT_MANIFEST)

    spec_file = '%s.xml' % os.path.join(self.all_specs_dir, version)
    if not os.path.exists(os.path.dirname(spec_file)):
      os.makedirs(os.path.dirname(spec_file))

    self.cros_source.ExportManifest(spec_file)
    logging.debug('Created New Build Spec %s', version)
    return version

  def DidLastBuildSucceed(self):
    """Returns True if this is our first build or the last build succeeded."""
    return not self.latest or self.latest in self.passed

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
    version_info = self._GetCurrentVersionInfo(sync=False)
    self._LoadSpecs(version_info, version=version)
    self.current_version = version
    return self.GetLocalManifest(self.current_version)

  def GetNextBuildSpec(self, retries=5):
    """Gets the version number of the next build spec to build.
      Args:
        retries: Number of retries for updating the status
      Returns:
        Local path to manifest to build or None in case of no need to build.
      Raises:
        GenerateBuildSpecException in case of failure to generate a buildspec
    """
    last_error = None
    for index in range(0, retries + 1):
      try:
        version_info = self._GetCurrentVersionInfo()
        logging.debug('Using version %s' % version_info.VersionString())
        self._LoadSpecs(version_info)
        self._PrepSpecChanges()
        if not self.unprocessed:
          self.current_version = self._CreateNewBuildSpec(version_info)
        else:
          self.current_version = self.latest_unprocessed

        if self.current_version:
          logging.debug('Using build spec: %s', self.current_version)
          commit_message = 'Automatic: Start %s %s' % (self.build_name,
                                                       self.current_version)
          self._SetInFlight()
          self._PushSpecChanges(commit_message)
          return self.GetLocalManifest(self.current_version)
        else:
          return None

      except (GitCommandException, cros_lib.RunCommandError) as e:
        last_error = 'Failed to generate buildspec. error: %s' % e
        logging.error(last_error)
        logging.error('Retrying to generate buildspec:  Retry %d/%d' %
                      (index + 1, retries))
    else:
      raise GenerateBuildSpecException(last_error)

  def _SetInFlight(self):
    """Marks the buildspec as inflight by creating a symlink in inflight dir."""
    dest_file = '%s.xml' % os.path.join(self.inflight_dir, self.current_version)
    src_file = '%s.xml' % os.path.join(self.all_specs_dir, self.current_version)
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

  def _PrepSpecChanges(self):
    PrepForChanges(self._TMP_MANIFEST_DIR, self.dry_run)

  def _PushSpecChanges(self, commit_message):
    _PushGitChanges(self._TMP_MANIFEST_DIR, commit_message,
                    dry_run=self.dry_run)

  def UpdateStatus(self, success, retries=5):
    """Updates the status of the build for the current build spec.
    Args:
      success: True for success, False for failure
      retries: Number of retries for updating the status
    """
    last_error = None
    for index in range(0, retries + 1):
      try:
        self._PrepSpecChanges()
        status = self.STATUS_PASSED if success else self.STATUS_FAILED
        commit_message = ('Automatic checkin: status=%s build_version %s for '
                          '%s' % (status,
                                  self.current_version,
                                  self.build_name))
        if success:
          self._SetPassed()
        else:
          self._SetFailed()

        self._PushSpecChanges(commit_message)
      except (GitCommandException, cros_lib.RunCommandError) as e:
        last_error = ('Failed to update the status for %s with the '
                      'following error %s' % (self.build_name,
                                              e.message))
        logging.error(last_error)
        logging.error('Retrying to generate buildspec:  Retry %d/%d' %
                      (index + 1, retries))
      else:
        return
    else:
      raise StatusUpdateException(last_error)


def SetLogFileHandler(logfile):
  """This sets the logging handler to a file.

    Defines a Handler which writes INFO messages or higher to the sys.stderr
    Add the log message handler to the logger

    Args:
      logfile: name of the logfile to open
  """
  logfile_handler = logging.handlers.RotatingFileHandler(logfile, backupCount=5)
  logfile_handler.setLevel(logging.DEBUG)
  logfile_handler.setFormatter(logging.Formatter(logging_format))
  logging.getLogger().addHandler(logfile_handler)
