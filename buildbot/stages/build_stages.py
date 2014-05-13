# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing the build stages."""

import functools
import glob
import os

from chromite.buildbot import cbuildbot_commands as commands
from chromite.buildbot import cbuildbot_failures as failures_lib
from chromite.buildbot import constants
from chromite.buildbot import portage_utilities
from chromite.buildbot import repository
from chromite.buildbot.stages import generic_stages
from chromite.buildbot.stages import test_stages
from chromite.lib import cros_build_lib
from chromite.lib import git
from chromite.lib import osutils
from chromite.lib import parallel


class CleanUpStage(generic_stages.BuilderStage):
  """Stages that cleans up build artifacts from previous runs.

  This stage cleans up previous KVM state, temporary git commits,
  clobbers, and wipes tmp inside the chroot.
  """

  option_name = 'clean'

  def _CleanChroot(self):
    commands.CleanupChromeKeywordsFile(self._boards,
                                       self._build_root)
    chroot_tmpdir = os.path.join(self._build_root, constants.DEFAULT_CHROOT_DIR,
                                 'tmp')
    if os.path.exists(chroot_tmpdir):
      cros_build_lib.SudoRunCommand(['rm', '-rf', chroot_tmpdir],
                                    print_cmd=False)
      cros_build_lib.SudoRunCommand(['mkdir', '--mode', '1777', chroot_tmpdir],
                                    print_cmd=False)

  def _DeleteChroot(self):
    chroot = os.path.join(self._build_root, constants.DEFAULT_CHROOT_DIR)
    if os.path.exists(chroot):
      # At this stage, it's not safe to run the cros_sdk inside the buildroot
      # itself because we haven't sync'd yet, and the version of the chromite
      # in there might be broken. Since we've already unmounted everything in
      # there, we can just remove it using rm -rf.
      osutils.RmDir(chroot, ignore_missing=True, sudo=True)

  def _DeleteArchivedTrybotImages(self):
    """Clear all previous archive images to save space."""
    for trybot in (False, True):
      archive_root = self._run.GetArchive().GetLocalArchiveRoot(trybot=trybot)
      osutils.RmDir(archive_root, ignore_missing=True)

  def _DeleteArchivedPerfResults(self):
    """Clear any previously stashed perf results from hw testing."""
    for result in glob.glob(os.path.join(
        self._run.options.log_dir,
        '*.%s' % test_stages.HWTestStage.PERF_RESULTS_EXTENSION)):
      os.remove(result)

  def _DeleteChromeBuildOutput(self):
    chrome_src = os.path.join(self._run.options.chrome_root, 'src')
    for out_dir in glob.glob(os.path.join(chrome_src, 'out_*')):
      osutils.RmDir(out_dir)

  def PerformStage(self):
    if (not (self._run.options.buildbot or self._run.options.remote_trybot)
        and self._run.options.clobber):
      if not commands.ValidateClobber(self._build_root):
        cros_build_lib.Die("--clobber in local mode must be approved.")

    # If we can't get a manifest out of it, then it's not usable and must be
    # clobbered.
    manifest = None
    if not self._run.options.clobber:
      try:
        manifest = git.ManifestCheckout.Cached(self._build_root, search=False)
      except (KeyboardInterrupt, MemoryError, SystemExit):
        raise
      except Exception as e:
        # Either there is no repo there, or the manifest isn't usable.  If the
        # directory exists, log the exception for debugging reasons.  Either
        # way, the checkout needs to be wiped since it's in an unknown
        # state.
        if os.path.exists(self._build_root):
          cros_build_lib.Warning("ManifestCheckout at %s is unusable: %s",
                                 self._build_root, e)

    # Clean mount points first to be safe about deleting.
    commands.CleanUpMountPoints(self._build_root)

    if manifest is None:
      self._DeleteChroot()
      repository.ClearBuildRoot(self._build_root,
                                self._run.options.preserve_paths)
    else:
      tasks = [functools.partial(commands.BuildRootGitCleanup,
                                 self._build_root),
               functools.partial(commands.WipeOldOutput, self._build_root),
               self._DeleteArchivedTrybotImages,
               self._DeleteArchivedPerfResults]
      if self._run.options.chrome_root:
        tasks.append(self._DeleteChromeBuildOutput)
      if self._run.config.chroot_replace and self._run.options.build:
        tasks.append(self._DeleteChroot)
      else:
        tasks.append(self._CleanChroot)
      parallel.RunParallelSteps(tasks)


class InitSDKStage(generic_stages.BuilderStage):
  """Stage that is responsible for initializing the SDK."""

  option_name = 'build'

  def __init__(self, builder_run, chroot_replace=False, **kwargs):
    """InitSDK constructor.

    Args:
      builder_run: Builder run instance for this run.
      chroot_replace: If True, force the chroot to be replaced.
    """
    super(InitSDKStage, self).__init__(builder_run, **kwargs)
    self.force_chroot_replace = chroot_replace

  def PerformStage(self):
    chroot_path = os.path.join(self._build_root, constants.DEFAULT_CHROOT_DIR)
    replace = self._run.config.chroot_replace or self.force_chroot_replace
    pre_ver = post_ver = None
    if os.path.isdir(self._build_root) and not replace:
      try:
        pre_ver = cros_build_lib.GetChrootVersion(chroot=chroot_path)
        commands.RunChrootUpgradeHooks(self._build_root)
      except failures_lib.BuildScriptFailure:
        cros_build_lib.PrintBuildbotStepText('Replacing broken chroot')
        cros_build_lib.PrintBuildbotStepWarnings()
        replace = True

    if not os.path.isdir(chroot_path) or replace:
      use_sdk = (self._run.config.use_sdk and not self._run.options.nosdk)
      pre_ver = None
      commands.MakeChroot(
          buildroot=self._build_root,
          replace=replace,
          use_sdk=use_sdk,
          chrome_root=self._run.options.chrome_root,
          extra_env=self._portage_extra_env)

    post_ver = cros_build_lib.GetChrootVersion(chroot=chroot_path)
    if pre_ver is not None and pre_ver != post_ver:
      cros_build_lib.PrintBuildbotStepText('%s->%s' % (pre_ver, post_ver))
    else:
      cros_build_lib.PrintBuildbotStepText(post_ver)

    commands.SetSharedUserPassword(
        self._build_root,
        password=self._run.config.shared_user_password)


class SetupBoardStage(generic_stages.BoardSpecificBuilderStage, InitSDKStage):
  """Stage that is responsible for building host pkgs and setting up a board."""

  option_name = 'build'

  def PerformStage(self):
    # Calculate whether we should use binary packages.
    usepkg = (self._run.config.usepkg_setup_board and
              not self._latest_toolchain)

    # We need to run chroot updates on most builders because they uprev after
    # the InitSDK stage. For the SDK builder, we can skip updates because uprev
    # is run prior to InitSDK. This is not just an optimization: It helps
    # workaround http://crbug.com/225509
    chroot_upgrade = (
      self._run.config.build_type != constants.CHROOT_BUILDER_TYPE)

    # Iterate through boards to setup.
    chroot_path = os.path.join(self._build_root, constants.DEFAULT_CHROOT_DIR)

    # Only update the board if we need to do so.
    board_path = os.path.join(chroot_path, 'build', self._current_board)
    if not os.path.isdir(board_path) or chroot_upgrade:
      commands.SetupBoard(
          self._build_root, board=self._current_board, usepkg=usepkg,
          chrome_binhost_only=self._run.config.chrome_binhost_only,
          force=self._run.config.board_replace,
          extra_env=self._portage_extra_env, chroot_upgrade=chroot_upgrade,
          profile=self._run.options.profile or self._run.config.profile)


class BuildPackagesStage(generic_stages.BoardSpecificBuilderStage,
                         generic_stages.ArchivingStageMixin):
  """Build Chromium OS packages."""

  option_name = 'build'
  def __init__(self, builder_run, board, pgo_generate=False, pgo_use=False,
               **kwargs):
    super(BuildPackagesStage, self).__init__(builder_run, board, **kwargs)
    self._pgo_generate, self._pgo_use = pgo_generate, pgo_use
    assert not pgo_generate or not pgo_use

    useflags = self._run.config.useflags[:]
    if pgo_generate:
      self.name += ' [%s]' % constants.USE_PGO_GENERATE
      useflags.append(constants.USE_PGO_GENERATE)
    elif pgo_use:
      self.name += ' [%s]' % constants.USE_PGO_USE
      useflags.append(constants.USE_PGO_USE)

  def _GetArchitectures(self):
    """Get the list of architectures built by this builder."""
    return set(self._GetPortageEnvVar('ARCH', b) for b in self._boards)

  def PerformStage(self):
    # Wait for PGO data to be ready if needed.
    if self._pgo_use:
      cpv = portage_utilities.BestVisible(constants.CHROME_CP,
                                          buildroot=self._build_root)
      commands.WaitForPGOData(self._GetArchitectures(), cpv)

    # If we have rietveld patches, always compile Chrome from source.
    noworkon = not self._run.options.rietveld_patches

    commands.Build(self._build_root,
                   self._current_board,
                   build_autotest=self._run.ShouldBuildAutotest(),
                   usepkg=self._run.config.usepkg_build_packages,
                   chrome_binhost_only=self._run.config.chrome_binhost_only,
                   packages=self._run.config.packages,
                   skip_chroot_upgrade=True,
                   chrome_root=self._run.options.chrome_root,
                   noworkon=noworkon,
                   extra_env=self._portage_extra_env)


class BuildImageStage(BuildPackagesStage):
  """Build standard Chromium OS images."""

  option_name = 'build'

  def _BuildImages(self):
    # We only build base, dev, and test images from this stage.
    if self._pgo_generate:
      images_can_build = set(['test'])
    else:
      images_can_build = set(['base', 'dev', 'test'])
    images_to_build = set(self._run.config.images).intersection(
        images_can_build)

    version = self._run.attrs.release_tag
    disk_layout = self._run.config.disk_layout
    if self._pgo_generate:
      disk_layout = constants.PGO_GENERATE_DISK_LAYOUT
      if version:
        version = '%s-pgo-generate' % version

    rootfs_verification = self._run.config.rootfs_verification
    commands.BuildImage(self._build_root,
                        self._current_board,
                        sorted(images_to_build),
                        rootfs_verification=rootfs_verification,
                        version=version,
                        disk_layout=disk_layout,
                        extra_env=self._portage_extra_env)

    # Update link to latest image.
    latest_image = os.readlink(self.GetImageDirSymlink('latest'))
    cbuildbot_image_link = self.GetImageDirSymlink()
    if os.path.lexists(cbuildbot_image_link):
      os.remove(cbuildbot_image_link)

    os.symlink(latest_image, cbuildbot_image_link)

    self.board_runattrs.SetParallel('images_generated', True)

    parallel.RunParallelSteps(
        [self._BuildVMImage, lambda: self._GenerateAuZip(cbuildbot_image_link)])

  def _BuildVMImage(self):
    if self._run.config.vm_tests and not self._pgo_generate:
      commands.BuildVMImageForTesting(
          self._build_root,
          self._current_board,
          disk_layout=self._run.config.disk_vm_layout,
          extra_env=self._portage_extra_env)

  def _GenerateAuZip(self, image_dir):
    """Create au-generator.zip."""
    if not self._pgo_generate:
      commands.GenerateAuZip(self._build_root,
                             image_dir,
                             extra_env=self._portage_extra_env)

  def _HandleStageException(self, exc_info):
    """Tell other stages to not wait on us if we die for some reason."""
    self.board_runattrs.SetParallelDefault('images_generated', False)
    return super(BuildImageStage, self)._HandleStageException(exc_info)

  def PerformStage(self):
    if self._run.config.images:
      self._BuildImages()


class UprevStage(generic_stages.BuilderStage):
  """Stage that uprevs Chromium OS packages that the builder intends to
  validate.
  """

  option_name = 'uprev'

  def __init__(self, builder_run, boards=None, enter_chroot=True, **kwargs):
    super(UprevStage, self).__init__(builder_run, **kwargs)
    self._enter_chroot = enter_chroot
    if boards is not None:
      self._boards = boards

  def PerformStage(self):
    # Perform other uprevs.
    if self._run.config.uprev:
      overlays, _ = self._ExtractOverlays()
      commands.UprevPackages(self._build_root,
                             self._boards,
                             overlays,
                             enter_chroot=self._enter_chroot)
