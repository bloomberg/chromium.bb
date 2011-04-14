#!/usr/bin/python
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
A script to generate and store the manifests for the canary builders to use.
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


class InvalidOptionException(Exception):
  """Exception gets thrown for invalid options"""
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


class GitMirror(object):
  """ A Class to deal with the source tree/mirror setup and sync repos
    Args:
      mirror_dir: location for setting up git mirror
      repo_url: gitserver URL to fetch manifests from
      manifest_file: manifest_file to use for syncing
  """
  def __init__(self, mirror_dir, repo_url, manifest_file='full.xml'):
    self.mirror_dir = mirror_dir
    self.repo_url = repo_url
    self.manifest_file = manifest_file
    self.InitMirrorRepos()

  def InitMirrorRepos(self):
    """Initialize the git mirroring"""
    if not os.path.exists(self.mirror_dir):
      os.makedirs(self.mirror_dir)
      logging.debug('git mirror does not exist. Creating it for the first time')

    if not os.path.exists(os.path.join(self.mirror_dir, '.repo')):
      cros_lib.RunCommand(['repo', 'init', '-u', self.repo_url, '-m',
                           self.manifest_file, '--mirror'], cwd=self.mirror_dir,
                           input='\n\ny\n')

  def SyncMirror(self):
    """Sync/update the git mirror location"""
    cros_lib.RunCommand(['repo', 'sync'], cwd=self.mirror_dir)


class VersionInfo(object):
  """Class to encapsualte the chrome os version info
    You can instantiate this class in two ways.
    1)using a version file, specifically chromeos_version.sh,
    which contains the version information.
    2) just passing in the 4 version components (major, minor, sp and patch)
    Args:
      ver_maj: major version
      ver_min: minor version
      ver_sp:  sp version
      ver_patch: patch version
      version_file: version file location.
  """
  def __init__(self, ver_maj=0, ver_min=0, ver_sp=0, ver_patch=0,
               incr_type=None, version_file=None):
    self.ver_maj = ver_maj
    self.ver_min = ver_min
    self.ver_sp = ver_sp
    self.ver_patch = ver_patch
    self.incr_type = incr_type
    if version_file:
      self.version_file = version_file
      self.load()

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

  def load(self):
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

  def IncrementVersion(self):
    """Updates the version file by incrementing the patch component"""
    if not self.version_file:
      raise VersionUpdateException('Cannot call IncrementVersion without '
                                   'an associated version_file')
    if not self.incr_type:
      raise VersionUpdateException('Need to specify the part of the version to'
                                   'increment')

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


class SpecsInfo(object):
  """Class that contains information about the buildspecs
    Args:
      working_dir: location of the buildspecs location
      build_name: name of the build that we will be dealing with
      dir_prefix: prefix of the version for sub-directory location
      build_prefix: prefix of the build version for build specs matching
  """
  def __init__(self, working_dir, ver_manifests, build_name, dir_prefix,
               build_prefix):
    self.working_dir = working_dir
    self.ver_manifests = ver_manifests
    self.build_name = build_name
    self.dir_prefix = dir_prefix
    self.build_prefix = build_prefix
    self.root_dir = os.path.join(self.working_dir, self.ver_manifests)
    self.all_specs_dir = os.path.join(self.root_dir, 'buildspecs',
                                      self.dir_prefix)
    self.pass_dir = os.path.join(self.root_dir, 'build-name', self.build_name,
                                 'pass', self.dir_prefix)
    self.fail_dir = os.path.join(self.root_dir, 'build-name', self.build_name,
                                 'fail', self.dir_prefix)
    self.inflight_dir = os.path.join(self.root_dir, 'build-name',
                                     self.build_name, 'inflight',
                                     self.dir_prefix)
    logging.debug('Build Specs Dir = %s', self.all_specs_dir)


class BuildSpecs(object):
  """ Class to manage buildspecs
    Args:
      working_dir:location where the ver_manifests repo is synced to
      ver_manifests: Name of the ver_manifests repo. Default: manifest-versions
      build_name: Name of the build that we are interested in. E.g., x86-alex
      dir_prefix: prefix of the version numbers for sub directory look up
      build_prefix: prefix of the build version for build specs matching
      incr_type: part of the version to increment. 'patch or branch'
  """
  def __init__(self, working_dir, ver_manifests, build_name, dir_prefix,
               build_prefix, incr_type):
    self.specs_info = SpecsInfo(working_dir, ver_manifests, build_name,
                                dir_prefix, build_prefix)
    self.all = self.GetMatchingSpecs(self.specs_info.all_specs_dir, incr_type)
    self.latest = '0.0.0.0'
    self.incr_type = incr_type

    if self.all:
      self.latest = self.all[-1]
    self.all = set(self.GetMatchingSpecs(self.specs_info.all_specs_dir,
                                         self.incr_type))
    self.passed = self.GetMatchingSpecs(self.specs_info.pass_dir,
                                        self.incr_type)
    self.failed = self.GetMatchingSpecs(self.specs_info.fail_dir,
                                        self.incr_type)
    self.inflight = self.GetMatchingSpecs(self.specs_info.inflight_dir,
                                          self.incr_type)
    self.processed = sorted((self.passed + self.failed + self.inflight),
                            key=lambda s: map(int, s.split('.')))
    self.last_processed = '0.0.0.0'
    if self.processed:
      self.last_processed = self.processed[-1]
    logging.debug('Last processed build for %s is %s' %
                  (build_name, self.last_processed))

    difference = list(self.all.difference(set(self.processed)))

    for build in difference[:]:
      build1 = map(int, build.split("."))
      build2 = map(int, self.last_processed.split("."))

      if cmp(build1 , build2) == 1:
        logging.debug('Still need to build %s' % build)
      else:
        logging.debug('Ignoring build %s less than %s' %
                      (build, self.last_processed))
        difference.remove(build)

    self.unprocessed = sorted(difference, key=lambda s: map(int, s.split('.')))

  def GetMatchingSpecs(self, specs_info, incr_type):
    """Returns the sorted list of buildspecs that match '*.xml'
      Args:
        specs_info: SpecsInfo object for the buildspecs location information.
    """
    matched_manifests = []
    if os.path.exists(specs_info):
      all_manifests = os.listdir(specs_info)
      match_string = self.specs_info.build_prefix + '.*.xml'

      if incr_type == 'branch':
         match_string = self.specs_info.build_prefix + '.*.0.xml'

      matched_manifests = fnmatch.filter(
          all_manifests, match_string)
      matched_manifests = [os.path.splitext(m)[0] for m in matched_manifests]

    return matched_manifests

  def FindNextBuild(self, latest=False):
    """"Find the next build to build from unprocessed buildspecs
      Args:
        latest: True of False. (Returns the last known build on latest =True
      Returns:
        Returns the next build to be built or None
    """
    if not self.unprocessed:
      return None

    if latest:
      return self.unprocessed[-1]

    return self.unprocessed[0]

  def ExistsBuildSpec(self, build_number):
    """"Find out if the build_number exists in existing buildspecs
      Args:
        build_number: build number to check for existence of build spec
      Returns:
        True if the buildspec exists, False if it doesn't
    """
    return build_number in self.all


class BuildSpecsManager(object):
  """A Class to manage buildspecs and their states
    Args:
      working_dir: location of the buildspecs repo
      build_name: build_name e.g., x86-mario, x86-mario-factory
      dir_prefix: version prefix to match for the sub directories
      build_prefix: prefix of the build version for build specs matching
  """

  def __init__(self, working_dir, ver_manifests, build_name, dir_prefix,
               build_prefix):
    self.specs_info = SpecsInfo(working_dir, ver_manifests, build_name,
                                dir_prefix, build_prefix)

  def SetUpSymLinks(self, src_file, dest_file, remove_file=None):
    """Setting up symlinks for various statuses in the git repo
      Args:
        status: status type to set to, (one of inflight, pass, fail)
        src_file: source for the symlink
        dest_file: destination for the symlink
        remove_file: symlink that needs to be deleted for clearing the old state
    """
    dest_dir = os.path.dirname(dest_file)
    if os.path.lexists(dest_file):
      os.unlink(dest_file)

    if not os.path.exists(dest_dir):
      os.makedirs(dest_dir)
    rel_src_file = os.path.relpath(src_file, dest_dir)
    logging.debug('Linking %s to %s', rel_src_file, dest_file)
    os.symlink(rel_src_file, dest_file)

    if remove_file and os.path.lexists(remove_file):
      logging.debug('REMOVE: Removing  %s', remove_file)
      os.unlink(remove_file)


  def SetInFlight(self, version):
    """Marks a buildspec as inflight by creating a symlink in 'inflight' dir
      Args:
        version: version of the buildspec to mark as inflight
    """
    dest_file = '%s.xml' % os.path.join(self.specs_info.inflight_dir, version)
    src_file = '%s.xml' % os.path.join(self.specs_info.all_specs_dir, version)
    logging.debug('Setting build in flight  %s: %s', src_file, dest_file)
    self.SetUpSymLinks(src_file, dest_file)

  def SetFailed(self, version):
    """Marks a buildspec as failed by creating a symlink in 'fail' dir
      Args:
        version: version of the buildspec to mark as failed
    """
    dest_file = '%s.xml' % os.path.join(self.specs_info.fail_dir, version)
    src_file = '%s.xml' % os.path.join(self.specs_info.all_specs_dir, version)
    remove_file = '%s.xml' % os.path.join(self.specs_info.inflight_dir, version)
    logging.debug('Setting build to failed  %s: %s', src_file, dest_file)
    self.SetUpSymLinks(src_file, dest_file, remove_file)

  def SetPassed(self, version):
    """Marks a buildspec as failed by creating a symlink in 'pass' dir
      Args:
        version: version of the buildspec to mark as passed
    """
    dest_file = '%s.xml' % os.path.join(self.specs_info.pass_dir, version)
    src_file = '%s.xml' % os.path.join(self.specs_info.all_specs_dir, version)
    remove_file = '%s.xml' % os.path.join(self.specs_info.inflight_dir, version)
    logging.debug('Setting build to passed  %s: %s', src_file, dest_file)
    self.SetUpSymLinks(src_file, dest_file, remove_file)


def CloneGitRepo(working_dir, repo_url):
  """"Clone Given git repo
    Args:
      repo_url: git repo to clione
      repo_dir: location where it should be cloned to
  """
  if not os.path.exists(working_dir):
    os.makedirs(working_dir)
  cros_lib.RunCommand(['git', 'clone', repo_url], cwd=working_dir)

def PushChanges(git_repo, message, use_repo=False, dry_run=True):
  """Do the final commit into the git repo
      Args:
        git_repo: git repo to push
        message: Commit message
        use_repo: use repo tool for pushing changes. Default: False
        dry_run: If true, don't actually push changes to the server
    raises: GitCommandException
  """
  branch = 'temp_auto_checkin_branch'
  try:
    if use_repo:
      cros_lib.RunCommand(['repo', 'start', branch, '.'], cwd=git_repo)
      cros_lib.RunCommand(['repo', 'sync', '.'], cwd=git_repo)
      cros_lib.RunCommand(['git', 'config', 'push.default', 'tracking'],
                          cwd=git_repo)
    else:
      cros_lib.RunCommand(['git', 'pull', '--force'], cwd=git_repo)
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
    CleanGitChanges(git_repo)
    raise GitCommandException(err_msg)
  finally:
    if use_repo:
      cros_lib.RunCommand(['repo', 'abandon', branch], cwd=git_repo)

def CleanGitChanges(git_repo):
  """"Clean git repo chanages
    Args:
      git_repo: location of the git repo to clean
    raises: GitCommandException: when fails to clean
  """
  try:
    cros_lib.RunCommand(['git', 'clean', '-d', '-f'], cwd=git_repo)
    cros_lib.RunCommand(['git', 'reset', '--hard', 'HEAD'], cwd=git_repo)
  except cros_lib.RunCommandError, e:
    err_msg = 'Failed to clean git repo %s' % e.message
    logging.error(err_msg)
    raise GitCommandException(err_msg)


def SetLogFileHandler(logfile):
  """This sets the logging handler to a file.
    define a Handler which writes INFO messages or higher to the sys.stderr
    Add the log message handler to the logger
    Args:
      logfile: name of the logfile to open
  """
  logfile_handler = logging.handlers.RotatingFileHandler(logfile, backupCount=5)
  logfile_handler.setLevel(logging.DEBUG)
  logfile_handler.setFormatter(logging.Formatter(logging_format))
  logging.getLogger().addHandler(logfile_handler)


def CheckOutSources(cros_source, source_dir, branch,
                    manifest_file='full.xml', retries=0):
  """"Fetches the sources from the git server using repo tool
    Args:
      cros_source: GitMirror Object
      source_dir: Location to sync the sources to
      branch: branch to use for the manifest.xml for repo tool
      manifest_file: manifest file to use for the file definition
      retries: Number of times to try to fetch sources
  """
  while True:
    if not os.path.exists(source_dir):
      os.makedirs(source_dir)
    try:
      cros_source.SyncMirror()
      cros_lib.RunCommand(['repo', 'init', '-u', cros_source.repo_url,
                           '-b', branch, '-m',
                           manifest_file, '--reference',
                           cros_source.mirror_dir], cwd=source_dir,
                           input='\n\ny\n')
      cros_lib.RunCommand(['repo', 'sync'], cwd=source_dir)
      break
    except cros_lib.RunCommandError, e:
      if retries < 1:
        err_msg = 'Failed to sync sources %s' % e.message
        logging.error(err_msg)
        raise SrcCheckOutException(err_msg)
      logging.info('Failed to sync sources %s', e.message)
      logging.info('But will try  %d times', retries)
      CleanUP(source_dir)
      retries = retries - 1

def ExportManifest(source_dir, output_file):
  """Exports manifests file
      Args:
        source_dir: repo root
        output_file: output_file to out put the manifest to
  """
  cros_lib.RunCommand(['repo', 'manifest', '-r', '-o', output_file],
                      cwd=source_dir)


def SetupExistingBuildSpec(build_specs_manager, next_build, dry_run):
  build_specs_manager.SetInFlight(next_build)
  commit_message = 'Automatic: Start %s %s' % (
      build_specs_manager.specs_info.build_name, next_build)
  PushChanges(build_specs_manager.specs_info.root_dir,
              commit_message,
              dry_run=dry_run)


def GenerateNewBuildSpec(source_dir, branch, latest_build, build_specs_root,
                         build_specs_manager, cros_source, version_info,
                         increment_version,
                         dry_run):
  """Generates a new buildspec for the builders to consume
    Checks to see, if there are new changes that need to be built from the last
    time another buildspec was created. Updates the version number in version
    number file. Generates the manifest as the next buildspecs. pushes it to git
    and returns the next build spec number
    If there are no new changes returns None
  Args:
    source_dir: location of the source tree root
    branch: branch to sync the sources to
    latest_build: latest known build to match the latest sources against
    build_specs_root: location of the build specs root
    build_specs_manager: BuildSpecsManager object
    cros_source: GitMirror object
    version_info: VersionInfo Object
    increment_version: True or False for incrementing the chromeos_version.sh
    dry_run: if True don't push changes externally
  Returns:
    next build number: on new changes or
    None: on no new changes
  """
  temp_manifest_file = tempfile.mkstemp(suffix='mvp', text=True)[1]

  ExportManifest(source_dir, temp_manifest_file)

  latest_spec_file = '%s.xml' % os.path.join(
      build_specs_manager.specs_info.all_specs_dir, latest_build)

  logging.debug('Calling DiffManifests with %s, %s', latest_spec_file,
                temp_manifest_file)
  if not (latest_build == '0.0.0.0' or
          DiffManifests(temp_manifest_file, latest_spec_file)):
    return None

  next_version = version_info.VersionString()

  if increment_version:
    next_version = version_info.IncrementVersion()
    logging.debug('Incremented version number to  %s', next_version)
    message = 'Automatic: Updating the new version number %s' % next_version
    PushChanges(os.path.dirname(version_info.version_file), message,
                use_repo=True, dry_run=dry_run)
    CheckOutSources(cros_source, source_dir, branch)

  next_spec_file = '%s.xml' % os.path.join(
      build_specs_manager.specs_info.all_specs_dir, next_version)
  if not os.path.exists(os.path.dirname(next_spec_file)):
    os.makedirs(os.path.dirname(next_spec_file))
  ExportManifest(source_dir, next_spec_file)
  message = 'Automatic: Buildspec %s. Created by %s' % (
      next_version, build_specs_manager.specs_info.build_name)
  logging.debug('Created New Build Spec %s', message)

  build_specs_manager.SetInFlight(next_version)
  PushChanges(build_specs_root, message, dry_run=dry_run)
  return next_version


def DiffManifests(manifest1, manifest2):
  """Diff two manifest files. excluding the revisions to manifest-verisons.git
    Args:
      manifest1: First manifest file to compare
      manifest2: Second manifest file to compare
    Returns:
      True: If the manifests are different
      False: If the manifests are same
  """
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


def SetStatus(build_version, status, working_dir, ver_manifests, build_name,
              dry_run):
  """Set the status of a particular build by moving the build_spec files around
      Args:
        build_version: version of the build that we should set the status for
        status: status to set. One of 'pass|fail'
        working_dir: working dir where our version-manifests repos are synced
        ver_manifests: name of the version manifests repo
        build_name: Name of the build that we are settting the status for
        dry_run: if True don't push changes externally
  """
  logging.info('Setting Status for %s as %s', build_version, status)
  match = re.search(VER_PATTERN, build_version)
  logging.debug('Matched %s - %s - %s - %s', match.group(1), match.group(2),
                match.group(3), match.group(4))
  version_info = VersionInfo(ver_maj=match.group(1), ver_min=match.group(2),
                             ver_sp=match.group(3), ver_patch=match.group(4))
  logging.debug('Using Version: %s', version_info.VersionString())
  logging.debug('Using Directory Prefix: %s', version_info.DirPrefix())
  logging.debug('Using Build Prefix: %s', version_info.BuildPrefix())

  build_specs_manager = BuildSpecsManager(working_dir, ver_manifests,
                                          build_name, version_info.DirPrefix(),
                                          version_info.BuildPrefix())
  if status == 'pass':
    build_specs_manager.SetPassed(build_version)
  if status == 'fail':
    build_specs_manager.SetFailed(build_version)

  commit_message = 'Automatic checkin: status = %s build_version %s for %s' % (
      status, build_version, build_name)
  PushChanges(build_specs_manager.specs_info.root_dir, commit_message,
              dry_run=dry_run)


def RemoveDirs(dir_name):
  """Remove directories recursively, if they exist"""
  if os.path.exists(dir_name):
    shutil.rmtree(dir_name)

def CleanUP(source_dir=None, working_dir=None, git_mirror_dir=None):
  """Clean  up of all the directories that are created by this script
    Args:
      source_dir: directory where the sources are. Default: None
      working_dir: directory where manifest-versions is. Default: None
      git_mirror_dir: directory where git mirror is setup. Default: None
  """
  if source_dir:
    logging.debug('Cleaning source dir = %s', source_dir)
    RemoveDirs(source_dir)

  if working_dir:
    logging.debug('Cleaning working dir = %s', working_dir)
    RemoveDirs(working_dir)

  if git_mirror_dir:
    logging.debug('Cleaning git mirror dir = %s', git_mirror_dir)
    RemoveDirs(git_mirror_dir)

def GenerateWorkload(tmp_dir, source_repo, manifest_repo,
                     branch, version_file,
                     build_name, incr_type, latest=False,
                     dry_run=False,
                     retries=0):
  """Function to see, if there are any new builds that need to be built.
    Args:
      tmp_dir: Temporary working directory
      source_repo: git URL to repo with source code
      manifest_repo: git URL to repo with manifests/status values
      branch: Which branch to build ('master' or mainline)
      version_file: location of chromeos_version.sh
      build_name: name of the build_name
      incr_type: part of the version number to increment 'patch or branch'
      latest: Whether we need to handout the latest build. Default: False
      dry_run: if True don't push changes externally
      retries: Number of retries for updating the status
    Returns:
      next_build: a string of the next build number for the builder to consume
                  or None in case of no need to build.
    Raises:
      GenerateBuildSpecException in case of failure to generate a buildspec
  """

  ver_manifests=os.path.basename(manifest_repo)

  git_mirror_dir = os.path.join(tmp_dir, 'mirror')
  source_dir = os.path.join(tmp_dir, 'source')
  working_dir = os.path.join(tmp_dir, 'working')

  while True:
    try:
      cros_source = GitMirror(git_mirror_dir, source_repo)
      CheckOutSources(cros_source, source_dir, branch)

      # Let's be conservative and remove the version_manifests directory.
      RemoveDirs(working_dir)
      CloneGitRepo(working_dir, manifest_repo)

      version_file = os.path.join(source_dir, version_file)
      logging.debug('Using VERSION _FILE = %s', version_file)
      version_info = VersionInfo(version_file=version_file, incr_type=incr_type)
      build_specs = BuildSpecs(working_dir, ver_manifests, build_name,
                               version_info.DirPrefix(),
                               version_info.BuildPrefix(),
                               version_info.incr_type)
      current_build = version_info.VersionString()
      logging.debug('Current build in %s: %s', version_file, current_build)

      build_specs_manager = BuildSpecsManager(
        working_dir, ver_manifests, build_name, version_info.DirPrefix(),
        version_info.BuildPrefix())

      next_build = build_specs.FindNextBuild(latest)
      if next_build:
        logging.debug('Using an existing build spec: %s', next_build)
        SetupExistingBuildSpec(build_specs_manager, next_build, dry_run)
        return next_build

      # If the build spec doesn't exist for the current version in chromeos_
      # version.sh. we should just create the build_spec for that version,
      # instead of incrementing chromeos_version.sh

      increment_version = build_specs.ExistsBuildSpec(current_build)
      return GenerateNewBuildSpec(source_dir, branch, build_specs.latest,
                                  build_specs.specs_info.root_dir,
                                  build_specs_manager, cros_source,
                                  version_info, increment_version,
                                  dry_run)

    except (cros_lib.RunCommandError, GitCommandException), e:
      if retries < 1:
        err_msg = 'Failed to generate buildspec. error: %s' % e
        logging.error(err_msg)
        raise GenerateBuildSpecException(err_msg)

      logging.info('Failed to generate a buildspec for consumption: %s',
                   e.message)
      logging.info('But will try  %d times', retries)
      CleanUP(source_dir, working_dir)
      retries = retries - 1

def UpdateStatus(tmp_dir, manifest_repo, build_name,
                 build_version, success, dry_run=False, retries=0):
  """Updates the status of the build
    Args:
      tmp_dir: Temporary working directory, best shared with GenerateWorkload
      manifest_repo: git URL to repo with manifests/status values
      build_name: name of the build_name
      build_version: value returned by GenerateWorkload for this build
      success: True for success, False for failure
      dry_run: if True don't push changes externally
      retries: Number of retries for updating the status
    Raises:
      GenerateBuildSpecExtension in case of failure to generate a buildspec
  """

  working_dir = os.path.join(tmp_dir, 'working')
  ver_manifests=os.path.basename(manifest_repo)
  ver_manifests_dir = os.path.join(working_dir, ver_manifests)

  if success:
    status = 'pass'
  else:
    status = 'fail'

  while True:
    try:
      if not os.path.exists(ver_manifests_dir):
        CloneGitRepo(manifest_repo, working_dir)
      cros_lib.RunCommand(['git', 'checkout', 'master'], cwd=ver_manifests_dir)
      logging.debug('Updating the status info for %s to %s', build_name,
                    status)
      SetStatus(build_version, status, working_dir, ver_manifests,
                build_name, dry_run)
      logging.debug('Updated the status info for %s to %s', build_name,
                    status)
      break
    except (GitCommandException, cros_lib.RunCommandError), e:
      if retries <= 0:
        logging.error('Failed to update the status for %s to %s', build_name,
                      status)
        err_msg = 'with the following error: %s' % e.message
        logging.error(err_msg)
        raise StatusUpdateException(err_msg)

      logging.info('Failed to update the status for %s to %s', build_name,
                   status)
      logging.info('But will try  %d times', retries)
      RemoveDirs(ver_manifests_dir)
      retries = retries - 1
