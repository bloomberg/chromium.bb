# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing the build stages."""

from __future__ import print_function

import functools
import glob
import os

from chromite.cbuildbot import chroot_lib
from chromite.cbuildbot import commands
from chromite.cbuildbot import constants
from chromite.cbuildbot import failures_lib
from chromite.cbuildbot import repository
from chromite.cbuildbot.stages import generic_stages
from chromite.cbuildbot.stages import test_stages
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import git
from chromite.lib import osutils
from chromite.lib import parallel
from chromite.lib import portage_util


class CleanUpStage(generic_stages.BuilderStage):
  """Stages that cleans up build artifacts from previous runs.

  This stage cleans up previous KVM state, temporary git commits,
  clobbers, and wipes tmp inside the chroot.
  """

  option_name = 'clean'

  def _CleanChroot(self):
    commands.CleanupChromeKeywordsFile(self._boards,
                                       self._build_root)
    chroot_dir = os.path.join(self._build_root, constants.DEFAULT_CHROOT_DIR)
    chroot_tmpdir = os.path.join(chroot_dir, 'tmp')
    if os.path.exists(chroot_tmpdir):
      osutils.RmDir(chroot_tmpdir, ignore_missing=True, sudo=True)
      cros_build_lib.SudoRunCommand(['mkdir', '--mode', '1777', chroot_tmpdir],
                                    print_cmd=False)

    # Clear out the incremental build cache between runs.
    cache_dir = 'var/cache/portage'
    d = os.path.join(chroot_dir, cache_dir)
    osutils.RmDir(d, ignore_missing=True, sudo=True)
    for board in self._boards:
      d = os.path.join(chroot_dir, 'build', board, cache_dir)
      osutils.RmDir(d, ignore_missing=True, sudo=True)

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

  def _DeleteAutotestSitePackages(self):
    """Clears any previously downloaded site-packages."""
    site_packages_dir = os.path.join(self._build_root, 'src', 'third_party',
                                     'autotest', 'files', 'site-packages')
    # Note that these shouldn't be recreated but might be around from stale
    # builders.
    osutils.RmDir(site_packages_dir, ignore_missing=True)

  @failures_lib.SetFailureType(failures_lib.InfrastructureFailure)
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
          logging.warning("ManifestCheckout at %s is unusable: %s",
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
               self._DeleteArchivedPerfResults,
               self._DeleteAutotestSitePackages]
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
        commands.RunChrootUpgradeHooks(
            self._build_root, chrome_root=self._run.options.chrome_root,
            extra_env=self._portage_extra_env)
      except failures_lib.BuildScriptFailure:
        logging.PrintBuildbotStepText('Replacing broken chroot')
        logging.PrintBuildbotStepWarnings()
      else:
        # Clear the chroot manifest version as we are in the middle of building.
        chroot_manager = chroot_lib.ChrootManager(self._build_root)
        chroot_manager.ClearChrootVersion()

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
      logging.PrintBuildbotStepText('%s->%s' % (pre_ver, post_ver))
    else:
      logging.PrintBuildbotStepText(post_ver)

    commands.SetSharedUserPassword(
        self._build_root,
        password=self._run.config.shared_user_password)


class SetupBoardStage(generic_stages.BoardSpecificBuilderStage, InitSDKStage):
  """Stage that is responsible for building host pkgs and setting up a board."""

  option_name = 'build'

  def PerformStage(self):
    # We need to run chroot updates on most builders because they uprev after
    # the InitSDK stage. For the SDK builder, we can skip updates because uprev
    # is run prior to InitSDK. This is not just an optimization: It helps
    # workaround http://crbug.com/225509
    if self._run.config.build_type != constants.CHROOT_BUILDER_TYPE:
      usepkg_toolchain = (self._run.config.usepkg_toolchain and
                          not self._latest_toolchain)
      commands.UpdateChroot(
          self._build_root, toolchain_boards=[self._current_board],
          usepkg=usepkg_toolchain)

    # Only update the board if we need to do so.
    chroot_path = os.path.join(self._build_root, constants.DEFAULT_CHROOT_DIR)
    board_path = os.path.join(chroot_path, 'build', self._current_board)
    if not os.path.isdir(board_path) or self._run.config.board_replace:
      usepkg = self._run.config.usepkg_build_packages
      commands.SetupBoard(
          self._build_root, board=self._current_board, usepkg=usepkg,
          chrome_binhost_only=self._run.config.chrome_binhost_only,
          force=self._run.config.board_replace,
          extra_env=self._portage_extra_env, chroot_upgrade=False,
          profile=self._run.options.profile or self._run.config.profile)


class BuildPackagesStage(generic_stages.BoardSpecificBuilderStage,
                         generic_stages.ArchivingStageMixin):
  """Build Chromium OS packages."""

  option_name = 'build'
  def __init__(self, builder_run, board, suffix=None, afdo_generate_min=False,
               afdo_use=False, update_metadata=False, **kwargs):
    if afdo_use:
      suffix = self.UpdateSuffix(constants.USE_AFDO_USE, suffix)
    super(BuildPackagesStage, self).__init__(builder_run, board, suffix=suffix,
                                             **kwargs)
    self._afdo_generate_min = afdo_generate_min
    self._update_metadata = update_metadata
    assert not afdo_generate_min or not afdo_use

    useflags = self._portage_extra_env.get('USE', '').split()
    if afdo_use:
      useflags.append(constants.USE_AFDO_USE)

    if useflags:
      self._portage_extra_env['USE'] = ' '.join(useflags)

  def VerifyChromeBinpkg(self, packages):
    # Sanity check: If we didn't check out Chrome (and we're running on ToT),
    # we should be building Chrome from a binary package.
    if (not self._run.options.managed_chrome and
        self._run.manifest_branch == 'master'):
      commands.VerifyBinpkg(self._build_root,
                            self._current_board,
                            constants.CHROME_CP,
                            packages,
                            extra_env=self._portage_extra_env)

  def GetListOfPackagesToBuild(self):
    """Returns a list of packages to build."""
    if self._run.config.packages:
      # If the list of packages is set in the config, use it.
      return self._run.config.packages

    # TODO: the logic below is duplicated from the build_packages
    # script. Once we switch to `cros build`, we should consolidate
    # the logic in a shared location.
    packages = ['virtual/target-os']
    # Build Dev packages by default.
    packages += ['virtual/target-os-dev']
    # Build test packages by default.
    packages += ['virtual/target-os-test']
    # Build factory packages if requested by config.
    if self._run.config.factory:
      packages += ['chromeos-base/chromeos-installshim',
                   'chromeos-base/chromeos-factory',
                   'chromeos-base/chromeos-hwid',
                   'chromeos-base/autotest-factory-install']

    if self._run.ShouldBuildAutotest():
      packages += ['chromeos-base/autotest-all']

    return packages

  def RecordPackagesUnderTest(self, packages_to_build):
    """Records all packages that may affect the board to BuilderRun."""
    deps = dict()
    # Include packages that are built in chroot because they can
    # affect any board.
    packages = ['virtual/target-sdk']
    # Include chromite because we are running cbuildbot.
    packages += ['chromeos-base/chromite']
    try:
      deps.update(commands.ExtractDependencies(self._build_root, packages))

      # Include packages that will be built as part of the board.
      deps.update(commands.ExtractDependencies(self._build_root,
                                               packages_to_build,
                                               board=self._current_board))
    except Exception as e:
      # Dependency extraction may fail due to bad ebuild changes. Let
      # the build continues because we have logic to triage build
      # packages failures separately. Note that we only categorize CLs
      # on the package-level if dependencies are extracted
      # successfully, so it is safe to ignore the exception.
      logging.warning('Unable to gather packages under test: %s', e)
    else:
      logging.info('Recording packages under test')
      self.board_runattrs.SetParallel('packages_under_test', set(deps.keys()))

  def PerformStage(self):
    # If we have rietveld patches, always compile Chrome from source.
    noworkon = not self._run.options.rietveld_patches
    packages = self.GetListOfPackagesToBuild()
    self.VerifyChromeBinpkg(packages)
    self.RecordPackagesUnderTest(packages)

    commands.Build(self._build_root,
                   self._current_board,
                   build_autotest=self._run.ShouldBuildAutotest(),
                   usepkg=self._run.config.usepkg_build_packages,
                   chrome_binhost_only=self._run.config.chrome_binhost_only,
                   packages=packages,
                   skip_chroot_upgrade=True,
                   chrome_root=self._run.options.chrome_root,
                   noworkon=noworkon,
                   extra_env=self._portage_extra_env)

    if self._update_metadata:
      # TODO: Consider moving this into its own stage if there are other similar
      # things to do after build_packages.

      # Extract firmware version information from the newly created updater.
      main, ec = commands.GetFirmwareVersions(self._build_root,
                                              self._current_board)
      update_dict = {'main-firmware-version': main, 'ec-firmware-version': ec}
      self._run.attrs.metadata.UpdateBoardDictWithDict(
          self._current_board, update_dict)

      # Write board metadata update to cidb
      build_id, db = self._run.GetCIDBHandle()
      if db:
        db.UpdateBoardPerBuildMetadata(build_id, self._current_board,
                                       update_dict)


class BuildImageStage(BuildPackagesStage):
  """Build standard Chromium OS images."""

  option_name = 'build'
  config_name = 'images'

  def _BuildImages(self):
    # We only build base, dev, and test images from this stage.
    if self._afdo_generate_min:
      images_can_build = set(['test'])
    else:
      images_can_build = set(['base', 'dev', 'test'])
    images_to_build = set(self._run.config.images).intersection(
        images_can_build)

    version = self._run.attrs.release_tag
    disk_layout = self._run.config.disk_layout
    if self._afdo_generate_min and version:
      version = '%s-afdo-generate' % version

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
        [self._BuildVMImage, lambda: self._GenerateAuZip(cbuildbot_image_link),
         self._BuildGceTarballs])

  def _BuildVMImage(self):
    if self._run.config.vm_tests and not self._afdo_generate_min:
      commands.BuildVMImageForTesting(
          self._build_root,
          self._current_board,
          extra_env=self._portage_extra_env)

  def _GenerateAuZip(self, image_dir):
    """Create au-generator.zip."""
    if not self._afdo_generate_min:
      commands.GenerateAuZip(self._build_root,
                             image_dir,
                             extra_env=self._portage_extra_env)

  def _BuildGceTarballs(self):
    """Creates .tar.gz files that can be converted to GCE images.

    These files will be used by VMTestStage for tests on GCE. They will also be
    be uploaded to GCS buckets, where they can be used as input to the "gcloud
    compute images create" command. This will convert them into images that can
    be used to create GCE VM instances.
    """
    if self._run.config.upload_gce_images:
      image_bins = []
      if 'base' in self._run.config['images']:
        image_bins.append(constants.IMAGE_TYPE_TO_NAME['base'])
      if 'test' in self._run.config['images']:
        image_bins.append(constants.IMAGE_TYPE_TO_NAME['test'])

      image_dir = self.GetImageDirSymlink('latest')
      for image_bin in image_bins:
        if os.path.exists(os.path.join(image_dir, image_bin)):
          commands.BuildGceTarball(image_dir, image_dir, image_bin)
        else:
          logging.warning('Missing image file skipped: %s', image_bin)

  def _HandleStageException(self, exc_info):
    """Tell other stages to not wait on us if we die for some reason."""
    self.board_runattrs.SetParallelDefault('images_generated', False)
    return super(BuildImageStage, self)._HandleStageException(exc_info)

  def PerformStage(self):
    self._BuildImages()


class UprevStage(generic_stages.BuilderStage):
  """Uprevs Chromium OS packages that the builder intends to validate."""

  config_name = 'uprev'
  option_name = 'uprev'

  def __init__(self, builder_run, boards=None, **kwargs):
    super(UprevStage, self).__init__(builder_run, **kwargs)
    if boards is not None:
      self._boards = boards

  def PerformStage(self):
    # Perform other uprevs.
    overlays, _ = self._ExtractOverlays()
    commands.UprevPackages(self._build_root,
                           self._boards,
                           overlays)


class RegenPortageCacheStage(generic_stages.BuilderStage):
  """Regenerates the Portage ebuild cache."""

  # We only need to run this if we're pushing at least one overlay.
  config_name = 'push_overlays'

  def PerformStage(self):
    _, push_overlays = self._ExtractOverlays()
    inputs = [[overlay] for overlay in push_overlays if os.path.isdir(overlay)]
    parallel.RunTasksInProcessPool(portage_util.RegenCache, inputs)
