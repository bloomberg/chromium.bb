# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing the build stages."""

from __future__ import print_function

import glob
import os

from chromite.cbuildbot import cbuildbot_run
from chromite.cbuildbot import chroot_lib
from chromite.cbuildbot import commands
from chromite.cbuildbot import goma_util
from chromite.cbuildbot import repository
from chromite.cbuildbot import topology
from chromite.cbuildbot.stages import generic_stages
from chromite.cbuildbot.stages import test_stages
from chromite.lib import buildbucket_lib
from chromite.lib import config_lib
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import failures_lib
from chromite.lib import git
from chromite.lib import metrics
from chromite.lib import osutils
from chromite.lib import parallel
from chromite.lib import portage_util
from chromite.lib import path_util


class CleanUpStage(generic_stages.BuilderStage):
  """Stages that cleans up build artifacts from previous runs.

  This stage cleans up previous KVM state, temporary git commits,
  clobbers, and wipes tmp inside the chroot.
  """

  option_name = 'clean'

  def _CleanChroot(self):
    logging.info('Cleaning chroot.')
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
    logging.info('Deleting chroot.')
    chroot = os.path.join(self._build_root, constants.DEFAULT_CHROOT_DIR)
    if os.path.exists(chroot):
      # At this stage, it's not safe to run the cros_sdk inside the buildroot
      # itself because we haven't sync'd yet, and the version of the chromite
      # in there might be broken. Since we've already unmounted everything in
      # there, we can just remove it using rm -rf.
      osutils.RmDir(chroot, ignore_missing=True, sudo=True)

  def _DeleteArchivedTrybotImages(self):
    """Clear all previous archive images to save space."""
    logging.info('Deleting archived trybot images.')
    for trybot in (False, True):
      archive_root = self._run.GetArchive().GetLocalArchiveRoot(trybot=trybot)
      osutils.RmDir(archive_root, ignore_missing=True)

  def _DeleteArchivedPerfResults(self):
    """Clear any previously stashed perf results from hw testing."""
    logging.info('Deleting archived perf results.')
    for result in glob.glob(os.path.join(
        self._run.options.log_dir,
        '*.%s' % test_stages.HWTestStage.PERF_RESULTS_EXTENSION)):
      os.remove(result)

  def _DeleteChromeBuildOutput(self):
    logging.info('Deleting Chrome build output.')
    chrome_src = os.path.join(self._run.options.chrome_root, 'src')
    for out_dir in glob.glob(os.path.join(chrome_src, 'out_*')):
      osutils.RmDir(out_dir)

  def _BuildRootGitCleanup(self):
    logging.info('Cleaning up buildroot git repositories.')
    # Run git gc --auto --prune=all on all repos in CleanUpStage
    repo = self.GetRepoRepository()
    repo.BuildRootGitCleanup(prune_all=True)

  def _DeleteAutotestSitePackages(self):
    """Clears any previously downloaded site-packages."""
    logging.info('Deleting autotest site packages.')
    site_packages_dir = os.path.join(self._build_root, 'src', 'third_party',
                                     'autotest', 'files', 'site-packages')
    # Note that these shouldn't be recreated but might be around from stale
    # builders.
    osutils.RmDir(site_packages_dir, ignore_missing=True)

  def _WipeOldOutput(self):
    logging.info('Wiping old output.')
    commands.WipeOldOutput(self._build_root)

  def _GetBuildbucketBucketsForSlaves(self):
    """Get Buildbucket buckets for slaves of current build.

    Returns:
      A list of Buildbucket buckets (strings) serving the slaves.
    """
    slave_config_map = self._GetSlaveConfigMap(important_only=False)

    bucket_set = set(
        buildbucket_lib.WATERFALL_BUCKET_MAP[slave_config.active_waterfall]
        for slave_config in slave_config_map.values()
        if slave_config.active_waterfall)

    return list(bucket_set)

  def CancelObsoleteSlaveBuilds(self):
    """Cancel the obsolete slave builds scheduled by the previous master."""
    logging.info('Cancelling obsolete slave builds.')

    buildbucket_client = self.GetBuildbucketClient()

    if buildbucket_client is not None:

      slave_buildbucket_buckets = self._GetBuildbucketBucketsForSlaves()
      if not slave_buildbucket_buckets:
        logging.info('No Buildbucket buckets to search for slave builds.')
        return

      buildbucket_ids = []
      # Search for scheduled/started slave builds in chromiumos waterfall
      # and chromeos waterfall.
      for status in [constants.BUILDBUCKET_BUILDER_STATUS_SCHEDULED,
                     constants.BUILDBUCKET_BUILDER_STATUS_STARTED]:
        builds = buildbucket_client.SearchAllBuilds(
            self._run.options.debug,
            buckets=slave_buildbucket_buckets,
            tags=['build_type:%s' % self._run.config.build_type,
                  'cbb_branch:%s' % self._run.manifest_branch,
                  'master:False',
                  'master_config:%s' % self._run.config.name],
            status=status)

        ids = buildbucket_lib.ExtractBuildIds(builds)
        if ids:
          logging.info('Found builds %s in status %s.', ids, status)
          buildbucket_ids.extend(ids)

      if buildbucket_ids:
        logging.info('Going to cancel buildbucket_ids: %s', buildbucket_ids)

        if not self._run.options.debug:
          fields = {'build_type': self._run.config.build_type,
                    'build_name': self._run.config.name}
          metrics.Counter(constants.MON_BB_CANCEL_BATCH_BUILDS_COUNT).increment(
              fields=fields)

        cancel_content = buildbucket_client.CancelBatchBuildsRequest(
            buildbucket_ids,
            dryrun=self._run.options.debug)

        result_map = buildbucket_lib.GetResultMap(cancel_content)
        for buildbucket_id, result in result_map.iteritems():
          # Check if the result contains error messages.
          if buildbucket_lib.GetNestedAttr(result, ['error']):
            # TODO(nxia): Get build url and log url in the warnings.
            logging.warning("Error cancelling build %s with reason: %s. "
                            "Please check the status of the build.",
                            buildbucket_id,
                            buildbucket_lib.GetErrorReason(result))

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
      tasks = [self._BuildRootGitCleanup,
               self._WipeOldOutput,
               self._DeleteArchivedTrybotImages,
               self._DeleteArchivedPerfResults,
               self._DeleteAutotestSitePackages]
      if self._run.options.chrome_root:
        tasks.append(self._DeleteChromeBuildOutput)
      if self._run.config.chroot_replace and self._run.options.build:
        tasks.append(self._DeleteChroot)
      else:
        tasks.append(self._CleanChroot)

      # Only enable CancelObsoleteSlaveBuilds on the master builds
      # which use the Buildbucket scheduler, it checks for builds in
      # ChromiumOs and ChromeOs waterfalls.
      if (config_lib.UseBuildbucketScheduler(self._run.config) and
          config_lib.IsMasterBuild(self._run.config)):

        # TODO(dgarrett): Remove when crbug.com/719789 is fixed.
        if self._run.config.build_type == constants.ANDROID_PFQ_TYPE:
          logging.info('Don\'t try to cancel Android PFQs. crbug.com/719789')
        else:
          tasks.append(self.CancelObsoleteSlaveBuilds)

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
          usepkg=usepkg_toolchain, extra_env=self._portage_extra_env)

    # Always update the board.
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
      packages += ['virtual/target-os-factory',
                   'virtual/target-os-factory-shim']

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

  def _ShouldEnableGoma(self):
    # Enable goma if 1) chrome actually needs to be built and 2) goma is
    # available.
    return self._run.options.managed_chrome and self._run.options.goma_dir

  def _SetupGomaIfNecessary(self):
    """Sets up goma envs if necessary.

    Updates related env vars, and returns args to chroot.

    Returns:
      args which should be provided to chroot in order to enable goma.
      If goma is unusable or disabled, None is returned.
    """
    if not self._ShouldEnableGoma():
      return None

    goma = goma_util.Goma(
        self._run.options.goma_dir, self._run.options.goma_client_json)

    # Set USE_GOMA env var so that chrome is built with goma.
    self._portage_extra_env['USE_GOMA'] = 'true'
    self._portage_extra_env.update(goma.GetChrootExtraEnv())

    # Keep GOMA_TMP_DIR for Report stage to upload logs.
    self._run.attrs.metadata.UpdateWithDict(
        {'goma_tmp_dir': goma.goma_tmp_dir})

    # Mount goma directory and service account json file (if necessary)
    # into chroot.
    chroot_args = ['--goma_dir', goma.goma_dir]
    if goma.goma_client_json:
      chroot_args.extend(['--goma_client_json', goma.goma_client_json])
    return chroot_args

  def PerformStage(self):
    # If we have rietveld patches, always compile Chrome from source.
    noworkon = not self._run.options.rietveld_patches
    packages = self.GetListOfPackagesToBuild()
    self.VerifyChromeBinpkg(packages)
    self.RecordPackagesUnderTest(packages)

    try:
      event_filename = 'build-events.json'
      event_file = os.path.join(self.archive_path, event_filename)
      logging.info('Logging events to %s', event_file)
      event_file_in_chroot = path_util.ToChrootPath(event_file)
    except cbuildbot_run.VersionNotSetError:
      #TODO(chingcodes): Add better detection of archive options
      logging.info('Unable to archive, disabling build events file')
      event_filename = None
      event_file = None
      event_file_in_chroot = None

    # Set up goma. Use goma iff chrome needs to be built.
    chroot_args = self._SetupGomaIfNecessary()
    commands.Build(self._build_root,
                   self._current_board,
                   build_autotest=self._run.ShouldBuildAutotest(),
                   usepkg=self._run.config.usepkg_build_packages,
                   chrome_binhost_only=self._run.config.chrome_binhost_only,
                   packages=packages,
                   skip_chroot_upgrade=True,
                   chrome_root=self._run.options.chrome_root,
                   noworkon=noworkon,
                   noretry=self._run.config.nobuildretry,
                   chroot_args=chroot_args,
                   extra_env=self._portage_extra_env,
                   event_file=event_file_in_chroot,
                   run_goma=bool(chroot_args))

    if event_file and os.path.isfile(event_file):
      logging.info('Archive build-events.json file')
      #TODO: @chingcodes Remove upload after events DB is final
      self.UploadArtifact(event_filename, archive=False, strict=True)

      creds_file = topology.topology.get(topology.DATASTORE_WRITER_CREDS_KEY)

      build_id, db = self._run.GetCIDBHandle()
      if db and creds_file:
        parent_key = ('Build',
                      build_id,
                      'BuildStage',
                      self._build_stage_id)

        commands.ExportToGCloud(self._build_root,
                                creds_file,
                                event_file,
                                parent_key=parent_key,
                                caller=type(self).__name__)
    else:
      logging.info('No build-events.json file to archive')

    if self._update_metadata:
      # TODO: Consider moving this into its own stage if there are other similar
      # things to do after build_packages.
      # sjg@chromium.org: Considered, but gosh there are a lot of stages
      # already. What is the benefit?

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

      # Get a list of models supported by this board.
      models = commands.GetModels(self._build_root, self._current_board)
      self._run.attrs.metadata.UpdateWithDict({'unibuild': bool(models)})
      # TODO(sjg@chromium.org): Adjust the code above to write the firmware
      # version for each model, rather than for the build as a whole. This
      # will require an updated chromeos-firmwareupdate tool as well as an
      # updated firmware package.


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
    builder_path = '/'.join([self._bot_id, self.version])
    commands.BuildImage(self._build_root,
                        self._current_board,
                        sorted(images_to_build),
                        rootfs_verification=rootfs_verification,
                        version=version,
                        builder_path=builder_path,
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
          extra_env=self._portage_extra_env,
          disk_layout=self._run.config.disk_layout)

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
    if self._run.config.gce_tests:
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

  def _UpdateBuildImageMetadata(self):
    """Update the new metadata available to the build image stage."""
    update = {}
    fingerprints = self._FindFingerprints()
    if fingerprints:
      update['fingerprints'] = fingerprints
    kernel_version = self._FindKernelVersion()
    if kernel_version:
      update['kernel-version'] = kernel_version
    self._run.attrs.metadata.UpdateBoardDictWithDict(self._current_board,
                                                     update)

  def _FindFingerprints(self):
    """Returns a list of build fingerprints for this build."""
    fp_file = 'cheets-fingerprint.txt'
    fp_path = os.path.join(self.GetImageDirSymlink('latest'), fp_file)
    if not os.path.isfile(fp_path):
      return None
    fingerprints = osutils.ReadFile(fp_path).splitlines()
    logging.info('Found build fingerprint(s): %s', fingerprints)
    return fingerprints

  def _FindKernelVersion(self):
    """Returns a string containing the kernel version for this build."""
    try:
      packages = portage_util.GetPackageDependencies(self._current_board,
                                                     'virtual/linux-sources')
    except cros_build_lib.RunCommandError:
      logging.warning('Unable to get package list for metadata.')
      return None
    for package in packages:
      if package.startswith('sys-kernel/chromeos-kernel-'):
        kernel_version = portage_util.SplitCPV(package).version
        logging.info('Found active kernel version: %s', kernel_version)
        return kernel_version
    return None

  def _HandleStageException(self, exc_info):
    """Tell other stages to not wait on us if we die for some reason."""
    self.board_runattrs.SetParallelDefault('images_generated', False)
    return super(BuildImageStage, self)._HandleStageException(exc_info)

  def PerformStage(self):
    self._BuildImages()
    self._UpdateBuildImageMetadata()


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
