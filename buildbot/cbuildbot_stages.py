# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing the various stages that a builder runs."""

import constants
import datetime
import math
import os
import re
import sys
import tempfile
import time
import traceback

from chromite.buildbot import cbuildbot_commands as commands
from chromite.buildbot import cbuildbot_config
from chromite.buildbot import lkgm_manager
from chromite.buildbot import manifest_version
from chromite.lib import cros_build_lib as cros_lib

_FULL_BINHOST = 'FULL_BINHOST'
_PORTAGE_BINHOST = 'PORTAGE_BINHOST'
PUBLIC_OVERLAY = '%(buildroot)s/src/third_party/chromiumos-overlay'
_CROS_ARCHIVE_URL = 'CROS_ARCHIVE_URL'
OVERLAY_LIST_CMD = '%(buildroot)s/src/platform/dev/host/cros_overlay_list'
VERSION_FILE = os.path.join('src/third_party/chromiumos-overlay',
                            'chromeos/config/chromeos_version.sh')

class BuildException(Exception):
  pass

class Results:
  """Static class that collects the results of our BuildStages as they run."""

  # List of results for all stages that's built up as we run. Members are of
  #  the form ('name', SUCCESS | SKIPPED | Exception, None | description)
  _results_log = []

  # Stages run in a previous run and restored. Stored as a list of
  # stage names.
  _previous = []

  # Stored in the results log for a stage skipped because it was previously
  # completed successfully.
  SUCCESS = "Stage was successful"
  SKIPPED = "Stage skipped because successful on previous run"

  @classmethod
  def Clear(cls):
    """Clear existing stage results."""
    cls._results_log = []
    cls._previous = []

  @classmethod
  def PreviouslyCompleted(cls, name):
    """Check to see if this stage was previously completed.

       Returns:
         A boolean showing the stage was successful in the previous run.
    """
    return name in cls._previous

  @classmethod
  def Success(cls):
    """Return true if all stages so far have passed."""
    for entry in cls._results_log:
      _, result, _, _ = entry

      if not result in (cls.SUCCESS, cls.SKIPPED):
        return False

    return True

  @classmethod
  def Record(cls, name, result, description=None, time=0):
    """Store off an additional stage result.

       Args:
         name: The name of the stage
         result:
           Result should be one of:
             Results.SUCCESS if the stage was successful.
             Results.SKIPPED if the stage was completed in a previous run.
             The exception the stage errored with.
         description:
           The textual backtrace of the exception, or None
    """
    cls._results_log.append((name, result, description, time))

  @classmethod
  def Get(cls):
    """Fetch stage results.

       Returns:
         A list with one entry per stage run with a result.
    """
    return cls._results_log

  @classmethod
  def GetPrevious(cls):
    """Fetch stage results.

       Returns:
         A list of stages names that were completed in a previous run.
    """
    return cls._previous

  @classmethod
  def SaveCompletedStages(cls, file):
    """Save out the successfully completed stages to the provided file."""
    for name, result, _, _ in cls._results_log:
      if result != cls.SUCCESS and result != cls.SKIPPED: break
      file.write(name)
      file.write('\n')

  @classmethod
  def RestoreCompletedStages(cls, file):
    """Load the successfully completed stages from the provided file."""
    # Read the file, and strip off the newlines
    cls._previous = [line.strip() for line in file.readlines()]

  @classmethod
  def Report(cls, file):
    """Generate a user friendly text display of the results data."""
    results = cls._results_log

    line = '*' * 60 + '\n'
    edge = '*' * 2

    if ManifestVersionedSyncStage.manifest_manager:
      file.write(line)
      file.write(edge +
                 ' RELEASE VERSION: ' +
                 ManifestVersionedSyncStage.manifest_manager.current_version +
                 '\n')

    file.write(line)
    file.write(edge + ' Stage Results\n')

    first_exception = None

    for name, result, description, run_time in results:
      timestr = datetime.timedelta(seconds=math.ceil(run_time))

      file.write(line)

      if result == cls.SUCCESS:
        # These was no error
        file.write('%s PASS %s (%s)\n' % (edge, name, timestr))

      elif result == cls.SKIPPED:
        # The stage was executed previously, and skipped this time
        file.write('%s %s previously completed\n' %
                   (edge, name))
      else:
        if type(result) in (cros_lib.RunCommandException,
                            cros_lib.RunCommandError):
          # If there was a RunCommand error, give just the command that
          # failed, not it's full argument list, since those are usually
          # too long.
          file.write('%s FAIL %s (%s) in %s\n' %
                     (edge, name, timestr, result.cmd[0]))
        else:
          # There was a normal error, give the type of exception
          file.write('%s FAIL %s (%s) with %s\n' %
                     (edge, name, timestr, type(result).__name__))

        if not first_exception:
          first_exception = description

    file.write(line)

    if BuilderStage.archive_url:
      file.write('%s BUILD ARTIFACTS FOR THIS BUILD CAN BE FOUND AT:\n' % edge)
      file.write('%s  %s\n' % (edge, BuilderStage.archive_url))
      file.write(line)

    if first_exception:
      file.write('\n')
      file.write('Build failed with:\n')
      file.write('\n')
      file.write(first_exception)

class BuilderStage(object):
  """Parent class for stages to be performed by a builder."""
  name_stage_re = re.compile('(\w+)Stage')

  # TODO(sosa): Remove these once we have a SEND/RECIEVE IPC mechanism
  # implemented.
  test_tarball = None
  overlays = None
  push_overlays = None
  archive_url = None

  # Class variable that stores the branch to build and test
  _tracking_branch = None

  @staticmethod
  def SetTrackingBranch(tracking_branch):
    BuilderStage._tracking_branch = tracking_branch

  def __init__(self, bot_id, options, build_config):
    self._bot_id = bot_id
    self._options = options
    self._build_config = build_config
    self._name = self.name_stage_re.match(self.__class__.__name__).group(1)
    self._prebuilt_type = None
    self._ExtractVariables()
    repo_dir = os.path.join(self._build_root, '.repo')
    if not self._options.clobber and os.path.isdir(repo_dir):
      self._ExtractOverlays()

  def _ExtractVariables(self):
    """Extracts common variables from build config and options into class."""
    self._build_root = os.path.abspath(self._options.buildroot)
    if (self._options.prebuilts and self._build_config['prebuilts'] and not
        self._options.debug):
      self._prebuilt_type = self._build_config['build_type']

  def _ExtractOverlays(self):
    """Extracts list of overlays into class."""
    if not BuilderStage.overlays or not BuilderStage.push_overlays:
      overlays = self._ResolveOverlays(self._build_config['overlays'])
      push_overlays = self._ResolveOverlays(self._build_config['push_overlays'])

      # Sanity checks.
      # We cannot push to overlays that we don't rev.
      assert set(push_overlays).issubset(set(overlays))
      # Either has to be a master or not have any push overlays.
      assert self._build_config['master'] or not push_overlays

      BuilderStage.overlays = overlays
      BuilderStage.push_overlays = push_overlays

  def _ResolveOverlays(self, overlays):
    """Return the list of overlays to use for a given buildbot.

    Args:
      overlays: A string describing which overlays you want.
                'private': Just the private overlay.
                'public': Just the public overlay.
                'both': Both the public and private overlays.
    """
    cmd = OVERLAY_LIST_CMD % {'buildroot': self._build_root}
    # Check in case we haven't checked out the source yet.
    if not os.path.exists(cmd):
      return []

    public_overlays = cros_lib.RunCommand([cmd, '--all_boards', '--noprivate'],
                                          redirect_stdout=True,
                                          print_cmd=False).output.split()
    private_overlays = cros_lib.RunCommand([cmd, '--all_boards', '--nopublic'],
                                           redirect_stdout=True,
                                           print_cmd=False).output.split()

    # TODO(davidjames): cros_overlay_list should include chromiumos-overlay in
    #                   its list of public overlays. But it doesn't yet...
    public_overlays.append(PUBLIC_OVERLAY % {'buildroot': self._build_root})

    if overlays == 'private':
      paths = private_overlays
    elif overlays == 'public':
      paths = public_overlays
    elif overlays == 'both':
      paths = public_overlays + private_overlays
    else:
      cros_lib.Info('No overlays found.')
      paths = []

    return paths

  def _PrintLoudly(self, msg):
    """Prints a msg with loudly."""

    border_line = '*' * 60
    edge = '*' * 2

    print border_line

    msg_lines = msg.split('\n')

    # If the last line is whitespace only drop it.
    if not msg_lines[-1].rstrip():
      del msg_lines[-1]

    for msg_line in msg_lines:
      print '%s %s' % (edge, msg_line)

    print border_line

  def _GetPortageEnvVar(self, envvar, board):
    """Get a portage environment variable for the configuration's board.

    envvar: The environment variable to get. E.g. 'PORTAGE_BINHOST'.

    Returns:
      The value of the environment variable, as a string. If no such variable
      can be found, return the empty string.
    """
    cwd = os.path.join(self._build_root, 'src', 'scripts')
    if board:
      portageq = 'portageq-%s' % board
    else:
      portageq = 'portageq'
    binhost = cros_lib.OldRunCommand(
        [portageq, 'envvar', envvar], cwd=cwd, redirect_stdout=True,
        enter_chroot=True, error_ok=True)
    return binhost.rstrip('\n')

  def _Begin(self):
    """Can be overridden.  Called before a stage is performed."""

    # Tell the buildbot we are starting a new step for the waterfall
    print '@@@BUILD_STEP %s@@@\n' % self._name

    self._PrintLoudly('Start Stage %s - %s\n\n%s' % (
        self._name, time.strftime('%H:%M:%S'), self.__doc__))

  def _Finish(self):
    """Can be overridden.  Called after a stage has been performed."""
    self._PrintLoudly('Finished Stage %s - %s' %
                      (self._name, time.strftime('%H:%M:%S')))

  def _PerformStage(self):
    """Subclassed stages must override this function to perform what they want
    to be done.
    """
    pass

  def Run(self):
    """Have the builder execute the stage."""

    if Results.PreviouslyCompleted(self._name):
      self._PrintLoudly('Skipping Stage %s' % self._name)
      Results.Record(self._name, Results.SKIPPED)
      return

    start_time = time.time()

    self._Begin()
    try:
      self._PerformStage()
    except Exception as e:
      # Tell the user about the exception, and record it
      description = traceback.format_exc()
      print >> sys.stderr, description
      elapsed_time = time.time() - start_time
      Results.Record(self._name, e, description, time=elapsed_time)

      # Tell the build bot this step failed for the waterfall
      print '@@@STEP_FAILURE@@@'

      raise BuildException()
    else:
      elapsed_time = time.time() - start_time
      Results.Record(self._name, Results.SUCCESS, time=elapsed_time)
    finally:
      self._Finish()


class SyncStage(BuilderStage):
  """Stage that performs syncing for the builder."""

  def _PerformStage(self):
    if self._options.clobber or not os.path.isdir(os.path.join(self._build_root,
                                                               '.repo')):
      commands.FullCheckout(self._build_root,
                            self._tracking_branch,
                            url=self._build_config['git_url'])
    else:
      board = self._build_config['board']
      commands.PreFlightRinse(self._build_root, board,
                              BuilderStage.overlays)
      commands.IncrementalCheckout(self._build_root)

    # Check that all overlays can be found.
    self._ExtractOverlays() # Our list of overlays are from pre-sync, refresh
    for path in BuilderStage.overlays:
      assert os.path.isdir(path), 'Missing overlay: %s' % path


class PatchChangesStage(BuilderStage):
  """Stage that patches a set of Gerrit changes to the buildroot source tree."""
  def __init__(self, bot_id, options, build_config, patches):
    BuilderStage.__init__(self, bot_id, options, build_config)
    self.patches = patches

  def _PerformStage(self):
    PUBLIC_URL = os.path.join(constants.GERRIT_HTTP_URL, 'gerrit/p')

    for patch in self.patches:
      url_prefix = constants.INTERNAL_SSH_URL if patch.internal else PUBLIC_URL
      url = os.path.join(url_prefix, patch.project)

      cmd = ['repo', 'forall', patch.project, '-c', 'pwd']
      project_dir = cros_lib.RunCommand(cmd, cwd=self._build_root,
                                        redirect_stdout=True).output.strip()

      cros_lib.RunCommand(['git', 'fetch', url, patch.ref], cwd=project_dir)
      cros_lib.RunCommand(['git', 'checkout', '--no-track',
                           '-b', constants.PATCH_BRANCH,
                           'FETCH_HEAD'], cwd=project_dir)
      m_branch = 'remotes/m/' + self._tracking_branch
      cros_lib.RunCommand(['git', 'rebase', m_branch], cwd=project_dir)


class ManifestVersionedSyncStage(BuilderStage):
  """Stage that generates a unique manifest file, and sync's to it."""

  manifest_manager = None

  def InitializeManifestManager(self):
    """Initializes a manager that manages manifests for associated stages."""
    increment = 'branch' if self._tracking_branch == 'master' else 'patch'

    ManifestVersionedSyncStage.manifest_manager = \
        manifest_version.BuildSpecsManager(
            source_dir=self._build_root,
            checkout_repo=self._build_config['git_url'],
            manifest_repo=self._build_config['manifest_version'],
            branch=self._tracking_branch,
            build_name=self._bot_id,
            incr_type=increment,
            clobber=self._options.clobber,
            dry_run=self._options.debug)

  def GetNextManifest(self):
    """Uses the initialized manifest manager to get the next manifest."""
    assert self.manifest_manager, \
        'Must run GetStageManager before checkout out build.'
    return self.manifest_manager.GetNextBuildSpec(VERSION_FILE, latest=True)

  def _PerformStage(self):
    if os.path.isdir(os.path.join(self._build_root, '.repo')):
      commands.PreFlightRinse(self._build_root, self._build_config['board'],
                              BuilderStage.overlays)

    self.InitializeManifestManager()
    next_manifest = self.GetNextManifest()
    if not next_manifest:
      print 'Manifest Revision: Nothing to build!'
      sys.exit(0);

    # Log this early on for the release team to grep out before we finish.
    print
    print 'RELEASETAG: %s' % (
        ManifestVersionedSyncStage.manifest_manager.current_version)
    print

    commands.ManifestCheckout(self._build_root,
                              self._tracking_branch,
                              next_manifest,
                              url=self._build_config['git_url'])

    # Check that all overlays can be found.
    self._ExtractOverlays()
    for path in BuilderStage.overlays:
      assert os.path.isdir(path), 'Missing overlay: %s' % path


class LGKMVersionedSyncStage(ManifestVersionedSyncStage):
  """Stage that generates a unique manifest file candidate, and sync's to it."""

  def InitializeManifestManager(self):
    """Override: Creates an LKGMManager rather than a ManifestManager."""
    ManifestVersionedSyncStage.manifest_manager = lkgm_manager.LKGMManager(
        source_dir=self._build_root,
        checkout_repo=self._build_config['git_url'],
        manifest_repo=self._build_config['manifest_version'],
        branch=self._tracking_branch,
        build_name=self._bot_id,
        clobber=self._options.clobber,
        dry_run=self._options.debug)

  def GetNextManifest(self):
    """Gets the next manifest using LKGM logic."""
    assert self.manifest_manager, \
        'Must run InitializeManifestManager before we can get a manifest.'
    assert isinstance(self.manifest_manager, lkgm_manager.LKGMManager), \
        'Manifest manager instantiated with wrong class.'

    if self._build_config['master']:
      return self.manifest_manager.CreateNewCandidate(VERSION_FILE)
    else:
      return self.manifest_manager.GetLatestCandidate(VERSION_FILE)


class ManifestVersionedSyncCompletionStage(BuilderStage):
  """Stage that records board specific results for a unique manifest file."""

  def __init__(self, bot_id, options, build_config, success):
    BuilderStage.__init__(self, bot_id, options, build_config)
    self.success = success

  def _PerformStage(self):
    if ManifestVersionedSyncStage.manifest_manager:
      ManifestVersionedSyncStage.manifest_manager.UpdateStatus(
         success=self.success)


class LGKMVersionedSyncCompletionStage(ManifestVersionedSyncCompletionStage):
  """Stage that records whether we passed or failed to build/test manifest."""

  @staticmethod
  def _GetImportantBuilders():
    builders = []
    for build_name, config in cbuildbot_config.config.iteritems():
      if config['important']: builders.append(build_name)

    return builders

  def _PerformStage(self):
    if not ManifestVersionedSyncStage.manifest_manager:
      return

    super(LGKMVersionedSyncCompletionStage, self)._PerformStage()
    if self._build_config['master']:
      builders = self._GetImportantBuilders()
      statuses = ManifestVersionedSyncStage.manifest_manager.GetBuildersStatus(
          builders)
      success = True
      for builder in builders:
        status = statuses[builder]
        if status != 'pass':
          cros_lib.Warning('Builder %s reported status %s' % (builder, status))
          success = False

      if not success:
        # TODO(sosa): Convert to Die / raise exception.
        cros_lib.Warning('An important build failed.')


class BuildBoardStage(BuilderStage):
  """Stage that is responsible for building host pkgs and setting up a board."""
  def _PerformStage(self):
    chroot_path = os.path.join(self._build_root, 'chroot')
    if not os.path.isdir(chroot_path) or self._build_config['chroot_replace']:
      commands.MakeChroot(
          buildroot=self._build_root,
          replace=self._build_config['chroot_replace'],
          fast=self._build_config['fast'],
          usepkg=self._build_config['usepkg_chroot'])
    else:
      commands.RunChrootUpgradeHooks(self._build_root)

    # If board is a string, convert to array.
    if isinstance(self._build_config['board'], str):
      board = [self._build_config['board']]
    else:
      assert self._build_config['build_type'] == 'chroot', \
        'Board array requires chroot type.'
      board = self._build_config['board']

    assert isinstance(board, list), 'Board was neither an array or a string.'

    # Iterate through boards to setup.
    for board_to_build in board:
      # Only build the board if the directory does not exist.
      board_path = os.path.join(chroot_path, 'build', board_to_build)
      if os.path.isdir(board_path):
        continue

      commands.SetupBoard(self._build_root,
                          board=board_to_build,
                          fast=self._build_config['fast'],
                          usepkg=self._build_config['usepkg_setup_board'])

    if self._prebuilt_type == 'chroot':
      commands.UploadPrebuilts(
          self._build_root, 'amd64-host', self._build_config['overlays'],
          [], self._prebuilt_type, None, self._options.buildnumber)



class UprevStage(BuilderStage):
  """Stage that uprevs Chromium OS packages that the builder intends to
  validate.
  """
  def _PerformStage(self):
    # Perform chrome uprev.
    chrome_atom_to_build = None
    if self._options.chrome_rev:
      chrome_atom_to_build = commands.MarkChromeAsStable(
          self._build_root, self._tracking_branch,
          self._options.chrome_rev, self._build_config['board'])

    # Perform other uprevs.
    if self._build_config['uprev']:
      commands.UprevPackages(self._build_root,
                             self._build_config['board'],
                             BuilderStage.overlays)
    elif self._options.chrome_rev and not chrome_atom_to_build:
      # TODO(sosa): Do this in a better way.
      sys.exit(0)


class BuildTargetStage(BuilderStage):
  """This stage builds Chromium OS for a target.

  Specifically, we build Chromium OS packages and perform imaging to get
  the images we want per the build spec."""
  def _PerformStage(self):
    build_autotest = (self._build_config['build_tests'] and
                      self._options.tests)
    env = {}
    if self._build_config.get('useflags'):
      env['USE'] = ' '.join(self._build_config['useflags'])

    commands.Build(self._build_root,
                   build_autotest=build_autotest,
                   fast=self._build_config['fast'],
                   usepkg=self._build_config['usepkg_build_packages'],
                   extra_env=env)

    if self._prebuilt_type == 'full':
      commands.UploadPrebuilts(
          self._build_root, self._build_config['board'],
          self._build_config['overlays'], [], self._prebuilt_type,
          None, self._options.buildnumber)

    commands.BuildImage(self._build_root, extra_env=env)

    if self._build_config['vm_tests']:
      commands.BuildVMImageForTesting(self._build_root, extra_env=env)


class TestStage(BuilderStage):
  """Stage that performs testing steps."""
  def _CreateTestRoot(self):
    """Returns a temporary directory for test results in chroot.

    Returns relative path from chroot rather than whole path.
    """
    # Create test directory within tmp in chroot.
    chroot = os.path.join(self._build_root, 'chroot')
    chroot_tmp = os.path.join(chroot, 'tmp')
    test_root = tempfile.mkdtemp(prefix='cbuildbot', dir=chroot_tmp)

    # Relative directory.
    (_, _, relative_path) = test_root.partition(chroot)
    return relative_path

  def _PerformStage(self):
    if self._build_config['unittests']:
      commands.RunUnitTests(self._build_root,
                            full=(not self._build_config['quick_unit']))

    if self._build_config['vm_tests']:
      test_results_dir = self._CreateTestRoot()
      try:
        commands.RunTestSuite(self._build_root,
                              self._build_config['board'],
                              os.path.join(test_results_dir,
                                           'test_harness'),
                              full=(not self._build_config['quick_vm']))

        if self._build_config['chrome_tests']:
          commands.RunChromeSuite(self._build_root,
                                  os.path.join(test_results_dir,
                                               'chrome_results'))
      finally:
        BuilderStage.test_tarball = commands.ArchiveTestResults(
            self._build_root, test_results_dir)


class RemoteTestStatusStage(BuilderStage):
  """Stage that performs testing steps."""
  def _PerformStage(self):
    test_status_cmd = ['./crostools/get_test_status.py',
                       '--board=%s' % self._build_config['board'],
                       '--build=%s' % self._options.buildnumber]
    for job in self._options.remote_test_status.split(','):
      result = cros_lib.RunCommand(
          test_status_cmd + ['--category=%s' % job],
          redirect_stdout=True, print_cmd=False)
      # Emit annotations for buildbot status updates.
      print result.output


class ArchiveStage(BuilderStage):
  """Archives build and test artifacts for developer consumption."""
  def _PerformStage(self):
    BuilderStage.archive_url, archive_dir = commands.LegacyArchiveBuild(
        self._build_root, self._bot_id, self._build_config,
        self._options.buildnumber, BuilderStage.test_tarball,
        self._options.debug)

    if not self._options.debug and self._build_config['upload_symbols']:
      commands.UploadSymbols(self._build_root,
                             board=self._build_config['board'],
                             official=self._build_config['chromeos_official'])

    # TODO: When we support branches fully, the friendly name of the branch
    # needs to be used with PushImages
    if not self._options.debug and  self._build_config['push_image']:
      commands.PushImages(self._build_root,
                          board=self._build_config['board'],
                          branch_name='master',
                          archive_dir=archive_dir)


class PushChangesStage(BuilderStage):
  """Pushes pfq and prebuilt url changes to git."""
  def _PerformStage(self):
    if self._prebuilt_type in ('binary', 'chrome'):
      board = self._build_config['board']
      binhosts = []
      binhosts.extend(self._GetPortageEnvVar(_PORTAGE_BINHOST, board).split())
      binhosts.extend(self._GetPortageEnvVar(_PORTAGE_BINHOST, None).split())
      commands.UploadPrebuilts(
          self._build_root, board, self._build_config['overlays'],
          binhosts, self._prebuilt_type, self._options.chrome_rev,
          self._options.buildnumber)

    commands.UprevPush(self._build_root,
                       self._build_config['board'],
                       BuilderStage.push_overlays,
                       self._options.debug)
