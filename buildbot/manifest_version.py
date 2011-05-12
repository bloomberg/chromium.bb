#!/usr/bin/python
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
A library to generate and store the manifests for cros builders to use.
"""

import fnmatch
import logging
import logging.handlers
import os
import re
import shutil
import tempfile

import chromite.lib.cros_build_lib as cros_lib

logging_format = '%(asctime)s - %(filename)s - %(levelname)-8s: %(message)s'
date_format = '%Y/%m/%d %H:%M:%S'
logging.basicConfig(level=logging.INFO, format=logging_format,
                    datefmt=date_format)

# Pattern for matching build name format. E.g, 12.3.4.5,1.0.25.3
VER_PATTERN = '(\d+).(\d+).(\d+).(\d+)'

class VersionUpdateException(Exception):
  """Exception gets thrown for failing to update the version file"""
  pass


class GitCommandException(Exception):
  """Exception gets thrown for a git command that fails to execute."""
  pass


class SrcCheckOutException(Exception):
  """Exception gets thrown for failure to sync sources"""
  pass


class StatusUpdateException(Exception):
  """Exception gets thrown for failure to update the status"""
  pass


class GenerateBuildSpecException(Exception):
  """Exception gets thrown for failure to Generate a buildspec for the build"""
  pass


def _CloneGitRepo(working_dir, repo_url):
  """"Clone Given git repo
  Args:
    repo_url: git repo to clione
    repo_dir: location where it should be cloned to
  """
  if not os.path.exists(working_dir): os.makedirs(working_dir)
  cros_lib.RunCommand(['git', 'clone', repo_url, working_dir])


def _PushGitChanges(git_repo, message, use_repo=False, dry_run=True):
  """Do the final commit into the git repo
  Args:
    git_repo: git repo to push
    message: Commit message
    use_repo: use repo tool for pushing changes. Default: False
    dry_run: If true, don't actually push changes to the server
  raises: GitCommandException
  """
  def CleanGitDirectory():
    """"Clean git repo chanages.

    raises: GitCommandException: when fails to clean.
    """
    try:
      cros_lib.RunCommand(['git', 'clean', '-d', '-f'], cwd=git_repo)
      cros_lib.RunCommand(['git', 'reset', '--hard', 'HEAD'], cwd=git_repo)
    except cros_lib.RunCommandError, e:
      err_msg = 'Failed to clean git repo %s' % e.message
      logging.error(err_msg)
      raise GitCommandException(err_msg)

  branch = 'temp_auto_checkin_branch'
  try:
    if use_repo:
      cros_lib.RunCommand(['repo', 'start', branch, '.'], cwd=git_repo)
      cros_lib.RunCommand(['repo', 'sync', '.'], cwd=git_repo)
      cros_lib.RunCommand(['git', 'config', 'push.default', 'tracking'],
                          cwd=git_repo)
    else:
      cros_lib.RunCommand(['git', 'pull', '--force'], cwd=git_repo)

    cros_lib.RunCommand(['git',
                         'config',
                         'url.ssh://gerrit.chromium.org:29418.insteadof',
                         'http://git.chromium.org'], cwd=git_repo)
    cros_lib.RunCommand(['git', 'add', '-A'], cwd=git_repo)
    cros_lib.RunCommand(['git', 'commit', '-am', message], cwd=git_repo)

    push_cmd = ['git', 'push', '--verbose']
    if dry_run: push_cmd.append('--dry-run')
    cros_lib.RunCommand(push_cmd, cwd=git_repo)
  except cros_lib.RunCommandError, e:
    err_msg = 'Failed to commit to %s' % e.message
    logging.error(err_msg)
    git_status = cros_lib.RunCommand(['git', 'status'], cwd=git_repo)
    logging.error('Current repo %s status: %s', git_repo, git_status)
    CleanGitDirectory()
    raise GitCommandException(err_msg)
  finally:
    if use_repo:
      cros_lib.RunCommand(['repo', 'abandon', branch], cwd=git_repo)


def _ExportManifest(source_dir, output_file):
  """Exports manifests file
  Args:
    source_dir: repo root
    output_file: output_file to out put the manifest to
  """
  cros_lib.RunCommand(['repo', 'manifest', '-r', '-o', output_file],
                      cwd=source_dir)


def _DiffManifests(manifest1, manifest2):
  """Diff two manifest files. excluding the revisions to manifest-verisons.git
  Args:
    manifest1: First manifest file to compare
    manifest2: Second manifest file to compare
  Returns:
    True: If the manifests are different
    False: If the manifests are same
  """
  logging.debug('Calling DiffManifests with %s, %s', manifest1, manifest2)
  black_list = ['manifest-versions']
  blacklist_pattern = re.compile(r'|'.join(black_list))
  with open(manifest1, 'r') as manifest1_fh:
    with open(manifest2, 'r') as manifest2_fh:
      for (line1, line2) in zip(manifest1_fh, manifest2_fh):
        if blacklist_pattern.search(line1):
          logging.debug('%s ignored %s', line1, line2)
          continue

        if line1 != line2:
          return True
      return False


def _RemoveDirs(dir_name):
  """Remove directories recursively, if they exist"""
  if os.path.exists(dir_name):
    shutil.rmtree(dir_name)


def _CreateSymlink(src_file, dest_file, remove_file=None):
  """Creates a relative symlink from src to dest with optional removal of file.

  More robust symlink creation that creates a relative symlink from src_file to
  dest_file.  Also if remove_file is set, removes symlink there.

  This is useful for multiple calls of _CreateSymlink where you are using
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


class _RepoRepository(object):
  """ A Class to deal with the source tree/mirror setup and sync repos
  Args:
    repo_url: gitserver URL to fetch manifests from.
    directory: location for setting up the repo repository.
    local_mirror: If set, sync from git mirror rather than repo url.
  """
  def __init__(self, repo_url, directory, local_mirror=None):
    self.repo_url = repo_url
    self.directory = directory
    self.local_mirror = local_mirror

  def Initialize(self, branch):
    """(Re)Initialize the repository"""
    if not os.path.exists(self.directory):
      os.makedirs(self.directory)
      logging.debug('Repo directory does not exist. '
                    'Creating it for the first time')

    # Do not reinitialize mirrors.
    if not os.path.exists(os.path.join(self.directory, '.repo')):
      init_cmd = ['repo', 'init', '-u', self.repo_url]
      if branch: init_cmd.extend(['-b', branch])

      if self.local_mirror:
        self.local_mirror.Sync()
        init_cmd.extend(['--reference', self.local_mirror.directory])
      else:
        init_cmd.append('--mirror')

      cros_lib.RunCommand(init_cmd, cwd=self.directory, input='\n\ny\n')

  def Sync(self, branch=None):
    """Sync/update the source"""
    try:
      self.Initialize(branch)
      cros_lib.RunCommand(['repo', 'sync', '-q', '--jobs=4'],
                          cwd=self.directory)
    except cros_lib.RunCommandError, e:
      err_msg = 'Failed to sync sources %s' % e.message
      logging.error(err_msg)
      raise SrcCheckOutException(err_msg)


class VersionInfo(object):
  """Class to encapsualte the chrome os version info

  You can instantiate this class in two ways.
  1)using a version file, specifically chromeos_version.sh,
  which contains the version information.
  2) just passing in the 4 version components (major, minor, sp and patch)
  Args:
    version_string: Optional version string to parse rather than from a file
    ver_maj: major version
    ver_min: minor version
    ver_sp:  sp version
    ver_patch: patch version
    version_file: version file location.
  """
  def __init__(self, version_string=None, incr_type=None, version_file=None):
    if version_file:
      self.version_file = version_file
      logging.debug('Using VERSION _FILE = %s', version_file)
      self._LoadFromFile()
    else:
      match = re.search(VER_PATTERN, version_string)
      self.ver_maj = match.group(1)
      self.ver_min = match.group(2)
      self.ver_sp = match.group(3)
      self.ver_patch = match.group(4)
      self.version_file = None

    self.incr_type = incr_type
    logging.debug('Using version %s' % self.VersionString())

  def _LoadFromFile(self):
    """Read the version file and set the version components"""
    with open(self.version_file, 'r') as version_fh:
      for line in version_fh:
        if not line.strip():
          continue

        match = self.FindValue('CHROMEOS_VERSION_MAJOR', line)
        if match:
          self.ver_maj = match
          logging.debug('Set the major version to:%s', self.ver_maj)
          continue

        match = self.FindValue('CHROMEOS_VERSION_MINOR', line)
        if match:
          self.ver_min = match
          logging.debug('Set the minor version to:%s', self.ver_min)
          continue

        match = self.FindValue('CHROMEOS_VERSION_BRANCH', line)
        if match:
          self.ver_sp = match
          logging.debug('Set the sp version to:%s', self.ver_sp)
          continue

        match = self.FindValue('CHROMEOS_VERSION_PATCH', line)
        if match:
          self.ver_patch = match
          logging.debug('Set the patch version to:%s', self.ver_patch)
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
    if not self.version_file:
      raise VersionUpdateException('Cannot call IncrementVersion without '
                                   'an associated version_file')
    if not self.incr_type:
      raise VersionUpdateException('Need to specify the part of the version to'
                                   ' increment')

    if self.incr_type == 'branch':
      self.ver_sp = str(int(self.ver_sp) + 1)
      self.ver_patch = '0'
    if self.incr_type == 'patch':
      self.ver_patch = str(int(self.ver_patch) + 1)
    temp_file = tempfile.mkstemp(suffix='mvp', prefix='tmp', dir=None,
                                 text=True)[1]
    with open(self.version_file, 'r') as source_version_fh:
      with open(temp_file, 'w') as temp_fh:
        for line in source_version_fh:
          old_patch = self.FindValue('CHROMEOS_VERSION_PATCH', line)
          if old_patch:
            temp_fh.write(line.replace(old_patch, self.ver_patch, 1))
            continue

          old_sp = self.FindValue('CHROMEOS_VERSION_BRANCH', line)
          if old_sp:
            temp_fh.write(line.replace(old_sp, self.ver_sp, 1))
            continue

          temp_fh.write(line)
        temp_fh.close()
      source_version_fh.close()
    shutil.copyfile(temp_file, self.version_file)
    os.unlink(temp_file)
    _PushGitChanges(os.path.dirname(self.version_file), message, use_repo=True,
                    dry_run=dry_run)
    return self.VersionString()

  def VersionString(self):
    """returns the version string"""
    return '%s.%s.%s.%s' % (self.ver_maj, self.ver_min, self.ver_sp,
                            self.ver_patch)
  def DirPrefix(self):
    """returns the sub directory suffix in manifest-versions"""
    return '%s.%s' % (self.ver_maj, self.ver_min)

  def BuildPrefix(self):
    """returns the build prefix to match the buildspecs in  manifest-versions"""
    if self.incr_type == 'patch':
      return '%s.%s.%s' % (self.ver_maj, self.ver_min, self.ver_sp)

    if self.incr_type == 'branch':
      return '%s.%s' % (self.ver_maj, self.ver_min)

    return None


class BuildSpecsManager(object):
  """A Class to manage buildspecs and their states."""
  def __init__(self, tmp_dir, source_repo, manifest_repo, branch,
               build_name, incr_type, dry_run):
    """Initializes a build specs manager.
    Args:
      tmp_dir: Working directory for all manifest_version work.
      source_repo:  Source repository for cros.
      manifest_repo:  Manifest repository for manifest versions / buildspecs.
        branch: The branch.
      build_name: Name of the build that we are interested in. E.g., x86-alex
      incr_type: part of the version to increment. 'patch or branch'
      dry_run: Whether we actually commit changes we make or not.
    """
    mirror_dir = os.path.join(tmp_dir, 'mirror')
    local_mirror = _RepoRepository(source_repo, mirror_dir, None)
    self.source_dir = os.path.join(tmp_dir, 'source')
    self.cros_source = _RepoRepository(source_repo, self.source_dir,
                                       local_mirror)
    self.manifest_repo = manifest_repo
    self.manifests_dir = os.path.join(tmp_dir, 'manifests')
    self.branch = branch
    self.build_name = build_name
    self.incr_type = incr_type
    self.dry_run = dry_run

    # Directories and specifications are set once we load the specs.
    self.all_specs_dir = None
    self.pass_dir = None
    self.fail_dir = None
    self.inflight_dir = None

    # Specs.
    self.all = None
    self.unprocessed = None
    self.latest = None
    self.latest_unprocessed = None
    self.current_build_spec = None
    self.compare_versions_fn = lambda s: map(int, s.split('.'))

  def _GetMatchingSpecs(self, version_info, directory):
    """Returns the sorted list of buildspecs that match '*.xml in a directory.'
    Args:
      version_info: Info class for version information of cros.
      directory: Directory of the buildspecs.
    """
    matched_manifests = []
    if os.path.exists(directory):
      all_manifests = os.listdir(directory)
      match_string = version_info.BuildPrefix() + '.*.xml'

      if self.incr_type == 'branch':
         match_string = version_info.BuildPrefix() + '.*.0.xml'

      matched_manifests = fnmatch.filter(
          all_manifests, match_string)
      matched_manifests = [os.path.splitext(m)[0] for m in matched_manifests]

    return sorted(matched_manifests, key=self.compare_versions_fn)

  def _LoadSpecs(self, version_info, relative_working_dir=''):
    """Loads the specifications from the working directory.
    Args:
      version_info: Info class for version information of cros.
    """
    working_dir = os.path.join(self.manifests_dir, relative_working_dir)
    dir_pfx = version_info.DirPrefix()
    specs_for_build = os.path.join(working_dir, 'build-name',
                                   self.build_name)
    self.all_specs_dir = os.path.join(working_dir, 'buildspecs', dir_pfx)
    self.pass_dir = os.path.join(specs_for_build, 'pass', dir_pfx)
    self.fail_dir = os.path.join(specs_for_build, 'fail', dir_pfx)
    self.inflight_dir = os.path.join(specs_for_build, 'inflight', dir_pfx)

    # Conservatively grab the latest manifest versions repository.
    _RemoveDirs(self.manifests_dir)
    _CloneGitRepo(self.manifests_dir, self.manifest_repo)

    # Build lists of specs.
    self.all = self._GetMatchingSpecs(version_info, self.all_specs_dir)

    # Build list of unprocessed specs.
    passed = self._GetMatchingSpecs(version_info, self.pass_dir)
    failed = self._GetMatchingSpecs(version_info, self.fail_dir)
    inflight = self._GetMatchingSpecs(version_info, self.inflight_dir)
    processed = sorted(set(passed + failed + inflight),
                            key=self.compare_versions_fn)
    self.unprocessed = sorted(set(self.all).difference(set(processed)),
                              key=self.compare_versions_fn)

    if self.all: self.latest = self.all[-1]
    latest_processed = None
    if processed: latest_processed = processed[-1]
    logging.debug('Last processed build for %s is %s' % (self.build_name,
                                                         latest_processed))
    # Remove unprocessed candidates that are older than the latest processed.
    if latest_processed:
      for build in self.unprocessed:
        build1 = map(int, build.split('.'))
        build2 = map(int, latest_processed.split('.'))

        if build1 > build2:
          logging.debug('Still need to build %s' % build)
        else:
          logging.debug('Ignoring build %s less than %s' %
                        (build, latest_processed))
          self.unprocessed.remove(build)

    if self.unprocessed: self.latest_unprocessed = self.unprocessed[-1]

  def _GetCurrentVersionInfo(self, version_file):
    """Returns the current version info from the version file.
    Args:
      version_info: Info class for version information of cros.
    """
    self.cros_source.Sync(branch=self.branch)
    version_file_path = os.path.join(self.source_dir, version_file)
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
    temp_manifest_file = tempfile.mkstemp(suffix='mvp', text=True)[1]
    _ExportManifest(self.source_dir, temp_manifest_file)
    if self.latest:
      latest_spec_file = '%s.xml' % os.path.join(self.all_specs_dir,
                                                 self.latest)
      if not _DiffManifests(temp_manifest_file, latest_spec_file):
        return None

    version = version_info.VersionString()
    if version in self.all:
      message = ('Automatic: Updating the new version number %s' %
                 version)
      version = version_info.IncrementVersion(message, dry_run=self.dry_run)
      logging.debug('Incremented version number to  %s', version)
      self.cros_source.Sync(branch=self.branch)

    spec_file = '%s.xml' % os.path.join(self.all_specs_dir, version)
    if not os.path.exists(os.path.dirname(spec_file)):
      os.makedirs(os.path.dirname(spec_file))

    _ExportManifest(self.source_dir, spec_file)
    logging.debug('Created New Build Spec %s', version)
    return version

  def GetNextBuildSpec(self, version_file, latest=False, retries=0):
    """Gets the version number of the next build spec to build.
      Args:
        version_file: File to use in cros when checking for cros version.
        latest: Whether we need to handout the latest build. Default: False
        retries: Number of retries for updating the status
      Returns:
        next_build: a string of the next build number for the builder to consume
                    or None in case of no need to build.
      Raises:
        GenerateBuildSpecException in case of failure to generate a buildspec
    """
    try:
      version_info = self._GetCurrentVersionInfo(version_file)
      self._LoadSpecs(version_info)

      if not self.unprocessed:
        self.current_version = self._CreateNewBuildSpec(version_info)
      elif latest:
        self.current_version = self.latest_unprocessed
      else:
        self.current_version = self.unprocessed[0]

      if self.current_version:
        logging.debug('Using build spec: %s', self.current_version)
        commit_message = 'Automatic: Start %s %s' % (self.build_name,
                                                     self.current_version)
        self._SetInFlight(commit_message)

      return self.current_version

    except (cros_lib.RunCommandError, GitCommandException) as e:
      err_msg = 'Failed to generate buildspec. error: %s' % e
      logging.error(err_msg)
      raise GenerateBuildSpecException(err_msg)

  def _SetInFlight(self, message):
    """Marks the current buildspec as inflight by creating a symlink.
    Args:
      message: Commit message to use when pushing new status.
    """
    dest_file = '%s.xml' % os.path.join(self.inflight_dir, self.current_version)
    src_file = '%s.xml' % os.path.join(self.all_specs_dir, self.current_version)
    logging.debug('Setting build in flight  %s: %s', src_file, dest_file)
    _CreateSymlink(src_file, dest_file)
    self._PushSpecChanges(message)

  def _SetFailed(self, message):
    """Marks the current buildspec as failed by creating a symlink in 'fail' dir
    Args:
      message: Commit message to use when pushing new status.
    """
    dest_file = '%s.xml' % os.path.join(self.fail_dir, self.current_version)
    src_file = '%s.xml' % os.path.join(self.all_specs_dir, self.current_version)
    remove_file = '%s.xml' % os.path.join(self.inflight_dir,
                                          self.current_version)
    logging.debug('Setting build to failed  %s: %s', src_file, dest_file)
    _CreateSymlink(src_file, dest_file, remove_file)
    self._PushSpecChanges(message)

  def _SetPassed(self, message):
    """Marks the current buildspec as passed by creating a symlink in 'pass' dir
    Args:
      message: Commit message to use when pushing new status.
    """
    dest_file = '%s.xml' % os.path.join(self.pass_dir, self.current_version)
    src_file = '%s.xml' % os.path.join(self.all_specs_dir, self.current_version)
    remove_file = '%s.xml' % os.path.join(self.inflight_dir,
                                          self.current_version)
    logging.debug('Setting build to passed  %s: %s', src_file, dest_file)
    _CreateSymlink(src_file, dest_file, remove_file)
    self._PushSpecChanges(message)

  def _PushSpecChanges(self, commit_message):
    _PushGitChanges(self.manifests_dir, commit_message, dry_run=self.dry_run)

  def UpdateStatus(self, success, retries=0):
    """Updates the status of the build for the current build spec.
    Args:
      success: True for success, False for failure
      retries: Number of retries for updating the status
    """
    try:
      status = 'fail'
      if success: status = 'pass'
      logging.debug('Updating the status info for %s to %s', self.build_name,
                    status)
      commit_message = (
          'Automatic checkin: status = %s build_version %s for %s' % (
              status, self.current_version, self.build_name))
      if status == 'pass':
        self._SetPassed(commit_message)
      if status == 'fail':
        self._SetFailed(commit_message)
    except (GitCommandException, cros_lib.RunCommandError) as e:
      err_msg = ('Failed to update the status for %s to %s with the following '
                 'error %s' % (self.build_name, status, e.message))
      logging.error(err_msg)
      raise StatusUpdateException(err_msg)


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
