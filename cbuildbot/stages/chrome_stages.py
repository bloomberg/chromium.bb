# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing the Chrome stages."""

import multiprocessing
import platform
import os
import sys

from chromite.cbuildbot import cbuildbot_commands as commands
from chromite.cbuildbot import cbuildbot_failures as failures_lib
from chromite.cbuildbot import constants
from chromite.cbuildbot import manifest_version
from chromite.cbuildbot.stages import artifact_stages
from chromite.cbuildbot.stages import generic_stages
from chromite.cbuildbot.stages import sync_stages
from chromite.lib import cros_build_lib
from chromite.lib import git
from chromite.lib import osutils
from chromite.lib import parallel


class SyncChromeStage(generic_stages.BuilderStage,
                      generic_stages.ArchivingStageMixin):
  """Stage that syncs Chrome sources if needed."""

  option_name = 'managed_chrome'

  def __init__(self, builder_run, **kwargs):
    super(SyncChromeStage, self).__init__(builder_run, **kwargs)
    # PerformStage() will fill this out for us.
    # TODO(mtennant): Replace with a run param.
    self.chrome_version = None

  def HandleSkip(self):
    """Set run.attrs.chrome_version to chrome version in buildroot now."""
    self._run.attrs.chrome_version = self._run.DetermineChromeVersion()
    cros_build_lib.Debug('Existing chrome version is %s.',
                         self._run.attrs.chrome_version)
    self._WriteChromeVersionToMetadata()
    super(SyncChromeStage, self).HandleSkip()

  @failures_lib.SetFailureType(failures_lib.InfrastructureFailure)
  def PerformStage(self):
    # Perform chrome uprev.
    chrome_atom_to_build = None
    if self._chrome_rev:
      chrome_atom_to_build = commands.MarkChromeAsStable(
          self._build_root, self._run.manifest_branch,
          self._chrome_rev, self._boards,
          chrome_version=self._run.options.chrome_version)

    kwargs = {}
    if self._chrome_rev == constants.CHROME_REV_SPEC:
      kwargs['revision'] = self._run.options.chrome_version
      cros_build_lib.PrintBuildbotStepText('revision %s' % kwargs['revision'])
      self.chrome_version = self._run.options.chrome_version
    else:
      kwargs['tag'] = self._run.DetermineChromeVersion()
      cros_build_lib.PrintBuildbotStepText('tag %s' % kwargs['tag'])
      self.chrome_version = kwargs['tag']

    useflags = self._run.config.useflags
    commands.SyncChrome(self._build_root, self._run.options.chrome_root,
                        useflags, **kwargs)
    if (self._chrome_rev and not chrome_atom_to_build and
        self._run.options.buildbot and
        self._run.config.build_type == constants.CHROME_PFQ_TYPE):
      cros_build_lib.Info('Chrome already uprevved')
      sys.exit(0)

  def _WriteChromeVersionToMetadata(self):
    """Write chrome version to metadata and upload partial json file."""
    self._run.attrs.metadata.UpdateKeyDictWithDict(
        'version',
        {'chrome': self._run.attrs.chrome_version})
    self.UploadMetadata(filename=constants.PARTIAL_METADATA_JSON)

  def _Finish(self):
    """Provide chrome_version to the rest of the run."""
    # Even if the stage failed, a None value for chrome_version still
    # means something.  In other words, this stage tried to run.
    self._run.attrs.chrome_version = self.chrome_version
    self._WriteChromeVersionToMetadata()
    super(SyncChromeStage, self)._Finish()


class PatchChromeStage(generic_stages.BuilderStage):
  """Stage that applies Chrome patches if needed."""

  option_name = 'rietveld_patches'

  URL_BASE = 'https://codereview.chromium.org/%(id)s'

  def PerformStage(self):
    for spatch in ' '.join(self._run.options.rietveld_patches).split():
      patch, colon, subdir = spatch.partition(':')
      if not colon:
        subdir = 'src'
      url = self.URL_BASE % {'id': patch}
      cros_build_lib.PrintBuildbotLink(spatch, url)
      commands.PatchChrome(self._run.options.chrome_root, patch, subdir)


class ChromeSDKStage(generic_stages.BoardSpecificBuilderStage,
                     generic_stages.ArchivingStageMixin):
  """Run through the simple chrome workflow."""

  def __init__(self, *args, **kwargs):
    super(ChromeSDKStage, self).__init__(*args, **kwargs)
    self._upload_queue = multiprocessing.Queue()
    self._pkg_dir = os.path.join(
        self._build_root, constants.DEFAULT_CHROOT_DIR,
        'build', self._current_board, 'var', 'db', 'pkg')
    if self._run.config.chrome_sdk_build_chrome:
      self.chrome_src = os.path.join(self._run.options.chrome_root, 'src')
      self.out_board_dir = os.path.join(
          self.chrome_src, 'out_%s' % self._current_board)

  def _BuildAndArchiveChromeSysroot(self):
    """Generate and upload sysroot for building Chrome."""
    assert self.archive_path.startswith(self._build_root)
    extra_env = {}
    if self._run.config.useflags:
      extra_env['USE'] = ' '.join(self._run.config.useflags)
    in_chroot_path = git.ReinterpretPathForChroot(self.archive_path)
    cmd = ['cros_generate_sysroot', '--out-dir', in_chroot_path, '--board',
           self._current_board, '--package', constants.CHROME_CP]
    cros_build_lib.RunCommand(cmd, cwd=self._build_root, enter_chroot=True,
                              extra_env=extra_env)
    self._upload_queue.put([constants.CHROME_SYSROOT_TAR])

  def _ArchiveChromeEbuildEnv(self):
    """Generate and upload Chrome ebuild environment."""
    chrome_dir = artifact_stages.ArchiveStage.SingleMatchGlob(
        os.path.join(self._pkg_dir, constants.CHROME_CP) + '-*')
    env_bzip = os.path.join(chrome_dir, 'environment.bz2')
    with osutils.TempDir(prefix='chrome-sdk-stage') as tempdir:
      # Convert from bzip2 to tar format.
      bzip2 = cros_build_lib.FindCompressor(cros_build_lib.COMP_BZIP2)
      cros_build_lib.RunCommand(
          [bzip2, '-d', env_bzip, '-c'],
          log_stdout_to_file=os.path.join(tempdir, constants.CHROME_ENV_FILE))
      env_tar = os.path.join(self.archive_path, constants.CHROME_ENV_TAR)
      cros_build_lib.CreateTarball(env_tar, tempdir)
      self._upload_queue.put([os.path.basename(env_tar)])

  def _VerifyChromeDeployed(self, tempdir):
    """Check to make sure deploy_chrome ran correctly."""
    if not os.path.exists(os.path.join(tempdir, 'chrome')):
      raise AssertionError('deploy_chrome did not run successfully!')

  def _VerifySDKEnvironment(self):
    """Make sure the SDK environment is set up properly."""
    # If the environment wasn't set up, then the output directory wouldn't be
    # created after 'gclient runhooks'.
    # TODO: Make this check actually look at the environment.
    if not os.path.exists(self.out_board_dir):
      raise AssertionError('%s not created!' % self.out_board_dir)

  def _BuildChrome(self, sdk_cmd):
    """Use the generated SDK to build Chrome."""
    # Validate fetching of the SDK and setting everything up.
    sdk_cmd.Run(['true'])
    # Actually build chromium.
    sdk_cmd.Run(['gclient', 'runhooks'])
    self._VerifySDKEnvironment()
    sdk_cmd.Ninja()

  def _TestDeploy(self, sdk_cmd):
    """Test SDK deployment."""
    with osutils.TempDir(prefix='chrome-sdk-stage') as tempdir:
      # Use the TOT deploy_chrome.
      script_path = os.path.join(
          self._build_root, constants.CHROMITE_BIN_SUBDIR, 'deploy_chrome')
      sdk_cmd.Run([script_path, '--build-dir',
                   os.path.join(self.out_board_dir, 'Release'),
                   '--staging-only', '--staging-dir', tempdir])
      self._VerifyChromeDeployed(tempdir)

  def _GenerateAndUploadMetadata(self):
    self.UploadMetadata(upload_queue=self._upload_queue,
                        filename=constants.PARTIAL_METADATA_JSON)

  def PerformStage(self):
    if platform.dist()[-1] == 'lucid':
      # Chrome no longer builds on Lucid. See crbug.com/276311
      print 'Ubuntu lucid is no longer supported.'
      print 'Please upgrade to Ubuntu Precise.'
      cros_build_lib.PrintBuildbotStepWarnings()
      return

    steps = [self._BuildAndArchiveChromeSysroot, self._ArchiveChromeEbuildEnv,
             self._GenerateAndUploadMetadata]
    with self.ArtifactUploader(self._upload_queue, archive=False):
      parallel.RunParallelSteps(steps)

      if self._run.config.chrome_sdk_build_chrome:
        with osutils.TempDir(prefix='chrome-sdk-cache') as tempdir:
          cache_dir = os.path.join(tempdir, 'cache')
          extra_args = ['--cwd', self.chrome_src, '--sdk-path',
                        self.archive_path]
          sdk_cmd = commands.ChromeSDK(
              self._build_root, self._current_board, chrome_src=self.chrome_src,
              goma=self._run.config.chrome_sdk_goma,
              extra_args=extra_args, cache_dir=cache_dir)
          self._BuildChrome(sdk_cmd)
          self._TestDeploy(sdk_cmd)


class ChromeLKGMSyncStage(sync_stages.SyncStage):
  """Stage that syncs to the last known good manifest for Chrome."""

  output_manifest_sha1 = False

  def GetNextManifest(self):
    """Override: Gets the LKGM from the Chrome tree."""
    chrome_lkgm = commands.GetChromeLKGM(self._run.options.chrome_version)

    # We need a full buildspecs manager here as we need an initialized manifest
    # manager with paths to the spec.
    # TODO(mtennant): Consider registering as manifest_manager run param, for
    # consistency, but be careful that consumers do not get confused.
    # Currently only the "manifest_manager" from ManifestVersionedSync (and
    # subclasses) is used later in the flow.
    manifest_manager = manifest_version.BuildSpecsManager(
      source_repo=self.repo,
      manifest_repo=self._GetManifestVersionsRepoUrl(read_only=False),
      build_names=self._run.GetBuilderIds(),
      incr_type='build',
      force=False,
      branch=self._run.manifest_branch)

    manifest_manager.BootstrapFromVersion(chrome_lkgm)
    return manifest_manager.GetLocalManifest(chrome_lkgm)
