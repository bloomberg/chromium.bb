# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing the various stages that a builder runs."""

import os
import re
import sys

import chromite.buildbot.cbuildbot_commands as commands
import chromite.lib.cros_build_lib as cros_lib

_FULL_BINHOST = 'FULL_BINHOST'
PUBLIC_OVERLAY = '%(buildroot)s/src/third_party/chromiumos-overlay'
PRIVATE_OVERLAY = '%(buildroot)s/src/private-overlays/chromeos-overlay'
_CROS_ARCHIVE_URL = 'CROS_ARCHIVE_URL'


class BuilderStage():
  """Parent class for stages to be performed by a builder."""
  name_stage_re = re.compile('(\w+)Stage')

  # TODO(sosa): Remove these once we have a SEND/RECIEVE IPC mechanism
  # implemented.
  new_binhost = None
  old_binhost = None
  test_tarball = None
  rev_overlays = None
  push_overlays = None
  archive_url = None

  def __init__(self, bot_id, options, build_config):
    self._bot_id = bot_id
    self._options = options
    self._build_config = build_config
    self._name = self.name_stage_re.match(self.__class__.__name__).group(1)
    self._build_type = None
    self._ExtractVariables()

  def _ExtractVariables(self):
    """Extracts common variables from build config and options into class."""
    # TODO(sosa): Create more general method of passing around configuration.
    self._build_root = os.path.abspath(self._options.buildroot)
    if self._options.prebuilts and not self._options.debug:
      self._build_type = self._build_config['build_type']

    if not BuilderStage.rev_overlays or not BuilderStage.push_overlays:
      rev_overlays = self._ResolveOverlays(self._build_config['rev_overlays'])
      push_overlays = self._ResolveOverlays(self._build_config['push_overlays'])

      # Sanity checks.
      # We cannot push to overlays that we don't rev.
      assert set(push_overlays).issubset(set(rev_overlays))
      # Either has to be a master or not have any push overlays.
      assert self._build_config['master'] or not push_overlays

      BuilderStage.rev_overlays = rev_overlays
      BuilderStage.push_overlays = push_overlays

  def _ResolveOverlays(self, overlays):
    """Return the list of overlays to use for a given buildbot.

    Args:
      overlays: A string describing which overlays you want.
                'private': Just the private overlay.
                'public': Just the public overlay.
                'both': Both the public and private overlays.
    """
    public_overlay = PUBLIC_OVERLAY % {'buildroot': self._build_root}
    private_overlay = PRIVATE_OVERLAY % {'buildroot': self._build_root}
    if overlays == 'private':
      paths = [private_overlay]
    elif overlays == 'public':
      paths = [public_overlay]
    elif overlays == 'both':
      paths = [public_overlay, private_overlay]
    else:
      cros_lib.Info('No overlays found.')
      paths = []
    return paths

  def _PrintLoudly(self, msg, is_start):
    """Prints a msg with loudly."""
    if is_start:
      cros_lib.Info('!!! STAGE START !!!------------------------------------\n')

    cros_lib.Info('%s\n' % msg)

  def _GetPortageEnvVar(self, envvar):
    """Get a portage environment variable for the configuration's board.

    envvar: The environment variable to get. E.g. 'PORTAGE_BINHOST'.

    Returns:
      The value of the environment variable, as a string. If no such variable
      can be found, return the empty string.
    """
    cwd = os.path.join(self._build_root, 'src', 'scripts')
    portageq = 'portageq-%s' % self._build_config['board']
    binhost = cros_lib.OldRunCommand(
        [portageq, 'envvar', envvar], cwd=cwd, redirect_stdout=True,
        enter_chroot=True, error_ok=True)
    return binhost.rstrip('\n')

  def _Begin(self):
    """Can be overridden.  Called before a stage is performed."""
    self._PrintLoudly('Beginning Stage %s\nDescription:  %s' % (
        self._name, self.__doc__), is_start=True)

  def _Finish(self):
    """Can be overridden.  Called after a stage has been performed."""
    self._PrintLoudly('Finished Stage %s' % self._name, is_start=False)

  def _PerformStage(self):
    """Subclassed stages must override this function to perform what they want
    to be done.
    """
    pass

  def Run(self):
    """Have the builder execute the stage."""
    self._Begin()
    try:
      self._PerformStage()
    finally:
      self._Finish()


class SyncStage(BuilderStage):
  """Stage that performs syncing for the builder."""
  def _PerformStage(self):
    commands.PreFlightRinse(self._build_root, self._build_config['board'],
                            self._options.tracking_branch,
                            BuilderStage.rev_overlays)
    if self._options.clobber or not os.path.isdir(os.path.join(self._build_root,
                                                               '.repo')):
      commands.FullCheckout(self._build_root, self._options.tracking_branch,
                            url=self._options.url)
    else:
      BuilderStage.old_binhost = self._GetPortageEnvVar(_FULL_BINHOST)
      commands.IncrementalCheckout(self._build_root)

    # Check that all overlays can be found.
    for path in BuilderStage.rev_overlays:
      assert os.path.isdir(path), 'Missing overlay: %s' % path


class BuildBoardStage(BuilderStage):
  """Stage that is responsible for building host pkgs and setting up a board."""
  def _PerformStage(self):
    chroot_path = os.path.join(self._build_root, 'chroot')
    board_path = os.path.join(chroot_path, 'build', self._build_config['board'])
    if not os.path.isdir(chroot_path) or self._build_config['chroot_replace']:
      commands.MakeChroot(
          self._build_root, self._build_config['chroot_replace'])

    if not os.path.isdir(board_path):
      commands.SetupBoard(self._build_root, board=self._build_config['board'])


class UprevStage(BuilderStage):
  """Stage that uprevs Chromium OS packages that the builder intends to
  validate.
  """
  def _PerformStage(self):
    # Perform chrome uprev.
    chrome_atom_to_build = None
    if self._options.chrome_rev:
      chrome_atom_to_build = commands.MarkChromeAsStable(
          self._build_root, self._options.tracking_branch,
          self._options.chrome_rev, self._build_config['board'])

    # Perform other uprevs.
    if self._build_config['uprev']:
      commands.UprevPackages(self._build_root, self._options.tracking_branch,
                             self._build_config['board'],
                             BuilderStage.rev_overlays)
    elif self._options.chrome_rev and not chrome_atom_to_build:
      # TODO(sosa): Do this in a better way.
      sys.exit(0)


class BuildTargetStage(BuilderStage):
  """This stage builds Chromium OS for a target.

  Specifically, we build Chromium OS packages and perform imaging to get
  the images we want per the build spec."""
  def _PerformStage(self):
    BuilderStage.new_binhost = self._GetPortageEnvVar(_FULL_BINHOST)
    emptytree = (BuilderStage.old_binhost and
                 BuilderStage.old_binhost != BuilderStage.new_binhost)

    commands.Build(
        self._build_root, emptytree, usepkg=self._build_config['usepkg'],
        build_autotest=(self._build_config['vm_tests'] and self._options.tests))

    # TODO(sosa):  Do this optimization in a better way.
    if self._build_type == 'full':
      commands.UploadPrebuilts(
          self._build_root, self._build_config['board'],
          self._build_config['rev_overlays'], [], self._build_type,
          False)

    commands.BuildImage(self._build_root)

    if self._build_config['vm_tests']:
      commands.BuildVMImageForTesting(self._build_root)


class TestStage(BuilderStage):
  """Stage that performs testing steps."""
  def _PerformStage(self):
    if self._build_config['unittests']:
      commands.RunUnitTests(self._build_root)

    if self._build_config['vm_tests']:
      test_results_dir = '/tmp/run_remote_tests.%s' % self._options.buildnumber
      try:
        commands.RunSmokeSuite(self._build_root, test_results_dir)
        commands.RunAUTestSuite(self._build_root,
                                self._build_config['board'],
                                full=(not self._build_config['quick_vm']))
      finally:
        BuilderStage.test_tarball = commands.ArchiveTestResults(
            self._build_root, test_results_dir)


class ArchiveStage(BuilderStage):
  """Archives build and test artifacts for developer consumption."""
  def _PerformStage(self):
    BuilderStage.archive_url = commands.LegacyArchiveBuild(
        self._build_root, self._bot_id, self._build_config,
        self._options.buildnumber, BuilderStage.test_tarball,
        self._options.debug)


class PushChangesStage(BuilderStage):
  """Pushes pfq and prebuilt url changes to git."""
  def _PerformStage(self):
    if self._build_type in ('preflight', 'chrome'):
      commands.UploadPrebuilts(
          self._build_root, self._build_config['board'],
          self._build_config['rev_overlays'], [BuilderStage.new_binhost],
          self._build_type, self._options.chrome_rev)

    commands.UprevPush(self._build_root, self._options.tracking_branch,
                       self._build_config['board'], BuilderStage.push_overlays,
                       self._options.debug)
