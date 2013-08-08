# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing the various stages that a builder runs."""

import contextlib
import datetime
import functools
import glob
import json
import logging
import math
import multiprocessing
import os
import platform
import Queue
import re
import shutil
import sys
import urllib
from xml.etree import ElementTree

from chromite.buildbot import builderstage as bs
from chromite.buildbot import cbuildbot_commands as commands
from chromite.buildbot import cbuildbot_config
from chromite.buildbot import cbuildbot_results as results_lib
from chromite.buildbot import constants
from chromite.buildbot import lab_status
from chromite.buildbot import lkgm_manager
from chromite.buildbot import manifest_version
from chromite.buildbot import portage_utilities
from chromite.buildbot import repository
from chromite.buildbot import trybot_patch_pool
from chromite.buildbot import validation_pool
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import git
from chromite.lib import gs
from chromite.lib import toolchain
from chromite.lib import osutils
from chromite.lib import parallel
from chromite.lib import patch as cros_patch

_FULL_BINHOST = 'FULL_BINHOST'
_PORTAGE_BINHOST = 'PORTAGE_BINHOST'
_CROS_ARCHIVE_URL = 'CROS_ARCHIVE_URL'
_PRINT_INTERVAL = 1
_VM_TEST_ERROR_MSG = """
!!!VMTests failed!!!

Logs are uploaded in the corresponding vm_test_results.tgz. This can be found
by clicking on the artifacts link in the "Report" Stage. Specifically look
for the test_harness/failed for the failing tests. For more
particulars, please refer to which test failed i.e. above see the
individual test that failed -- or if an update failed, check the
corresponding update directory.
"""


class NonHaltingBuilderStage(bs.BuilderStage):
  """Build stage that fails a build but finishes the other steps."""
  def Run(self):
    try:
      super(NonHaltingBuilderStage, self).Run()
    except results_lib.StepFailure:
      name = self.__class__.__name__
      cros_build_lib.Error('Ignoring StepFailure in %s', name)


class ForgivingBuilderStage(bs.BuilderStage):
  """Build stage that turns a build step red but not a build."""
  def _HandleStageException(self, exception):
    """Override and don't set status to FAIL but FORGIVEN instead."""
    return self._HandleExceptionAsWarning(exception)


class BoardSpecificBuilderStage(bs.BuilderStage):

  def __init__(self, options, build_config, board, suffix=None):
    super(BoardSpecificBuilderStage, self).__init__(options, build_config,
                                                    suffix)
    self._current_board = board

    if not isinstance(board, basestring):
      raise TypeError('Expected string, got %r' % (board,))

    # Add a board name suffix to differentiate between various boards (in case
    # more than one board is built on a single builder.)
    if len(self._boards) > 1 or build_config['grouped']:
      self.name = '%s [%s]' % (self.name, board)

  def GetImageDirSymlink(self, pointer='latest-cbuildbot'):
    """Get the location of the current image."""
    buildroot, board = self._options.buildroot, self._current_board
    return os.path.join(buildroot, 'src', 'build', 'images', board, pointer)


class ArchivingStage(BoardSpecificBuilderStage):
  """Helper for stages that archive files.

  Attributes:
    archive_stage: The ArchiveStage instance for this board.
    bot_archive_root: The root path where output from this builder is stored.
    download_url: The URL where we can download artifacts.
    upload_url: The Google Storage location where we should upload artifacts.
  """

  PROCESSES = 10
  _BUILDBOT_ARCHIVE = 'buildbot_archive'
  _TRYBOT_ARCHIVE = 'trybot_archive'

  @classmethod
  def GetArchiveRoot(cls, buildroot, trybot=False):
    """Return the location where archive images are kept."""
    archive_base = cls._TRYBOT_ARCHIVE if trybot else cls._BUILDBOT_ARCHIVE
    return os.path.join(buildroot, archive_base)

  def __init__(self, options, build_config, board, archive_stage, suffix=None):
    super(ArchivingStage, self).__init__(options, build_config, board,
                                         suffix=suffix)
    self.archive_stage = archive_stage

    if options.remote_trybot:
      self.debug = options.debug_forced
    else:
      self.debug = options.debug

    self.version = archive_stage.GetVersion()

    gsutil_archive = self._GetGSUtilArchiveDir()
    self.upload_url = '%s/%s' % (gsutil_archive, self.version)

    trybot = not options.buildbot or options.debug
    archive_root = ArchivingStage.GetArchiveRoot(self._build_root, trybot)
    self.bot_archive_root = os.path.join(archive_root, self._bot_id)
    self.archive_path = os.path.join(self.bot_archive_root, self.version)

    if options.buildbot or options.remote_trybot:
      base_download_url = gs.PRIVATE_BASE_HTTPS_URL
      self.download_url = self.upload_url.replace('gs://', base_download_url)
    else:
      self.download_url = self.archive_path

  @contextlib.contextmanager
  def ArtifactUploader(self, queue=None, archive=True, strict=True):
    """Upload each queued input in the background.

    This context manager starts a set of workers in the background, who each
    wait for input on the specified queue. These workers run
    self.UploadArtifact(*args, archive=archive) for each input in the queue.

    Arguments:
      queue: Queue to use. Add artifacts to this queue, and they will be
        uploaded in the background.  If None, one will be created on the fly.
      archive: Whether to automatically copy files to the archive dir.
      strict: Whether to treat upload errors as fatal.

    Returns:
      The queue to use. This is only useful if you did not supply a queue.
    """
    upload = lambda path: self.UploadArtifact(path, archive, strict)
    with parallel.BackgroundTaskRunner(upload, queue=queue,
                                       processes=self.PROCESSES) as bg_queue:
      yield bg_queue

  def PrintDownloadLink(self, filename, prefix=''):
    """Print a link to an artifact in Google Storage.

    Args:
      filename: The filename of the uploaded file.
      prefix: The prefix to put in front of the filename.
    """
    url = '%s/%s' % (self.download_url.rstrip('/'), filename)
    cros_build_lib.PrintBuildbotLink(prefix + filename, url)

  def UploadArtifact(self, path, archive=True, strict=True):
    """Upload generated artifact to Google Storage.

    Arguments:
      path: Path of local file to upload to Google Storage.
      archive: Whether to automatically copy files to the archive dir.
      strict: Whether to treat upload errors as fatal.
    """
    acl = None if self._build_config['internal'] else 'public-read'
    filename = path
    if archive:
      filename = commands.ArchiveFile(path, self.archive_path)
    try:
      commands.UploadArchivedFile(self.archive_path, self.upload_url, filename,
                                  self.debug, update_list=True, acl=acl)
    except cros_build_lib.RunCommandError as e:
      cros_build_lib.PrintBuildbotStepText('Upload failed')
      if strict:
        raise
      # Treat gsutil flake as a warning if it's the only problem.
      self._HandleExceptionAsWarning(e)

  def _GetGSUtilArchiveDir(self):
    if self._options.archive_base:
      gs_base = self._options.archive_base
    elif (self._options.remote_trybot or
          self._build_config['gs_path'] == cbuildbot_config.GS_PATH_DEFAULT):
      gs_base = constants.DEFAULT_ARCHIVE_BUCKET
    else:
      return self._build_config['gs_path']

    return '%s/%s' % (gs_base, self._bot_id)

  def GetMetadata(self, stage=None, final_status=None, sync_instance=None):
    """Constructs the metadata json object.

    Args:
      stage: The stage name that this metadata file is being uploaded for.
      final_status: Whether the build passed or failed. If None, the build
        will be treated as still running.
      sync_instance: The stage instance that was used for syncing the source
        code. This should be a derivative of SyncStage. If None, the list of
        commit queue patches will not be included in the metadata.
    """
    config = self._build_config

    start_time = results_lib.Results.start_time
    current_time = datetime.datetime.now()
    start_time_stamp = cros_build_lib.UserDateTimeFormat(timeval=start_time)
    current_time_stamp = cros_build_lib.UserDateTimeFormat(timeval=current_time)
    duration = '%s' % (current_time - start_time,)

    sdk_verinfo = cros_build_lib.LoadKeyValueFile(
        os.path.join(self._build_root, constants.SDK_VERSION_FILE),
        ignore_missing=True)
    verinfo = self.archive_stage.GetVersionInfo()
    metadata = {
        # Version of the metadata format.
        'metadata-version': '2',
        # Data for this build.
        'bot-config': config['name'],
        'bot-hostname': cros_build_lib.GetHostName(fully_qualified=True),
        'boards': config['boards'],
        'build-number': self._options.buildnumber,
        'builder-name': os.environ.get('BUILDBOT_BUILDERNAME', ''),
        'status': {
            'current-time': current_time_stamp,
            'status': final_status if final_status else 'running',
            'summary': stage or '',
        },
        'time': {
            'start': start_time_stamp,
            'finish': current_time_stamp if final_status else '',
            'duration': duration,
        },
        'version': {
            'chrome': self.archive_stage.chrome_version,
            'full': self.version,
            'milestone': verinfo.chrome_branch,
            'platform': (self.archive_stage.release_tag
                         or verinfo.VersionString()),
        },
        # Data for the toolchain used.
        'sdk-version': sdk_verinfo.get('SDK_LATEST_VERSION', '<unknown>'),
        'toolchain-url': sdk_verinfo.get('TC_PATH', '<unknown>'),
    }
    if len(config['boards']) == 1:
      toolchains = toolchain.GetToolchainsForBoard(config['boards'][0],
                                                   buildroot=self._build_root)
      metadata['toolchain-tuple'] = (
          toolchain.FilterToolchains(toolchains, 'default', True).keys() +
          toolchain.FilterToolchains(toolchains, 'default', False).keys())

    metadata['results'] = []
    for name, result, description, run_time in results_lib.Results.Get():
      timestr = datetime.timedelta(seconds=math.ceil(run_time))
      if result in results_lib.Results.NON_FAILURE_TYPES:
        status = 'passed'
      else:
        status = 'failed'
      metadata['results'].append({
          'name': name,
          'status': status,
          # The result might be a custom exception.
          'summary': str(result),
          'duration': '%s' % timestr,
          'description': description,
          'log': self.ConstructDashboardURL(stage=name),
      })

    if (sync_instance and isinstance(sync_instance, CommitQueueSyncStage) and
        sync_instance.pool):
      changes = []
      for change in sync_instance.pool.changes:
        changes.append({'gerrit_number': change.gerrit_number,
                        'patch_number': change.patch_number,
                        'internal': change.internal})
      metadata['changes'] = changes

    return metadata

  def UploadMetadata(self, stage=None, upload_queue=None, **kwargs):
    """Create a JSON of various metadata describing this build."""
    metadata = self.GetMetadata(stage=stage, **kwargs)
    filename = constants.METADATA_JSON
    if stage is not None:
      filename = constants.METADATA_STAGE_JSON % { 'stage': stage }
    metadata_json = os.path.join(self.archive_path, filename)

    # Stages may run in parallel, so we have to do atomic updates on this.
    osutils.WriteFile(metadata_json, json.dumps(metadata), atomic=True)

    if upload_queue is not None:
      upload_queue.put([filename])
    else:
      self.UploadArtifact(filename, archive=False)


class CleanUpStage(bs.BuilderStage):
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
    """For trybots, clear all previus archive images to save space."""
    archive_root = ArchivingStage.GetArchiveRoot(self._build_root, trybot=True)
    shutil.rmtree(archive_root, ignore_errors=True)

  def _DeleteArchivedPerfResults(self):
    """Clear any previously stashed perf results from hw testing."""
    for result in glob.glob(os.path.join(
        self._options.log_dir, '*.%s' % HWTestStage.PERF_RESULTS_EXTENSION)):
      os.remove(result)

  def _DeleteChromeBuildOutput(self):
    chrome_src = os.path.join(self._options.chrome_root, 'src')
    for out_dir in glob.glob(os.path.join(chrome_src, 'out_*')):
      osutils.RmDir(out_dir)

  def PerformStage(self):
    if (not (self._options.buildbot or self._options.remote_trybot)
        and self._options.clobber):
      if not commands.ValidateClobber(self._build_root):
        cros_build_lib.Die("--clobber in local mode must be approved.")

    # If we can't get a manifest out of it, then it's not usable and must be
    # clobbered.
    manifest = None
    if not self._options.clobber:
      try:
        manifest = git.ManifestCheckout.Cached(self._build_root, search=False)
      except (KeyboardInterrupt, MemoryError, SystemExit):
        raise
      except Exception, e:
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
      repository.ClearBuildRoot(self._build_root, self._options.preserve_paths)
    else:
      commands.BuildRootGitCleanup(self._build_root, self._options.debug)
      tasks = [functools.partial(commands.BuildRootGitCleanup,
                                 self._build_root, self._options.debug),
               functools.partial(commands.WipeOldOutput, self._build_root),
               self._DeleteArchivedTrybotImages,
               self._DeleteArchivedPerfResults]
      if self._options.chrome_root:
        tasks.append(self._DeleteChromeBuildOutput)
      if self._build_config['chroot_replace'] and self._options.build:
        tasks.append(self._DeleteChroot)
      else:
        tasks.append(self._CleanChroot)
      parallel.RunParallelSteps(tasks)


class PatchChangesStage(bs.BuilderStage):
  """Stage that patches a set of Gerrit changes to the buildroot source tree."""
  def __init__(self, options, build_config, patch_pool):
    """Construct a PatchChangesStage.

    Args:
      options, build_config: See arguments to bs.BuilderStage.__init__()
      patch_pool: A TrybotPatchPool object containing the different types of
                  patches to apply.
    """
    bs.BuilderStage.__init__(self, options, build_config)
    self.patch_pool = patch_pool

  @staticmethod
  def _CheckForDuplicatePatches(_series, changes):
    conflicts = {}
    duplicates = []
    for change in changes:
      if change.id is None:
        cros_build_lib.Warning(
            "Change %s lacks a usable ChangeId; duplicate checking cannot "
            "be done for this change.  If cherry-picking fails, this is a "
            "potential cause.", change)
        continue
      conflicts.setdefault(change.id, []).append(change)

    duplicates = [x for x in conflicts.itervalues() if len(x) > 1]
    if not duplicates:
      return changes

    for conflict in duplicates:
      cros_build_lib.Error(
          "Changes %s conflict with each other- they have same id %s.",
          ', '.join(map(str, conflict)), conflict[0].id)

    cros_build_lib.Die("Duplicate patches were encountered: %s", duplicates)

  @staticmethod
  def _FixIncompleteRemotePatches(series, changes):
    """Identify missing remote patches from older cbuildbot instances.

    Cbuildbot, prior to I8ab6790de801900c115a437b5f4ebb9a24db542f, uploaded
    a single patch per project- despite if there may have been a hundred
    patches actually pulled in by that patch.  This method detects when
    we're dealing w/ the old incomplete version, and fills in those gaps."""
    broken = [x for x in changes
              if isinstance(x, cros_patch.UploadedLocalPatch)]
    if not broken:
      return changes

    changes = list(changes)
    known = cros_patch.PatchCache(changes)

    for change in broken:
      git_repo = series.GetGitRepoForChange(change)
      tracking = series.GetTrackingBranchForChange(change)
      branch = getattr(change, 'original_branch', tracking)

      for target in cros_patch.GeneratePatchesFromRepo(
          git_repo, change.project, tracking, branch, change.internal,
          allow_empty=True, starting_ref='%s^' % change.sha1):

        if target in known:
          continue

        known.Inject(target)
        changes.append(target)

    return changes

  def _PatchSeriesFilter(self, series, changes):
    if self._options.remote_version == 3:
      changes = self._FixIncompleteRemotePatches(series, changes)
    return self._CheckForDuplicatePatches(series, changes)

  def _ApplyPatchSeries(self, series, patch_pool, **kwargs):
    """Applies a patch pool using a patch series."""
    kwargs.setdefault('frozen', False)
    # Honor the given ordering, so that if a gerrit/remote patch
    # conflicts w/ a local patch, the gerrit/remote patch are
    # blamed rather than local (patch ordering is typically
    # local, gerrit, then remote).
    kwargs.setdefault('honor_ordering', True)
    kwargs['changes_filter'] = self._PatchSeriesFilter

    _applied, failed_tot, failed_inflight = series.Apply(
        list(patch_pool), **kwargs)

    failures = failed_tot + failed_inflight
    if failures:
      cros_build_lib.Die("Failed applying patches: %s",
                         "\n".join(map(str, failures)))

  def PerformStage(self):
    class NoisyPatchSeries(validation_pool.PatchSeries):
      """Custom PatchSeries that adds links to buildbot logs for remote trys."""

      def ApplyChange(self, change, dryrun=False):
        if isinstance(change, cros_patch.GerritPatch):
          cros_build_lib.PrintBuildbotLink(str(change), change.url)
        elif isinstance(change, cros_patch.UploadedLocalPatch):
          cros_build_lib.PrintBuildbotStepText(str(change))

        return validation_pool.PatchSeries.ApplyChange(self, change,
                                                       dryrun=dryrun)

    # If we're an external builder, ignore internal patches.
    helper_pool = validation_pool.HelperPool.SimpleCreate(
        cros_internal=self._build_config['internal'], cros=True)

    # Limit our resolution to non-manifest patches.
    patch_series = NoisyPatchSeries(
        self._build_root,
        force_content_merging=True,
        helper_pool=helper_pool,
        deps_filter_fn=lambda p: not trybot_patch_pool.ManifestFilter(p))

    self._ApplyPatchSeries(patch_series, self.patch_pool)


class BootstrapStage(PatchChangesStage):
  """Stage that patches a chromite repo and re-executes inside it.

  Attributes:
    returncode - the returncode of the cbuildbot re-execution.  Valid after
                 calling stage.Run().
  """
  option_name = 'bootstrap'

  def __init__(self, options, build_config, chromite_patch_pool,
               manifest_patch_pool=None):
    super(BootstrapStage, self).__init__(
        options, build_config, trybot_patch_pool.TrybotPatchPool())
    self.chromite_patch_pool = chromite_patch_pool
    self.manifest_patch_pool = manifest_patch_pool
    self.returncode = None

  def _ApplyManifestPatches(self, patch_pool):
    """Apply a pool of manifest patches to a temp manifest checkout.

    Arguments:
      filter_fn: Used to filter changes during dependency resolution.

    Returns:
      The path to the patched manifest checkout.

    Raises:
      Exception, if the new patched manifest cannot be parsed.
    """
    checkout_dir = os.path.join(self.tempdir, 'manfest-checkout')
    repository.CloneGitRepo(checkout_dir,
                            self._build_config['manifest_repo_url'])

    patch_series = validation_pool.PatchSeries.WorkOnSingleRepo(
        checkout_dir, deps_filter_fn=trybot_patch_pool.ManifestFilter,
        tracking_branch=self._target_manifest_branch)

    self._ApplyPatchSeries(patch_series, patch_pool)
    # Create the branch that 'repo init -b <target_branch> -u <patched_repo>'
    # will look for.
    cmd = ['branch', '-f', self._target_manifest_branch, constants.PATCH_BRANCH]
    git.RunGit(checkout_dir, cmd)

    # Verify that the patched manifest loads properly. Propagate any errors as
    # exceptions.
    manifest = os.path.join(checkout_dir, self._build_config['manifest'])
    git.Manifest.Cached(manifest, manifest_include_dir=checkout_dir)
    return checkout_dir

  @staticmethod
  def _FilterArgsForApi(parsed_args, api_minor):
    """Remove arguments that are introduced after an api version."""
    def filter_fn(passed_arg):
      return passed_arg.opt_inst.api_version <= api_minor

    accepted, removed = commandline.FilteringParser.FilterArgs(
        parsed_args, filter_fn)

    if removed:
      cros_build_lib.Warning('The following arguments were removed due to api: '
                             "'%s'" % ' '.join(removed))
    return accepted

  @classmethod
  def FilterArgsForTargetCbuildbot(cls, buildroot, cbuildbot_path, options):
    _, minor = cros_build_lib.GetTargetChromiteApiVersion(buildroot)
    args = [cbuildbot_path]
    args.extend(options.build_targets)
    args.extend(cls._FilterArgsForApi(options.parsed_args, minor))

    # Only pass down --cache-dir if it was specified. By default, we want
    # the cache dir to live in the root of each checkout, so this means that
    # each instance of cbuildbot needs to calculate the default separately.
    if minor >= 2 and options.cache_dir_specified:
      args += ['--cache-dir', options.cache_dir]

    return args

  #pylint: disable=E1101
  @osutils.TempDirDecorator
  def PerformStage(self):
    # The plan for the builders is to use master branch to bootstrap other
    # branches. Now, if we wanted to test patches for both the bootstrap code
    # (on master) and the branched chromite (say, R20), we need to filter the
    # patches by branch.
    filter_branch = self._target_manifest_branch
    if self._options.test_bootstrap:
      filter_branch = 'master'

    chromite_dir = os.path.join(self.tempdir, 'chromite')
    reference_repo = os.path.join(constants.SOURCE_ROOT, 'chromite', '.git')
    repository.CloneGitRepo(chromite_dir, constants.CHROMITE_URL,
                            reference=reference_repo)
    git.RunGit(chromite_dir, ['checkout', filter_branch])

    def BranchAndChromiteFilter(patch):
      return (trybot_patch_pool.BranchFilter(filter_branch, patch) and
              trybot_patch_pool.ChromiteFilter(patch))

    patch_series = validation_pool.PatchSeries.WorkOnSingleRepo(
        chromite_dir, filter_branch,
        deps_filter_fn=BranchAndChromiteFilter)

    filtered_pool = self.chromite_patch_pool.FilterBranch(filter_branch)
    if filtered_pool:
      self._ApplyPatchSeries(patch_series, filtered_pool)

    cbuildbot_path = constants.PATH_TO_CBUILDBOT
    if not os.path.exists(os.path.join(self.tempdir, cbuildbot_path)):
      cbuildbot_path = 'chromite/buildbot/cbuildbot'
    cmd = self.FilterArgsForTargetCbuildbot(self.tempdir, cbuildbot_path,
                                       self._options)

    extra_params = ['--sourceroot=%s' % self._options.sourceroot]
    extra_params.extend(self._options.bootstrap_args)
    if self._options.test_bootstrap:
      # We don't want re-executed instance to see this.
      cmd = [a for a in cmd if a != '--test-bootstrap']
    else:
      # If we've already done the desired number of bootstraps, disable
      # bootstrapping for the next execution.  Also pass in the patched manifest
      # repository.
      extra_params.append('--nobootstrap')
      if self.manifest_patch_pool:
        manifest_dir = self._ApplyManifestPatches(self.manifest_patch_pool)
        extra_params.extend(['--manifest-repo-url', manifest_dir])

    cmd += extra_params
    result_obj = cros_build_lib.RunCommand(
        cmd, cwd=self.tempdir, kill_timeout=30, error_code_ok=True)
    self.returncode = result_obj.returncode


class SyncStage(bs.BuilderStage):
  """Stage that performs syncing for the builder."""

  option_name = 'sync'
  output_manifest_sha1 = True

  def __init__(self, options, build_config):
    super(SyncStage, self).__init__(options, build_config)
    self.repo = None
    self.skip_sync = False
    self.internal = self._build_config['internal']

  def _GetManifestVersionsRepoUrl(self, read_only=False):
    return cbuildbot_config.GetManifestVersionsRepoUrl(
        self.internal,
        read_only=read_only)

  def Initialize(self):
    self._InitializeRepo()

  def _InitializeRepo(self, build_root=None, **kwds):
    if build_root is None:
      build_root = self._build_root

    manifest_url = self._options.manifest_repo_url
    if manifest_url is None:
      manifest_url = self._build_config['manifest_repo_url']

    kwds.setdefault('referenced_repo', self._options.reference_repo)
    kwds.setdefault('branch', self._target_manifest_branch)
    kwds.setdefault('manifest', self._build_config['manifest'])

    self.repo = repository.RepoRepository(manifest_url, build_root, **kwds)

  def GetNextManifest(self):
    """Returns the manifest to use."""
    return self._build_config['manifest']

  def ManifestCheckout(self, next_manifest):
    """Checks out the repository to the given manifest."""
    self._Print('\n'.join(['BUILDROOT: %s' % self.repo.directory,
                           'TRACKING BRANCH: %s' % self.repo.branch,
                           'NEXT MANIFEST: %s' % next_manifest]))

    if not self.skip_sync:
      self.repo.Sync(next_manifest)
    print >> sys.stderr, self.repo.ExportManifest(
        mark_revision=self.output_manifest_sha1)

  def PerformStage(self):
    self.Initialize()
    with osutils.TempDir() as tempdir:
      # Save off the last manifest.
      fresh_sync = True
      if os.path.exists(self.repo.directory) and not self._options.clobber:
        old_filename = os.path.join(tempdir, 'old.xml')
        try:
          old_contents = self.repo.ExportManifest()
        except cros_build_lib.RunCommandError as e:
          cros_build_lib.Warning(str(e))
        else:
          osutils.WriteFile(old_filename, old_contents)
          fresh_sync = False

      # Sync.
      self.ManifestCheckout(self.GetNextManifest())

      # Print the blamelist.
      if fresh_sync:
        cros_build_lib.PrintBuildbotStepText('(From scratch)')
      elif self._options.buildbot:
        lkgm_manager.GenerateBlameList(self.repo, old_filename)


class LKGMSyncStage(SyncStage):
  """Stage that syncs to the last known good manifest blessed by builders."""

  output_manifest_sha1 = False

  def GetNextManifest(self):
    """Override: Gets the LKGM."""
    # TODO(sosa):  Should really use an initialized manager here.
    if self.internal:
      mv_dir = 'manifest-versions-internal'
    else:
      mv_dir = 'manifest-versions'

    manifest_path = os.path.join(self._build_root, mv_dir)
    manifest_repo = self._GetManifestVersionsRepoUrl(read_only=True)
    manifest_version.RefreshManifestCheckout(manifest_path, manifest_repo)
    return os.path.join(manifest_path, lkgm_manager.LKGMManager.LKGM_PATH)


class ChromeLKGMSyncStage(SyncStage):
  """Stage that syncs to the last known good manifest for Chrome."""

  output_manifest_sha1 = False

  def GetNextManifest(self):
    """Override: Gets the LKGM from the Chrome tree."""
    chrome_lkgm = commands.GetChromeLKGM(self._options.chrome_version)

    # We need a full buildspecs manager here as we need an initialized manifest
    # manager with paths to the spec.
    manifest_manager = manifest_version.BuildSpecsManager(
      source_repo=self.repo,
      manifest_repo=self._GetManifestVersionsRepoUrl(read_only=False),
      build_name=self._bot_id,
      incr_type='build',
      force=False,
      branch=self._target_manifest_branch)

    manifest_manager.BootstrapFromVersion(chrome_lkgm)
    return manifest_manager.GetLocalManifest(chrome_lkgm)


class ManifestVersionedSyncStage(SyncStage):
  """Stage that generates a unique manifest file, and sync's to it."""

  manifest_manager = None
  output_manifest_sha1 = False

  def __init__(self, options, build_config):
    # Perform the sync at the end of the stage to the given manifest.
    super(ManifestVersionedSyncStage, self).__init__(options, build_config)
    self.repo = None

    # If a builder pushes changes (even with dryrun mode), we need a writable
    # repository. Otherwise, the push will be rejected by the server.
    self.manifest_repo = self._GetManifestVersionsRepoUrl(read_only=False)

    # 1. If we're uprevving Chrome, Chrome might have changed even if the
    #    manifest has not, so we should force a build to double check. This
    #    means that we'll create a new manifest, even if there are no changes.
    # 2. If we're running with --debug, we should always run through to
    #    completion, so as to ensure a complete test.
    self._force = self._chrome_rev or options.debug

  def HandleSkip(self):
    """Initializes a manifest manager to the specified version if skipped."""
    super(ManifestVersionedSyncStage, self).HandleSkip()
    if self._options.force_version:
      self.Initialize()
      self.ForceVersion(self._options.force_version)

  def ForceVersion(self, version):
    """Creates a manifest manager from given version and returns manifest."""
    cros_build_lib.PrintBuildbotStepText(version)
    return ManifestVersionedSyncStage.manifest_manager.BootstrapFromVersion(
        version)

  def Initialize(self):
    """Initializes a manager that manages manifests for associated stages."""
    increment = ('build' if self._target_manifest_branch == 'master'
                 else 'branch')

    dry_run = self._options.debug

    self._InitializeRepo()

    # If chrome_rev is somehow set, fail.
    assert not self._chrome_rev, \
        'chrome_rev is unsupported on release builders.'

    ManifestVersionedSyncStage.manifest_manager = \
        manifest_version.BuildSpecsManager(
            source_repo=self.repo,
            manifest_repo=self.manifest_repo,
            manifest=self._build_config['manifest'],
            build_name=self._bot_id,
            incr_type=increment,
            force=self._force,
            branch=self._target_manifest_branch,
            dry_run=dry_run,
            master=self._build_config['master'])

  def GetNextManifest(self):
    """Uses the initialized manifest manager to get the next manifest."""
    assert self.manifest_manager, \
        'Must run GetStageManager before checkout out build.'

    to_return = self.manifest_manager.GetNextBuildSpec()
    previous_version = self.manifest_manager.GetLatestPassingSpec()
    target_version = self.manifest_manager.current_version

    # Print the Blamelist here.
    url_prefix = 'http://chromeos-images.corp.google.com/diff/report?'
    url = url_prefix + 'from=%s&to=%s' % (previous_version, target_version)
    cros_build_lib.PrintBuildbotLink('Blamelist', url)
    # The testManifestVersionedSyncOnePartBranch interacts badly with this
    # function.  It doesn't fully initialize self.manifest_manager which
    # causes target_version to be None.  Since there isn't a clean fix in
    # either direction, just throw this through str().  In the normal case,
    # it's already a string anyways.
    cros_build_lib.PrintBuildbotStepText(str(target_version))

    return to_return

  @contextlib.contextmanager
  def LocalizeManifest(self, manifest, filter_cros=False):
    """Remove restricted projects from the manifest if needed.

    Arguments:
      filter_cros: If set, then only projects with a remote of 'cros' or
        'cros-internal' are kept, and the rest are filtered out.
    """
    if filter_cros:
      with osutils.TempDir() as tempdir:
        filtered_manifest = os.path.join(tempdir, 'filtered.xml')
        doc = ElementTree.parse(manifest)
        root = doc.getroot()
        for project in root.findall('project'):
          remote = project.attrib.get('remote')
          if remote and remote not in constants.GIT_REMOTES:
            root.remove(project)
        doc.write(filtered_manifest)
        yield filtered_manifest
    else:
      yield manifest

  def PerformStage(self):
    self.Initialize()
    if self._options.force_version:
      next_manifest = self.ForceVersion(self._options.force_version)
    else:
      next_manifest = self.GetNextManifest()

    if not next_manifest:
      cros_build_lib.Info('Found no work to do.')
      if ManifestVersionedSyncStage.manifest_manager.DidLastBuildFail():
        raise results_lib.StepFailure('The previous build failed.')
      else:
        sys.exit(0)

    # Log this early on for the release team to grep out before we finish.
    if ManifestVersionedSyncStage.manifest_manager:
      self._Print('\nRELEASETAG: %s\n' % (
          ManifestVersionedSyncStage.manifest_manager.current_version))

    # To keep local trybots working, remove restricted projects from the
    # official manifest we get from manifest-versions.
    with self.LocalizeManifest(
        next_manifest, filter_cros=self._options.local) as new_manifest:
      self.ManifestCheckout(new_manifest)


class LKGMCandidateSyncStage(ManifestVersionedSyncStage):
  """Stage that generates a unique manifest file candidate, and sync's to it."""

  sub_manager = None

  def __init__(self, options, build_config):
    super(LKGMCandidateSyncStage, self).__init__(options, build_config)
    # lkgm_manager deals with making sure we're synced to whatever manifest
    # we get back in GetNextManifest so syncing again is redundant.
    self.skip_sync = True

  def _GetInitializedManager(self, internal):
    """Returns an initialized lkgm manager."""
    increment = ('build' if self._target_manifest_branch == 'master'
                 else 'branch')
    return lkgm_manager.LKGMManager(
        source_repo=self.repo,
        manifest_repo=cbuildbot_config.GetManifestVersionsRepoUrl(
            internal, read_only=False),
        manifest=self._build_config['manifest'],
        build_name=self._bot_id,
        build_type=self._build_config['build_type'],
        incr_type=increment,
        force=self._force,
        branch=self._target_manifest_branch,
        dry_run=self._options.debug,
        master=self._build_config['master'])

  def Initialize(self):
    """Override: Creates an LKGMManager rather than a ManifestManager."""
    self._InitializeRepo()
    ManifestVersionedSyncStage.manifest_manager = self._GetInitializedManager(
        self.internal)
    if (self._build_config['master'] and
        self._GetSlavesForMaster(self._build_config)):
      assert self.internal, 'Unified masters must use an internal checkout.'
      LKGMCandidateSyncStage.sub_manager = self._GetInitializedManager(False)

  def ForceVersion(self, version):
    manifest = super(LKGMCandidateSyncStage, self).ForceVersion(version)
    if LKGMCandidateSyncStage.sub_manager:
      LKGMCandidateSyncStage.sub_manager.BootstrapFromVersion(version)

    return manifest

  def GetNextManifest(self):
    """Gets the next manifest using LKGM logic."""
    assert self.manifest_manager, \
        'Must run Initialize before we can get a manifest.'
    assert isinstance(self.manifest_manager, lkgm_manager.LKGMManager), \
        'Manifest manager instantiated with wrong class.'

    if self._build_config['master']:
      manifest = self.manifest_manager.CreateNewCandidate()
      if LKGMCandidateSyncStage.sub_manager:
        LKGMCandidateSyncStage.sub_manager.CreateFromManifest(manifest)
      return manifest
    else:
      return self.manifest_manager.GetLatestCandidate()


class CommitQueueSyncStage(LKGMCandidateSyncStage):
  """Commit Queue Sync stage that handles syncing and applying patches.

  This stage handles syncing to a manifest, passing around that manifest to
  other builders and finding the Gerrit Reviews ready to be committed and
  applying them into its out checkout.
  """

  def __init__(self, options, build_config):
    super(CommitQueueSyncStage, self).__init__(options, build_config)
    # Figure out the builder's name from the buildbot waterfall.
    builder_name = build_config['paladin_builder_name']
    self.builder_name = builder_name if builder_name else build_config['name']

    # The pool of patches to be picked up by the commit queue.
    # - For the master commit queue, it's initialized in GetNextManifest.
    # - For slave commit queues, it's initialized in SetPoolFromManifest.
    #
    # In all cases, the pool is saved to disk, and refreshed after bootstrapping
    # by HandleSkip.
    self.pool = None

  def HandleSkip(self):
    """Handles skip and initializes validation pool from manifest."""
    super(CommitQueueSyncStage, self).HandleSkip()
    filename = self._options.validation_pool
    if filename:
      self.pool = validation_pool.ValidationPool.Load(filename)
    else:
      self.SetPoolFromManifest(self.manifest_manager.GetLocalManifest())

  def ChangeFilter(self, pool, changes, non_manifest_changes):
    # First, look for changes that were tested by the Pre-CQ.
    changes_to_test = []
    for change in changes:
      status = pool.GetPreCQStatus(change)
      if status == manifest_version.BuilderStatus.STATUS_PASSED:
        changes_to_test.append(change)

    # If we only see changes that weren't verified by Pre-CQ, try all of the
    # changes. This ensures that the CQ continues to work even if the Pre-CQ is
    # down.
    if not changes_to_test:
      changes_to_test = changes

    return changes_to_test, non_manifest_changes

  def SetPoolFromManifest(self, manifest):
    """Sets validation pool based on manifest path passed in."""
    # Note that GetNextManifest() calls GetLatestCandidate() in this case,
    # so the repo will already be sync'd appropriately. This means that
    # AcquirePoolFromManifest doesn't need to sync.
    self.pool = validation_pool.ValidationPool.AcquirePoolFromManifest(
        manifest, self._build_config['overlays'], self.repo,
        self._options.buildnumber, self.builder_name,
        self._build_config['master'], self._options.debug)

  def GetNextManifest(self):
    """Gets the next manifest using LKGM logic."""
    assert self.manifest_manager, \
        'Must run Initialize before we can get a manifest.'
    assert isinstance(self.manifest_manager, lkgm_manager.LKGMManager), \
        'Manifest manager instantiated with wrong class.'

    if self._build_config['master']:
      try:
        # In order to acquire a pool, we need an initialized buildroot.
        if not git.FindRepoDir(self.repo.directory):
          self.repo.Initialize()

        pool = validation_pool.ValidationPool.AcquirePool(
            self._build_config['overlays'], self.repo,
            self._options.buildnumber, self.builder_name,
            self._options.debug, check_tree_open=not self._options.debug,
            changes_query=self._options.cq_gerrit_override,
            change_filter=self.ChangeFilter)

        # We only have work to do if there are changes to try.
        try:
          # Try our best to submit these but may have been overridden and won't
          # let that stop us from continuing the build.
          pool.SubmitNonManifestChanges()
        except validation_pool.FailedToSubmitAllChangesException as e:
          cros_build_lib.Warning(str(e))

        self.pool = pool

      except validation_pool.TreeIsClosedException as e:
        cros_build_lib.Warning(str(e))
        return None

      manifest = self.manifest_manager.CreateNewCandidate(validation_pool=pool)
      if LKGMCandidateSyncStage.sub_manager:
        LKGMCandidateSyncStage.sub_manager.CreateFromManifest(manifest)

      return manifest
    else:
      manifest = self.manifest_manager.GetLatestCandidate()
      if manifest:
        self.SetPoolFromManifest(manifest)
        self.pool.ApplyPoolIntoRepo()

      return manifest

  def PerformStage(self):
    """Performs normal stage and prints blamelist at end."""
    if self._options.force_version:
      self.HandleSkip()
    else:
      ManifestVersionedSyncStage.PerformStage(self)


class ManifestVersionedSyncCompletionStage(ForgivingBuilderStage):
  """Stage that records board specific results for a unique manifest file."""

  option_name = 'sync'

  def __init__(self, options, build_config, sync_stage, success):
    super(ManifestVersionedSyncCompletionStage, self).__init__(
        options, build_config)
    self.sync_stage = sync_stage
    self.success = success
    # Message that can be set that well be sent along with the status in
    # UpdateStatus.
    self.message = None

  def PerformStage(self):
    if ManifestVersionedSyncStage.manifest_manager:
      ManifestVersionedSyncStage.manifest_manager.UpdateStatus(
          success=self.success, message=self.message)


class ImportantBuilderFailedException(Exception):
  """Exception thrown when an important build fails to build."""
  pass


class LKGMCandidateSyncCompletionStage(ManifestVersionedSyncCompletionStage):
  """Stage that records whether we passed or failed to build/test manifest."""

  def _GetSlavesStatus(self):
    if self._options.debug:
      # In debug mode, nothing is uploaded to Google Storage, so we bypass
      # the extra hop and just look at what we have locally.
      status = manifest_version.BuilderStatus.GetCompletedStatus(self.success)
      status_obj = manifest_version.BuilderStatus(status, self.message)
      return {self._bot_id: status_obj}
    elif not self._build_config['master']:
      # Slaves only need to look at their own status.
      return ManifestVersionedSyncStage.manifest_manager.GetBuildersStatus(
          [self._bot_id])
    else:
      builders = self._GetSlavesForMaster(self._build_config)
      manager = ManifestVersionedSyncStage.manifest_manager
      sub_manager = LKGMCandidateSyncStage.sub_manager
      if sub_manager:
        public_builders = [b['name'] for b in builders if not b['internal']]
        statuses = sub_manager.GetBuildersStatus(public_builders)
        private_builders = [b['name'] for b in builders if b['internal']]
        statuses.update(manager.GetBuildersStatus(private_builders))
      else:
        statuses = manager.GetBuildersStatus([b['name'] for b in builders])
      return statuses

  def _AbortCQHWTests(self):
    """Abort any HWTests started by the CQ."""
    manifest_manager = ManifestVersionedSyncStage.manifest_manager
    if (cbuildbot_config.IsCQType(self._build_config['build_type']) and
        manifest_manager is not None and
        self._target_manifest_branch == 'master'):
      release_tag = manifest_manager.current_version
      if release_tag and not commands.HaveHWTestsBeenAborted(release_tag):
        commands.AbortHWTests(release_tag, self._options.debug)

  def HandleSuccess(self):
    # We only promote for the pfq, not chrome pfq.
    # TODO(build): Run this logic in debug mode too.
    if (not self._options.debug and
        cbuildbot_config.IsPFQType(self._build_config['build_type']) and
        self._build_config['master'] and
        self._target_manifest_branch == 'master' and
        ManifestVersionedSyncStage.manifest_manager is not None and
        self._build_config['build_type'] != constants.CHROME_PFQ_TYPE):
      ManifestVersionedSyncStage.manifest_manager.PromoteCandidate()
      if LKGMCandidateSyncStage.sub_manager:
        LKGMCandidateSyncStage.sub_manager.PromoteCandidate()

  def HandleValidationFailure(self, failing_statuses):
    cros_build_lib.PrintBuildbotStepWarnings()
    cros_build_lib.Warning('\n'.join([
        'The following builders failed with this manifest:',
        ', '.join(sorted(failing_statuses.keys())),
        'Please check the logs of the failing builders for details.']))

  def HandleValidationTimeout(self, inflight_statuses):
    cros_build_lib.PrintBuildbotStepWarnings()
    cros_build_lib.Warning('\n'.join([
        'The following builders took too long to finish:',
        ', '.join(sorted(inflight_statuses.keys())),
        'Please check the logs of these builders for details.']))

  def PerformStage(self):
    if ManifestVersionedSyncStage.manifest_manager:
      ManifestVersionedSyncStage.manifest_manager.UploadStatus(
          success=self.success, message=self.message)
      if not self.success and self._build_config['important']:
        tracebacks = results_lib.Results.GetTracebacks()
        if len(tracebacks) != 1 or tracebacks[0] != 'HWTest':
          self._AbortCQHWTests()

      statuses = self._GetSlavesStatus()
      failing_build_dict, inflight_build_dict = {}, {}
      for builder, status in statuses.iteritems():
        if status.Failed():
          failing_build_dict[builder] = status
        elif status.Inflight():
          inflight_build_dict[builder] = status

      if failing_build_dict or inflight_build_dict:
        if failing_build_dict:
          self.HandleValidationFailure(failing_build_dict)

        if inflight_build_dict:
          self.HandleValidationTimeout(inflight_build_dict)

      if failing_build_dict or inflight_build_dict:
        raise results_lib.StepFailure()
      else:
        self.HandleSuccess()


class CommitQueueCompletionStage(LKGMCandidateSyncCompletionStage):
  """Commits or reports errors to CL's that failed to be validated."""

  def HandleSuccess(self):
    if self._build_config['master']:
      self.sync_stage.pool.SubmitPool()
      # After submitting the pool, update the commit hashes for uprevved
      # ebuilds.
      manifest = git.ManifestCheckout.Cached(self._build_root)
      portage_utilities.EBuild.UpdateCommitHashesForChanges(
          self.sync_stage.pool.changes, self._build_root, manifest)
      if cbuildbot_config.IsPFQType(self._build_config['build_type']):
        super(CommitQueueCompletionStage, self).HandleSuccess()

  def HandleValidationFailure(self, failing_statuses):
    """Sends the failure message of all failing builds in one go."""
    super(CommitQueueCompletionStage, self).HandleValidationFailure(
        failing_statuses)

    if self._build_config['master']:
      failing_messages = [x.message for x in failing_statuses.itervalues()]
      self.sync_stage.pool.HandleValidationFailure(failing_messages)

  def HandleValidationTimeout(self, inflight_builders):
    super(CommitQueueCompletionStage, self).HandleValidationTimeout(
        inflight_builders)
    self.sync_stage.pool.HandleValidationTimeout()

  def PerformStage(self):
    if not self.success and self._build_config['important']:
      # This message is sent along with the failed status to the master to
      # indicate a failure.
      self.message = self.sync_stage.pool.GetValidationFailedMessage()

    super(CommitQueueCompletionStage, self).PerformStage()

    if ManifestVersionedSyncStage.manifest_manager:
      ManifestVersionedSyncStage.manifest_manager.UpdateStatus(
          success=self.success, message=self.message)


class PreCQSyncStage(SyncStage):
  """Sync and apply patches to test if they compile."""

  def __init__(self, options, build_config, patches):
    super(PreCQSyncStage, self).__init__(options, build_config)

    # The list of patches to test.
    self.patches = patches

    # The ValidationPool of patches to test. Initialized in PerformStage, and
    # refreshed after bootstrapping by HandleSkip.
    self.pool = None

  def HandleSkip(self):
    """Handles skip and loads validation pool from disk."""
    super(PreCQSyncStage, self).HandleSkip()
    filename = self._options.validation_pool
    if filename:
      self.pool = validation_pool.ValidationPool.Load(filename)

  def PerformStage(self):
    super(PreCQSyncStage, self).PerformStage()
    self.pool = validation_pool.ValidationPool.AcquirePreCQPool(
        self._build_config['overlays'], self._build_root,
        self._options.buildnumber, self._build_config['name'],
        dryrun=self._options.debug_forced, changes=self.patches)
    self.pool.ApplyPoolIntoRepo()


class PreCQCompletionStage(bs.BuilderStage):
  """Reports the status of a trybot run to Google Storage and Gerrit."""

  def __init__(self, options, build_config, sync_stage, success):
    super(PreCQCompletionStage, self).__init__(options, build_config)
    self.sync_stage = sync_stage
    self.success = success

  def PerformStage(self):
    # Update Gerrit and Google Storage with the Pre-CQ status.
    if self.success:
      self.sync_stage.pool.HandlePreCQSuccess()
    else:
      message = self.sync_stage.pool.GetValidationFailedMessage()
      self.sync_stage.pool.HandleValidationFailure([message])


class PreCQLauncherStage(SyncStage):
  """Scans for CLs and automatically launches Pre-CQ jobs to test them."""

  STATUS_INFLIGHT = validation_pool.ValidationPool.STATUS_INFLIGHT
  STATUS_PASSED = validation_pool.ValidationPool.STATUS_PASSED
  STATUS_FAILED = validation_pool.ValidationPool.STATUS_FAILED
  STATUS_LAUNCHING = validation_pool.ValidationPool.STATUS_LAUNCHING
  STATUS_WAITING = validation_pool.ValidationPool.STATUS_WAITING

  # The number of minutes we allow before considering a launch attempt failed.
  # If this window isn't hit in a given launcher run, the window will start
  # again from scratch in the next run.
  LAUNCH_DELAY = 10

  def __init__(self, options, build_config):
    super(PreCQLauncherStage, self).__init__(options, build_config)
    self.skip_sync = True
    self.launching = {}
    self.retried = set()

  def _HasLaunchTimedOut(self, change):
    """Check whether a given |change| has timed out on its trybot launch.

    Assumes that the change is in the middle of being launched.

    Returns:
      True if the change has timed out. False otherwise.
    """
    diff = datetime.timedelta(minutes=self.LAUNCH_DELAY)
    return datetime.datetime.now() - self.launching[change] > diff

  def GetPreCQStatus(self, pool, changes):
    """Get the Pre-CQ status of a list of changes.

    Args:
      pool: The validation pool.
      changes: Changes to examine.

    Returns:
      busy: The set of CLs that are currently being tested.
      passed: The set of CLs that have been verified.
    """
    busy, passed = set(), set()

    for change in changes:
      status = pool.GetPreCQStatus(change)

      if status != self.STATUS_LAUNCHING:
        # The trybot has finished launching, so we should remove it from our
        # data structures.
        self.launching.pop(change, None)

      if status == self.STATUS_LAUNCHING:
        # The trybot is in the process of launching.
        busy.add(change)
        if change not in self.launching:
          self.launching[change] = datetime.datetime.now()
        elif self._HasLaunchTimedOut(change):
          if change in self.retried:
            msg = 'Failed twice to launch a Pre-CQ trybot for this change.'
            pool.SendNotification(change, '%(details)s', details=msg)
            pool.RemoveCommitReady(change)
            pool.UpdatePreCQStatus(change, self.STATUS_FAILED)
            self.retried.discard(change)
          else:
            # Try the change again.
            self.retried.add(change)
            pool.UpdatePreCQStatus(change, self.STATUS_WAITING)
      elif status == self.STATUS_INFLIGHT:
        # Once a Pre-CQ run actually starts, it'll set the status to
        # STATUS_INFLIGHT.
        busy.add(change)
      elif status == self.STATUS_FAILED:
        # The Pre-CQ run failed for this change. It's possible that we got
        # unlucky and this change was just marked as 'Not Ready' by a bot. To
        # test this, mark the CL as 'waiting' for now. If the CL is still marked
        # as 'Ready' next time we check, we'll know the CL is truly still ready.
        busy.add(change)
        pool.UpdatePreCQStatus(change, self.STATUS_WAITING)
      elif status == self.STATUS_PASSED:
        passed.add(change)

    return busy, passed

  def LaunchTrybot(self, pool, plan):
    """Launch a Pre-CQ run with the provided list of CLs.

    Args:
      plan: The list of patches to test in the Pre-CQ run.
    """
    cmd = ['cbuildbot', '--remote', '--nobootstrap',
           constants.PRE_CQ_BUILDER_NAME]
    if self._options.debug_forced:
      cmd.append('--debug')
    for patch in plan:
      number = cros_patch.FormatGerritNumber(
          patch.gerrit_number, force_internal=patch.internal)
      cmd += ['-g', number]
    cros_build_lib.RunCommand(cmd, cwd=self._build_root)
    for patch in plan:
      if pool.GetPreCQStatus(patch) != self.STATUS_PASSED:
        pool.UpdatePreCQStatus(patch, self.STATUS_LAUNCHING)

  def GetDisjointTransactionsToTest(self, pool, changes):
    """Get the list of disjoint transactions to test.

    Returns:
      A list of disjoint transactions to test. Each transaction should be sent
      to a different Pre-CQ trybot.
    """
    busy, passed = self.GetPreCQStatus(pool, changes)

    # Create a list of disjoint transactions to test.
    manifest = git.ManifestCheckout.Cached(self._build_root)
    plans = pool.CreateDisjointTransactions(manifest)
    for plan in plans:
      # If any of the CLs in the plan are currently "busy" being tested,
      # wait until they're done before launching our trybot run. This helps
      # avoid race conditions.
      #
      # Similarly, if all of the CLs in the plan have already been validated,
      # there's no need to launch a trybot run.
      plan = set(plan)
      if plan.issubset(passed):
        logging.info('CLs already verified: %r', ' '.join(map(str, plan)))
      elif plan.intersection(busy):
        logging.info('CLs currently being verified: %r',
                     ' '.join(map(str, plan.intersection(busy))))
        if plan.difference(busy):
          logging.info('CLs waiting on verification of dependencies: %r',
              ' '.join(map(str, plan.difference(busy))))
      else:
        yield plan

  def ProcessChanges(self, pool, changes, _non_manifest_changes):
    """Process a list of changes that were marked as Ready.

    From our list of changes that were marked as Ready, we create a
    list of disjoint transactions and send each one to a separate Pre-CQ
    trybot.

    Non-manifest changes are just submitted here because they don't need to be
    verified by either the Pre-CQ or CQ.
    """
    # Submit non-manifest changes if we can.
    if cros_build_lib.TreeOpen(
        validation_pool.ValidationPool.STATUS_URL, 0, max_timeout=0):
      try:
        pool.SubmitNonManifestChanges(check_tree_open=False)
      except validation_pool.FailedToSubmitAllChangesException as e:
        cros_build_lib.Warning(str(e))

    # Launch trybots for manifest changes.
    for plan in self.GetDisjointTransactionsToTest(pool, changes):
      self.LaunchTrybot(pool, plan)

    # Tell ValidationPool to keep waiting for more changes until we hit
    # its internal timeout.
    return [], []

  def PerformStage(self):
    # Setup and initialize the repo.
    super(PreCQLauncherStage, self).PerformStage()

    # Loop through all of the changes until we hit a timeout.
    validation_pool.ValidationPool.AcquirePool(
        self._build_config['overlays'], self.repo,
        self._options.buildnumber, urllib.quote(constants.PRE_CQ_LAUNCHER_NAME),
        dryrun=self._options.debug_forced,
        changes_query=self._options.cq_gerrit_override,
        check_tree_open=False, change_filter=self.ProcessChanges)


class BranchError(Exception):
  """Raised by branch creation code on error."""


class BranchUtilStage(bs.BuilderStage):
  """Creates, deletes and renames branches, depending on cbuildbot options.

  The two main types of branches are release branches and non-release
  branches.  Release branches have the form 'release-*' - e.g.,
  'release-R29-4319.B'.

  On a very basic level, a branch is created by parsing the manifest of a
  specific version of Chrome OS (e.g., 4319.0.0), and creating the branch
  remotely for each project in the manifest at the specified hash.

  Once a branch is created however, the branch component of the version on the
  newly created branch needs to be incremented.  Additionally, in some cases
  the Chrome major version (i.e, R29) and/or the Chrome OS version (i.e.,
  4319.0.0) of the source branch must be incremented
  (see IncrementVersionForSourceBranch docstring).  Finally, the external and
  internal manifests of the new branch need to be fixed up (see
  FixUpManifests docstring).
  """

  COMMIT_MESSAGE = 'Bump %(target)s after branching %(branch)s'

  def __init__(self, options, build_config):
    super(BranchUtilStage, self).__init__(options, build_config)
    self.dryrun = self._options.debug_forced
    self.dest_ref = git.NormalizeRef(self._options.branch_name)
    self.rename_to = git.NormalizeRef(self._options.rename_to)

  def RunPush(self, project, src_ref=None, dest_ref=None, force=False):
    """Perform a git push for a project.

    Args:
      project: A dictionary of project manifest attributes.
      src_ref: The source local ref to push to the remote.
      dest_ref: The destination ref name.
      force: Whether to override non-fastforward checks.
    """
    if src_ref is None:
      src_ref = project['revision']
    if dest_ref is None:
      dest_ref = self.dest_ref

    remote = project['push_remote']
    push_to = git.RemoteRef(remote, dest_ref)
    git.GitPush(project['local_path'], src_ref, push_to, dryrun=self.dryrun,
                force=force)

  def FetchAndCheckoutTo(self, project_dir, remote_ref):
    """Fetch a remote ref and check out to it.

    Args:
      project_dir: Path to git repo to operate on.
      remote_ref: A git.RemoteRef object.
    """
    git.RunGit(project_dir, ['fetch', remote_ref.remote, remote_ref.ref],
               print_cmd=True)
    git.RunGit(project_dir, ['checkout', 'FETCH_HEAD'], print_cmd=True)

  def ProcessProject(self, project):
    """Performs per-project push operations."""
    ls_remote = cros_build_lib.RunCommandCaptureOutput(
        ['git', 'ls-remote', project['remote_alias'],
        self.dest_ref],
        cwd=project['local_path']).output.strip()

    if self.rename_to and ls_remote:
      git.RunGit(project['local_path'], ['remote', 'update'])
      self.RunPush(project, src_ref=self.dest_ref, dest_ref=self.rename_to)

    if self._options.delete_branch or self.rename_to:
      if ls_remote:
        self.RunPush(project, src_ref='')
    elif ls_remote and not self._options.force_create:
      raise BranchError('Project %s already contains branch %s.  Run with '
                        '--force-create to overwrite.'
                        % (project['name'], self.dest_ref))
    else:
      self.RunPush(project, force=self._options.force_create)

  def FixUpManifests(self, manifest):
    """Points the projects at the new branch in the manifests.

    The 'master' branch manifest (full.xml) contains projects that are checked
    out to branches other than 'refs/heads/master'.  But in the new branch,
    these should all be checked out to 'refs/heads/<new_branch>', so we go
    through the manifest and fix those projects.

    Args:
      manifest: A git.Manifest object.
    """
    for project in ('chromiumos/manifest', 'chromeos/manifest-internal'):
      manifest_project = manifest.projects[project]
      manifest_path = manifest_project['local_path']
      push_remote = manifest_project['push_remote']

      git.CreateBranch(
          manifest_path, manifest_version.PUSH_BRANCH,
          branch_point=manifest_project['revision'])
      full_manifest = os.path.join(manifest_project['local_path'], 'full.xml')
      result = re.sub(r'\brevision="[^"]*"', 'revision="%s"' % self.dest_ref,
                    osutils.ReadFile(full_manifest))
      osutils.WriteFile(full_manifest, result)

      git.RunGit(manifest_path, ['add', '-A'], print_cmd=True)
      message = 'Fix up manifest after branching %s.' % self.dest_ref
      git.RunGit(manifest_path, ['commit', '-m', message], print_cmd=True)
      push_to = git.RemoteRef(push_remote, self.dest_ref)
      git.GitPush(manifest_path, manifest_version.PUSH_BRANCH, push_to,
                  dryrun=self.dryrun, force=self.dryrun)

  def IncrementVersion(self, incr_type, push_to, message):
    """Bumps the version found in chromeos_version.sh on a branch.

    Args:
      incr_type: See docstring for manifest_version.VersionInfo.
      push_to: A git.RemoteRef object.
      message: The message to give the git commit that bumps the version.
    """
    version_info = manifest_version.VersionInfo.from_repo(
        self._build_root, incr_type=incr_type)
    version_info.IncrementVersion(message, dry_run=self.dryrun,
                                  push_to=push_to)

  @staticmethod
  def DetermineBranchIncrParams(version_info):
    """Determines the version component to bump for the new branch."""
    # We increment the left-most component that is zero.
    if version_info.branch_build_number != '0':
      if version_info.patch_number != '0':
        raise BranchError('Version %s cannot be branched.' %
                          version_info.VersionString())
      return 'patch', 'patch number'
    else:
      return 'branch', 'branch number'

  @staticmethod
  def DetermineSourceIncrParams(source_name, dest_name):
    """Determines the version component to bump for the original branch."""
    if dest_name.startswith('refs/heads/release-'):
      return 'chrome_branch', 'Chrome version'
    elif source_name == 'refs/heads/master':
      return 'build', 'build number'
    else:
      return 'branch', 'branch build number'

  def IncrementVersionForNewBranch(self, push_remote):
    """Bumps the version found in chromeos_version.sh on the new branch

    When a new branch is created, the branch component of the new branch's
    version needs to bumped.

    For example, say 'stabilize-link' is created from a the 4230.0.0 manifest.
    The new branch's version needs to be bumped to 4230.1.0.

    Args:
      push_remote: a git.RemoteRef identifying the new branch.
    """
    # This needs to happen before the source branch version bumping above
    # because we rely on the fact that since our current overlay checkout
    # is what we just pushed to the new branch, we don't need to do another
    # sync.  This also makes it easier to implement dryrun functionality (the
    # new branch doesn't actually get created in dryrun mode).
    push_to = git.RemoteRef(push_remote, self.dest_ref)
    version_info = manifest_version.VersionInfo(
        version_string=self._options.force_version)
    incr_type, incr_target = self.DetermineBranchIncrParams(version_info)
    message = self.COMMIT_MESSAGE % {
        'target': incr_target,
        'branch': self.dest_ref,
    }
    self.IncrementVersion(incr_type, push_to, message)

  def IncrementVersionForSourceBranch(self, overlay_dir, push_remote,
                                      source_branch):
    """Bumps the version found in chromeos_version.sh on the source branch

    The source branch refers to the branch that the manifest used for creating
    the new branch came from.  For release branches, we generally branch from a
    'master' branch manifest.

    To work around crbug.com/213075, for both non-release and release branches,
    we need to bump the Chrome OS version on the source branch if the manifest
    used for branch creation is the latest generated manifest for the source
    branch.

    When we are creating a release branch, the Chrome major version of the
    'master' (source) branch needs to be bumped.  For example, if we branch
    'release-R29-4230.B' from the 4230.0.0 manifest (which is from the 'master'
    branch), the 'master' branch's Chrome major version in chromeos_version.sh
    (which is 29) needs to be bumped to 30.

    Args:
      overlay_dir: Absolute path to the chromiumos overlay repo.
      push_remote: The remote to push to.
      source_branch: The branch that the manifest we are using comes from.
    """
    push_to = git.RemoteRef(push_remote, source_branch)
    self.FetchAndCheckoutTo(overlay_dir, push_to)

    tot_version_info = manifest_version.VersionInfo.from_repo(self._build_root)
    if (self.dest_ref.startswith('refs/heads/release-') or
        tot_version_info.VersionString() == self._options.force_version):
      incr_type, incr_target = self.DetermineSourceIncrParams(
          source_branch, self.dest_ref)
      message = self.COMMIT_MESSAGE % {
          'target': incr_target,
          'branch': self.dest_ref,
      }
      try:
        self.IncrementVersion(incr_type, push_to, message)
      except cros_build_lib.RunCommandError:
        # There's a chance we are racing against the buildbots for this
        # increment.  We shouldn't quit the script because of this.  Instead, we
        # print a warning.
        self.FetchAndCheckoutTo(overlay_dir, push_to)
        new_version =  manifest_version.VersionInfo.from_repo(self._build_root)
        if new_version.VersionString() != tot_version_info.VersionString():
          logging.warning('Version number for branch %s was bumped by another '
                          'bot.', push_to.ref)
        else:
          raise

  def PerformStage(self):
    """Run the branch operation."""
    def TestPushable(project):
      return project['pushable']

    # Setup and initialize the repo.
    super(BranchUtilStage, self).PerformStage()

    manifest = git.ManifestCheckout.Cached(self._build_root)
    # Project tuples are in the form (project_name, project_dict).
    projects = [proj_tuple[1]
                for proj_tuple in sorted(manifest.projects.iteritems())]
    pushable, skipped = cros_build_lib.PredicateSplit(TestPushable, projects)
    for p in skipped:
      logging.warning('Skipping project %s.', p['name'])

    parallel.RunTasksInProcessPool(
        self.ProcessProject, [[p] for p in pushable], processes=4)

    if self._options.delete_branch or self.rename_to:
      return

    self.FixUpManifests(manifest)

    overlay_name = 'chromiumos/overlays/chromiumos-overlay'
    overlay_project = manifest.projects[overlay_name]
    overlay_dir = overlay_project['local_path']
    push_remote = overlay_project['push_remote']
    self.IncrementVersionForNewBranch(push_remote)

    source_branch = manifest.default['revision']
    self.IncrementVersionForSourceBranch(overlay_dir, push_remote,
                                         source_branch)


class RefreshPackageStatusStage(bs.BuilderStage):
  """Stage for refreshing Portage package status in online spreadsheet."""
  def PerformStage(self):
    commands.RefreshPackageStatus(buildroot=self._build_root,
                                  boards=self._boards,
                                  debug=self._options.debug)


class InitSDKStage(bs.BuilderStage):
  """Stage that is responsible for initializing the SDK."""

  option_name = 'build'

  def __init__(self, options, build_config):
    super(InitSDKStage, self).__init__(options, build_config)
    self._env = {}
    if self._options.clobber:
      self._env['IGNORE_PREFLIGHT_BINHOST'] = '1'

    self._latest_toolchain = (self._build_config['latest_toolchain'] or
                              self._options.latest_toolchain)
    if self._latest_toolchain and self._build_config['gcc_githash']:
      self._env['USE'] = 'git_gcc'
      self._env['GCC_GITHASH'] = self._build_config['gcc_githash']

  def PerformStage(self):
    chroot_path = os.path.join(self._build_root, constants.DEFAULT_CHROOT_DIR)
    replace = self._build_config['chroot_replace']
    if os.path.isdir(self._build_root) and not replace:
      try:
        commands.RunChrootUpgradeHooks(self._build_root)
      except results_lib.BuildScriptFailure:
        cros_build_lib.PrintBuildbotStepText('Replacing broken chroot')
        cros_build_lib.PrintBuildbotStepWarnings()
        replace = True

    if not os.path.isdir(chroot_path) or replace:
      use_sdk = (self._build_config['use_sdk'] and not self._options.nosdk)
      commands.MakeChroot(
          buildroot=self._build_root,
          replace=replace,
          use_sdk=use_sdk,
          chrome_root=self._options.chrome_root,
          extra_env=self._env)


class SetupBoardStage(InitSDKStage):
  """Stage that is responsible for building host pkgs and setting up a board."""

  option_name = 'build'

  def __init__(self, options, build_config, boards=None):
    super(SetupBoardStage, self).__init__(options, build_config)
    if boards is not None:
      self._boards = boards

  def PerformStage(self):
    # Calculate whether we should use binary packages.
    usepkg = (self._build_config['usepkg_setup_board'] and
              not self._latest_toolchain)

    # We need to run chroot updates on most builders because they uprev after
    # the InitSDK stage. For the SDK builder, we can skip updates because uprev
    # is run prior to InitSDK. This is not just an optimization: It helps
    # workaround http://crbug.com/225509
    chroot_upgrade = (
      self._build_config['build_type'] != constants.CHROOT_BUILDER_TYPE)

    # Iterate through boards to setup.
    chroot_path = os.path.join(self._build_root, constants.DEFAULT_CHROOT_DIR)
    for board_to_build in self._boards:
      # Only update the board if we need to do so.
      board_path = os.path.join(chroot_path, 'build', board_to_build)
      if os.path.isdir(board_path) and not chroot_upgrade:
        continue

      commands.SetupBoard(
          self._build_root, board=board_to_build, usepkg=usepkg,
          chrome_binhost_only=self._build_config['chrome_binhost_only'],
          force=self._build_config['board_replace'],
          extra_env=self._env, chroot_upgrade=chroot_upgrade,
          profile=self._options.profile or self._build_config['profile'])
      chroot_upgrade = False

    commands.SetSharedUserPassword(
        self._build_root,
        password=self._build_config['shared_user_password'])


class UprevStage(bs.BuilderStage):
  """Stage that uprevs Chromium OS packages that the builder intends to
  validate.
  """

  option_name = 'uprev'

  def __init__(self, options, build_config, boards=None, enter_chroot=True):
    super(UprevStage, self).__init__(options, build_config)
    self._enter_chroot = enter_chroot
    if boards is not None:
      self._boards = boards

  def PerformStage(self):
    # Perform other uprevs.
    if self._build_config['uprev']:
      overlays, _ = self._ExtractOverlays()
      commands.UprevPackages(self._build_root,
                             self._boards,
                             overlays,
                             enter_chroot=self._enter_chroot)


class SyncChromeStage(bs.BuilderStage):
  """Stage that syncs Chrome sources if needed."""

  option_name = 'managed_chrome'

  def __init__(self, options, build_config):
    super(SyncChromeStage, self).__init__(options, build_config)
    # PerformStage() will fill this out for us.
    self.chrome_version = None

  def PerformStage(self):
    # Perform chrome uprev.
    chrome_atom_to_build = None
    if self._chrome_rev:
      chrome_atom_to_build = commands.MarkChromeAsStable(
          self._build_root, self._target_manifest_branch,
          self._chrome_rev, self._boards,
          chrome_version=self._options.chrome_version)

    kwargs = {}
    if self._chrome_rev == constants.CHROME_REV_SPEC:
      kwargs['revision'] = self._options.chrome_version
      cpv = None
      cros_build_lib.PrintBuildbotStepText('revision %s' % kwargs['revision'])
      self.chrome_version = self._options.chrome_version
    else:
      cpv = portage_utilities.BestVisible(constants.CHROME_CP,
                                          buildroot=self._build_root)
      kwargs['tag'] = cpv.version_no_rev.partition('_')[0]
      cros_build_lib.PrintBuildbotStepText('tag %s' % kwargs['tag'])
      self.chrome_version = kwargs['tag']

    useflags = self._build_config['useflags'] or []
    commands.SyncChrome(self._build_root, self._options.chrome_root, useflags,
                        **kwargs)
    if (self._chrome_rev and not chrome_atom_to_build and
        self._options.buildbot and
        self._build_config['build_type'] == constants.CHROME_PFQ_TYPE):
      cros_build_lib.Info('Chrome already uprevved')
      sys.exit(0)


class PatchChromeStage(bs.BuilderStage):
  """Stage that applies Chrome patches if needed."""

  option_name = 'rietveld_patches'

  def PerformStage(self):
    for patch in ' '.join(self._options.rietveld_patches).split():
      patch, colon, subdir = patch.partition(':')
      if not colon:
        subdir = 'src'
      commands.PatchChrome(self._options.chrome_root, patch, subdir)


class BuildPackagesStage(ArchivingStage):
  """Build Chromium OS packages."""

  option_name = 'build'
  def __init__(self, options, build_config, board, archive_stage,
               pgo_generate=False, pgo_use=False):
    useflags = build_config['useflags'][:] if build_config['useflags'] else []
    self._pgo_generate, self._pgo_use = pgo_generate, pgo_use
    suffix = None
    assert not pgo_generate or not pgo_use
    if pgo_generate:
      suffix = ' [%s]' % constants.USE_PGO_GENERATE
      useflags.append(constants.USE_PGO_GENERATE)
    elif pgo_use:
      suffix = ' [%s]' % constants.USE_PGO_USE
      useflags.append(constants.USE_PGO_USE)
    super(BuildPackagesStage, self).__init__(options, build_config, board,
                                             archive_stage, suffix=suffix)

    self._env = {}
    if useflags:
      self._env['USE'] = ' '.join(useflags)

    if self._options.chrome_root:
      self._env['CHROME_ORIGIN'] = 'LOCAL_SOURCE'

    if self._options.clobber:
      self._env['IGNORE_PREFLIGHT_BINHOST'] = '1'

    self._build_autotest = (self._build_config['build_tests'] and
                            self._options.tests)

  def _GetArchitectures(self):
    """Get the list of architectures built by this builder."""
    return set(self._GetPortageEnvVar('ARCH', b) for b in self._boards)

  def PerformStage(self):
    # Wait for PGO data to be ready if needed.
    if self._pgo_use:
      cpv = portage_utilities.BestVisible(constants.CHROME_CP,
                                          buildroot=self._build_root)
      commands.WaitForPGOData(self._GetArchitectures(), cpv)

    config = self._build_config
    commands.Build(self._build_root,
                   self._current_board,
                   build_autotest=self._build_autotest,
                   usepkg=config['usepkg_build_packages'],
                   chrome_binhost_only=config['chrome_binhost_only'],
                   packages=config['packages'],
                   skip_chroot_upgrade=True,
                   chrome_root=self._options.chrome_root,
                   extra_env=self._env)


class ChromeSDKStage(ArchivingStage):
  """Run through the simple chrome workflow."""

  def __init__(self, *args, **kwargs):
    super(ChromeSDKStage, self).__init__(*args, **kwargs)
    self._upload_queue = multiprocessing.Queue()
    self._pkg_dir = os.path.join(
        self._build_root, constants.DEFAULT_CHROOT_DIR,
        'build', self._current_board, 'var', 'db', 'pkg')
    self.chrome_src = os.path.join(self._options.chrome_root, 'src')
    self.out_board_dir = os.path.join(
        self.chrome_src, 'out_%s' % self._current_board)

  def _BuildAndArchiveChromeSysroot(self):
    """Generate and upload sysroot for building Chrome."""
    assert self.archive_path.startswith(self._build_root)
    extra_env = {}
    if self._build_config['useflags']:
      extra_env['USE'] = ' '.join(self._build_config['useflags'])
    in_chroot_path = git.ReinterpretPathForChroot(self.archive_path)
    cmd = ['cros_generate_sysroot', '--out-dir', in_chroot_path, '--board',
           self._current_board, '--package', constants.CHROME_CP]
    cros_build_lib.RunCommand(cmd, cwd=self._build_root, enter_chroot=True,
                              extra_env=extra_env)
    self._upload_queue.put([constants.CHROME_SYSROOT_TAR])

  def _ArchiveChromeEbuildEnv(self):
    """Generate and upload Chrome ebuild environment."""
    chrome_dir = self.archive_stage.SingleMatchGlob(
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

  def PerformStage(self):
    if platform.dist()[-1] == 'lucid':
      # Chrome no longer builds on Lucid. See crbug.com/276311
      print 'Ubuntu lucid is no longer supported.'
      print 'Please upgrade to Ubuntu Precise.'
      cros_build_lib.PrintBuildbotStepWarnings()
      return

    upload_metadata = functools.partial(
        self.UploadMetadata, upload_queue=self._upload_queue)
    steps = [self._BuildAndArchiveChromeSysroot, self._ArchiveChromeEbuildEnv,
             upload_metadata]
    with self.ArtifactUploader(self._upload_queue, archive=False):
      parallel.RunParallelSteps(steps)

      with osutils.TempDir(prefix='chrome-sdk-cache') as tempdir:
        cache_dir = os.path.join(tempdir, 'cache')
        extra_args = ['--cwd', self.chrome_src, '--sdk-path', self.archive_path]
        sdk_cmd = commands.ChromeSDK(
            self._build_root, self._current_board, chrome_src=self.chrome_src,
            goma=self._build_config['chrome_sdk_goma'],
            extra_args=extra_args, cache_dir=cache_dir)
        self._BuildChrome(sdk_cmd)
        self._TestDeploy(sdk_cmd)


class BuildImageStage(BuildPackagesStage):
  """Build standard Chromium OS images."""

  option_name = 'build'

  def _BuildImages(self):
    # We only build base, dev, and test images from this stage.
    if self._pgo_generate:
      images_can_build = set(['test'])
    else:
      images_can_build = set(['base', 'dev', 'test'])
    images_to_build = set(self._build_config['images']).intersection(
        images_can_build)

    version = self.archive_stage.release_tag
    disk_layout = self._build_config['disk_layout']
    if self._pgo_generate:
      disk_layout = constants.PGO_GENERATE_DISK_LAYOUT
      if version:
        version = '%s-pgo-generate' % version

    rootfs_verification = self._build_config['rootfs_verification']
    commands.BuildImage(self._build_root,
                        self._current_board,
                        sorted(images_to_build),
                        rootfs_verification=rootfs_verification,
                        version=version,
                        disk_layout=disk_layout,
                        extra_env=self._env)

    # Update link to latest image.
    latest_image = os.readlink(self.GetImageDirSymlink('latest'))
    cbuildbot_image_link = self.GetImageDirSymlink()
    if os.path.lexists(cbuildbot_image_link):
      os.remove(cbuildbot_image_link)

    os.symlink(latest_image, cbuildbot_image_link)

    parallel.RunParallelSteps(
        [self._BuildVMImage, self.ArchivePayloads,
         lambda: self._GenerateAuZip(cbuildbot_image_link)])

  def _BuildVMImage(self):
    if self._build_config['vm_tests'] and not self._pgo_generate:
      commands.BuildVMImageForTesting(
          self._build_root,
          self._current_board,
          disk_layout=self._build_config['disk_vm_layout'],
          extra_env=self._env)

  def ArchivePayloads(self):
    """Archives update payloads when they are ready."""
    with osutils.TempDir(prefix='cbuildbot-payloads') as tempdir:
      with self.ArtifactUploader() as queue:
        if self._build_config['upload_hw_test_artifacts']:
          image_path = os.path.join(self.GetImageDirSymlink(),
                                    'chromiumos_test_image.bin')
          # For non release builds, we are only interested in generating
          # payloads for the purpose of imaging machines. This means we
          # shouldn't generate delta payloads for n-1->n testing.
          # TODO: Add a config flag for generating delta payloads instead.
          if (self._build_config['build_type'] == constants.CANARY_TYPE and
              not self._pgo_generate):
            commands.GenerateNPlus1Payloads(
                self._build_root, self.bot_archive_root, image_path, tempdir)
          else:
            commands.GenerateFullPayload(self._build_root, image_path, tempdir)

          for payload in os.listdir(tempdir):
            queue.put([os.path.join(tempdir, payload)])

  def _GenerateAuZip(self, image_dir):
    """Create au-generator.zip."""
    if not self._pgo_generate:
      commands.GenerateAuZip(self._build_root,
                             image_dir,
                             extra_env=self._env)

  def _BuildAutotestTarballs(self):
    with osutils.TempDir(prefix='cbuildbot-autotest') as tempdir:
      with self.ArtifactUploader() as queue:
        cwd = os.path.join(self._build_root, 'chroot', 'build',
                           self._current_board, 'usr', 'local')

        # Find the control files in autotest/
        control_files = commands.FindFilesWithPattern(
            'control*', target='autotest', cwd=cwd)

        # Tar the control files and the packages.
        autotest_tarball = os.path.join(tempdir, 'autotest.tar')
        input_list = control_files + ['autotest/packages']
        commands.BuildTarball(self._build_root, input_list, autotest_tarball,
                              cwd=cwd, compressed=False)
        queue.put([autotest_tarball])

        # Tar up the test suites.
        test_suites_tarball = os.path.join(tempdir, 'test_suites.tar.bz2')
        commands.BuildTarball(self._build_root, ['autotest/test_suites'],
                              test_suites_tarball, cwd=cwd)
        queue.put([test_suites_tarball])

  def PerformStage(self):
    # Build images and autotest tarball in parallel.
    steps = []
    if (self._build_config['upload_hw_test_artifacts'] or
        self._build_config['archive_build_debug']) and self._build_autotest:
      steps.append(self._BuildAutotestTarballs)

    if self._build_config['images']:
      steps.append(self._BuildImages)

    parallel.RunParallelSteps(steps)


class SignerTestStage(ArchivingStage):
  """Run signer related tests."""

  option_name = 'tests'
  config_name = 'signer_tests'

  # If the signer tests take longer than 30 minutes, abort. They usually take
  # five minutes to run.
  SIGNER_TEST_TIMEOUT = 1800

  def PerformStage(self):
    if not self.archive_stage.WaitForRecoveryImage():
      raise InvalidTestConditionException('Missing recovery image.')
    with cros_build_lib.SubCommandTimeout(self.SIGNER_TEST_TIMEOUT):
      commands.RunSignerTests(self._build_root, self._current_board)


class UnitTestStage(BoardSpecificBuilderStage):
  """Run unit tests."""

  option_name = 'tests'
  config_name = 'unittests'

  # If the unit tests take longer than 70 minutes, abort. They usually take
  # ten minutes to run.
  #
  # If the processes hang, parallel_emerge will print a status report after 60
  # minutes, so we picked 70 minutes because it gives us a little buffer time.
  UNIT_TEST_TIMEOUT = 70 * 60

  def PerformStage(self):
    extra_env = {}
    if self._build_config['useflags']:
      extra_env['USE'] = ' '.join(self._build_config['useflags'])
    with cros_build_lib.SubCommandTimeout(self.UNIT_TEST_TIMEOUT):
      commands.RunUnitTests(self._build_root,
                            self._current_board,
                            full=(not self._build_config['quick_unit']),
                            blacklist=self._build_config['unittest_blacklist'],
                            extra_env=extra_env)

    if os.path.exists(os.path.join(self.GetImageDirSymlink(),
                                   'au-generator.zip')):
      commands.TestAuZip(self._build_root,
                         self.GetImageDirSymlink())


class VMTestStage(ArchivingStage):
  """Run autotests in a virtual machine."""

  option_name = 'tests'
  config_name = 'vm_tests'

  def _ArchiveTestResults(self, test_results_dir):
    """Archives test results to Google Storage."""
    test_tarball = commands.ArchiveTestResults(
        self._build_root, test_results_dir, prefix='')

    # Wait for breakpad symbols.
    got_symbols = self.archive_stage.WaitForBreakpadSymbols()
    filenames = commands.GenerateStackTraces(
        self._build_root, self._current_board, test_tarball, self.archive_path,
        got_symbols)
    filenames.append(commands.ArchiveFile(test_tarball, self.archive_path))

    cros_build_lib.Info('Uploading artifacts to Google Storage...')
    with self.ArtifactUploader(archive=False, strict=False) as queue:
      for filename in filenames:
        queue.put([filename])
        prefix = 'crash: ' if filename.endswith('.dmp.txt') else ''
        self.PrintDownloadLink(filename, prefix)

  def PerformStage(self):
    # These directories are used later to archive test artifacts.
    test_results_dir = commands.CreateTestRoot(self._build_root)
    try:
      test_type = self._build_config['vm_tests']
      commands.RunTestSuite(self._build_root,
                            self._current_board,
                            self.GetImageDirSymlink(),
                            os.path.join(test_results_dir,
                                         'test_harness'),
                            test_type=test_type,
                            whitelist_chrome_crashes=self._chrome_rev is None,
                            archive_dir=self.bot_archive_root)

      if self._build_config['build_type'] == constants.CANARY_TYPE:
        commands.RunDevModeTest(
            self._build_root, self._current_board, self.GetImageDirSymlink())

    except Exception:
      cros_build_lib.Error(_VM_TEST_ERROR_MSG)
      raise
    finally:
      self._ArchiveTestResults(test_results_dir)


class TestTimeoutException(Exception):
  """Raised when a critical test times out."""
  pass


class InvalidTestConditionException(Exception):
  """Raised when pre-conditions for a test aren't met."""
  pass


class HWTestStage(ArchivingStage):
  """Stage that runs tests in the Autotest lab."""

  option_name = 'tests'
  config_name = 'hw_tests'

  PERF_RESULTS_EXTENSION = 'results'

  def __init__(self, options, build_config, board, archive_stage, suite_config):
    super(HWTestStage, self).__init__(options, build_config, board,
                                      archive_stage,
                                      suffix=' [%s]' % suite_config.suite)
    self.suite_config = suite_config
    self.wait_for_results = True

  def _PrintFile(self, filename):
    with open(filename) as f:
      print f.read()

  def _SendPerfResults(self):
    """Sends the perf results from the test to the perf dashboard."""
    result_file_name = '%s.%s' % (self.suite_config.suite,
                                  HWTestStage.PERF_RESULTS_EXTENSION)
    gs_results_file = '/'.join([self.upload_url, result_file_name])
    gs_context = gs.GSContext()
    gs_context.Copy(gs_results_file, self._options.log_dir)
    # Prints out the actual result from gs_context.Copy.
    logging.info('Copy of %s completed. Printing below:', result_file_name)
    self._PrintFile(os.path.join(self._options.log_dir, result_file_name))

  def _CheckAborted(self):
    aborted = (self.archive_stage.release_tag and
               commands.HaveHWTestsBeenAborted(self.archive_stage.release_tag))
    if aborted:
      cros_build_lib.PrintBuildbotStepText('aborted')
      cros_build_lib.Warning('HWTests aborted')
    return aborted

  # Disable complaint about calling _HandleStageException.
  # pylint: disable=W0212
  def _HandleStageException(self, exception):
    """Override and don't set status to FAIL but FORGIVEN instead."""

    # Deal with timeout errors specially.
    if isinstance(exception, cros_build_lib.TimeoutError):
      return self._HandleStageTimeoutException(exception)

    # 2 for warnings returned by run_suite.py, or CLIENT_HTTP_CODE error
    # returned by autotest_rpc_client.py. It is the former that we care about.
    # 11, 12, 13 for cases when rpc is down, see autotest_rpc_errors.py.
    codes_handled_as_warning = (2, 11, 12, 13)

    if self.suite_config.critical:
      return super(HWTestStage, self)._HandleStageException(exception)
    is_lab_down = (isinstance(exception, lab_status.LabIsDownException) or
                   isinstance(exception, lab_status.BoardIsDisabledException))
    is_warning_code = (isinstance(exception, cros_build_lib.RunCommandError) and
                       exception.result.returncode in codes_handled_as_warning)
    if is_lab_down or is_warning_code or self._CheckAborted():
      return self._HandleExceptionAsWarning(exception)
    else:
      return super(HWTestStage, self)._HandleStageException(exception)

  def _HandleStageTimeoutException(self, exception):
    if not self.suite_config.critical and not self.suite_config.fatal_timeouts:
      return self._HandleExceptionAsWarning(exception)

    return super(HWTestStage, self)._HandleStageException(exception)

  def PerformStage(self):
    if self._CheckAborted():
      cros_build_lib.Info('Skipping HWTests as they have been aborted.')
      return

    build = '/'.join([self._bot_id, self.version])
    if self._options.remote_trybot and self._options.hwtest:
      debug = self._options.debug_forced
    else:
      debug = self._options.debug
    lab_status.CheckLabStatus(self._current_board)
    with cros_build_lib.SubCommandTimeout(self.suite_config.timeout):
      commands.RunHWTestSuite(build,
                              self.suite_config.suite,
                              self._current_board,
                              self.suite_config.pool,
                              self.suite_config.num,
                              self.suite_config.file_bugs,
                              self.wait_for_results,
                              debug)

      if self.suite_config.copy_perf_results:
        self._SendPerfResults()


class AUTestStage(HWTestStage):
  """Stage for au hw test suites that requires special pre-processing."""

  def PerformStage(self):
    """Wait for payloads to be staged and uploads its au control files."""
    with osutils.TempDir() as tempdir:
      tarball = commands.BuildAUTestTarball(
          self._build_root, self._current_board, tempdir,
          self.version, self.upload_url)
      self.UploadArtifact(tarball)

    super(AUTestStage, self).PerformStage()


class ASyncHWTestStage(HWTestStage, ForgivingBuilderStage):
  """Stage that fires and forgets hw test suites to the Autotest lab."""

  def __init__(self, *args, **dargs):
    super(ASyncHWTestStage, self).__init__(*args, **dargs)
    self.wait_for_results = False


class SDKPackageStage(bs.BuilderStage):
  """Stage that performs preparing and packaging SDK files"""

  # Version of the Manifest file being generated. Should be incremented for
  # Major format changes.
  MANIFEST_VERSION = '1'
  _EXCLUDED_PATHS = ('usr/lib/debug', 'usr/local/autotest', 'packages', 'tmp')

  def PerformStage(self):
    tarball_name = 'built-sdk.tar.xz'
    tarball_location = os.path.join(self._build_root, tarball_name)
    chroot_location = os.path.join(self._build_root,
                                   constants.DEFAULT_CHROOT_DIR)
    board_location = os.path.join(chroot_location, 'build/amd64-host')
    manifest_location = os.path.join(self._build_root,
                                     '%s.Manifest' % tarball_name)

    # Create a tarball of the latest SDK.
    self.CreateSDKTarball(chroot_location, board_location, tarball_location)

    # Create a package manifest for the tarball.
    self.CreateManifestFromSDK(board_location, manifest_location)

    # Create toolchain packages.
    self.CreateRedistributableToolchains(chroot_location)

    # Make sure the regular user has the permission to read.
    cmd = ['chmod', 'a+r', tarball_location]
    cros_build_lib.SudoRunCommand(cmd, cwd=board_location)

  def CreateRedistributableToolchains(self, chroot_location):
    osutils.RmDir(os.path.join(chroot_location,
                               constants.SDK_TOOLCHAINS_OUTPUT),
                  ignore_missing=True)
    cros_build_lib.RunCommand(
        ['cros_setup_toolchains', '--create-packages',
         '--output-dir', os.path.join('/', constants.SDK_TOOLCHAINS_OUTPUT)],
        enter_chroot=True)

  def CreateSDKTarball(self, _chroot, sdk_path, dest_tarball):
    """Creates an SDK tarball from a given source chroot.

    Args:
      chroot: A chroot used for finding compression tool.
      sdk_path: Path to the root of newly generated SDK image.
      dest_tarball: Path of the tarball that should be created.
    """
    # TODO(zbehan): We cannot use xz from the chroot unless it's
    # statically linked.
    extra_args = ['--exclude=%s/*' % path for path in self._EXCLUDED_PATHS]
    # Options for maximum compression.
    extra_env = { 'XZ_OPT' : '-e9' }
    cros_build_lib.CreateTarball(
        dest_tarball, sdk_path, sudo=True, extra_args=extra_args,
        extra_env=extra_env)

  def CreateManifestFromSDK(self, sdk_path, dest_manifest):
    """Creates a manifest from a given source chroot.

    Args:
      sdk_path: Path to the root of the SDK to describe.
      dest_manifest: Path to the manifest that should be generated.
    """
    package_data = {}
    for key, version in portage_utilities.ListInstalledPackages(sdk_path):
      package_data.setdefault(key, []).append((version, {}))
    self._WriteManifest(package_data, dest_manifest)

  def _WriteManifest(self, data, manifest):
    """Encode manifest into a json file."""
    json_input = dict(version=self.MANIFEST_VERSION, packages=data)
    osutils.WriteFile(manifest, json.dumps(json_input))


class SDKTestStage(bs.BuilderStage):
  """Stage that performs testing an SDK created in a previous stage"""

  option_name = 'tests'

  def PerformStage(self):
    tarball_location = os.path.join(self._build_root, 'built-sdk.tar.xz')
    new_chroot_cmd = ['cros_sdk', '--chroot', 'new-sdk-chroot']
    # Build a new SDK using the provided tarball.
    cmd = new_chroot_cmd + ['--download', '--replace', '--nousepkg',
        '--url', 'file://' + tarball_location]
    cros_build_lib.RunCommand(cmd, cwd=self._build_root)

    for board in self._boards:
      cros_build_lib.PrintBuildbotStepText(board)
      cmd = new_chroot_cmd + ['--', './setup_board',
          '--board', board, '--skip_chroot_upgrade']
      cros_build_lib.RunCommand(cmd, cwd=self._build_root)
      cmd = new_chroot_cmd + ['--', './build_packages',
          '--board', board, '--nousepkg', '--skip_chroot_upgrade']
      cros_build_lib.RunCommand(cmd, cwd=self._build_root)


class NothingToArchiveException(Exception):
  """Thrown if ArchiveStage found nothing to archive."""
  def __init__(self, message='No images found to archive.'):
    super(NothingToArchiveException, self).__init__(message)


class ArchiveStage(ArchivingStage):
  """Archives build and test artifacts for developer consumption.

  Attributes:
    release_tag: The release tag. E.g. 2981.0.0
    version: The full version string, including the milestone.
        E.g. R26-2981.0.0-b123
  """

  option_name = 'archive'

  # This stage is intended to run in the background, in parallel with tests.
  def __init__(self, options, build_config, board, release_tag,
               chrome_version=None):
    self.release_tag = release_tag
    self.chrome_version = chrome_version
    super(ArchiveStage, self).__init__(options, build_config, board, self)

    self._breakpad_symbols_queue = multiprocessing.Queue()
    self._recovery_image_status_queue = multiprocessing.Queue()
    self._release_upload_queue = multiprocessing.Queue()
    self._upload_queue = multiprocessing.Queue()
    self._upload_symbols_queue = multiprocessing.Queue()

    # Setup the archive path. This is used by other stages.
    self._SetupArchivePath()

  @cros_build_lib.MemoizedSingleCall
  def GetVersionInfo(self):
    """Helper for picking apart various version bits"""
    return manifest_version.VersionInfo.from_repo(self._build_root)

  @cros_build_lib.MemoizedSingleCall
  def GetVersion(self):
    """Helper for calculating self.version."""
    verinfo = self.GetVersionInfo()
    calc_version = self.release_tag or verinfo.VersionString()
    calc_version = 'R%s-%s' % (verinfo.chrome_branch, calc_version)

    # Non-versioned builds need the build number to uniquify the image.
    if not self.release_tag:
      calc_version += '-b%s' % self._options.buildnumber

    return calc_version

  def WaitForRecoveryImage(self):
    """Wait until artifacts needed by SignerTest stage are created.

    Returns:
      True if artifacts created successfully.
      False otherwise.
    """
    cros_build_lib.Info('Waiting for recovery image...')
    status = self._recovery_image_status_queue.get()
    # Put the status back so other SignerTestStage instances don't starve.
    self._recovery_image_status_queue.put(status)
    return status

  def _BreakpadSymbolsGenerated(self, success):
    """Signal that breakpad symbols have been generated.

    Arguments:
      success: True to indicate the symbols were generated, else False.
    """
    self._breakpad_symbols_queue.put(success)

  def WaitForBreakpadSymbols(self):
    """Wait for the breakpad symbols to be generated.

    Returns:
      True if the breakpad symbols were generated.
      False if the breakpad symbols were not generated within 20 mins.
    """
    success = False
    cros_build_lib.Info('Waiting for breakpad symbols...')
    try:
      # TODO: Clean this up so that we no longer rely on a timeout
      success = self._breakpad_symbols_queue.get(True, 1200)
    except Queue.Empty:
      cros_build_lib.Warning(
          'Breakpad symbols were not generated within timeout period.')
    return success

  def _SetupArchivePath(self):
    """Create a fresh directory for archiving a build."""
    if self._options.buildbot:
      # Buildbot: Clear out any leftover build artifacts, if present.
      shutil.rmtree(self.archive_path, ignore_errors=True)
    else:
      # Clear the list of uploaded file if it exists
      osutils.SafeUnlink(os.path.join(self.archive_path,
                                      commands.UPLOADED_LIST_FILENAME))

    osutils.SafeMakedirs(self.archive_path)

  @staticmethod
  def SingleMatchGlob(path_pattern):
    """Returns the last match (after sort) if multiple found."""
    files = glob.glob(path_pattern)
    files.sort()
    if not files:
      raise NothingToArchiveException('No %s found!' % path_pattern)
    elif len(files) > 1:
      cros_build_lib.PrintBuildbotStepWarnings()
      cros_build_lib.Warning('Expecting one result for %s package, but '
                             'found multiple.', path_pattern)
    return files[-1]

  def ArchiveStrippedChrome(self):
    """Generate and upload stripped Chrome package."""
    cmd = ['strip_package', '--board', self._current_board,
           constants.CHROME_PN]
    cros_build_lib.RunCommand(cmd, cwd=self._build_root, enter_chroot=True)
    pkg_dir = os.path.join(
        self._build_root, constants.DEFAULT_CHROOT_DIR, 'build',
        self._current_board, 'stripped-packages')
    chrome_tarball = self.SingleMatchGlob(
        os.path.join(pkg_dir, constants.CHROME_CP) + '-*')
    filename = os.path.basename(chrome_tarball)
    os.link(chrome_tarball, os.path.join(self.archive_path, filename))
    self._upload_queue.put([filename])

  def BuildAndArchiveDeltaSysroot(self):
    """Generate and upload delta sysroot for initial build_packages."""
    extra_env = {}
    if self._build_config['useflags']:
      extra_env['USE'] = ' '.join(self._build_config['useflags'])
    in_chroot_path = git.ReinterpretPathForChroot(self.archive_path)
    cmd = ['generate_delta_sysroot', '--out-dir', in_chroot_path,
           '--board', self._current_board]
    if not self._build_config['build_tests'] or not self._options.tests:
      cmd.append('--skip-tests')
    cros_build_lib.RunCommand(cmd, cwd=self._build_root, enter_chroot=True,
                              extra_env=extra_env)
    self._upload_queue.put([constants.DELTA_SYSROOT_TAR])

  def PerformStage(self):
    buildroot = self._build_root
    config = self._build_config
    board = self._current_board
    debug = self.debug
    upload_url = self.upload_url
    archive_path = self.archive_path
    image_dir = self.GetImageDirSymlink()

    extra_env = {}
    if config['useflags']:
      extra_env['USE'] = ' '.join(config['useflags'])

    if not archive_path:
      raise NothingToArchiveException()

    # The following functions are run in parallel (except where indicated
    # otherwise)
    # \- BuildAndArchiveArtifacts
    #    \- ArchiveReleaseArtifacts
    #       \- ArchiveDebugSymbols
    #       \- ArchiveFirmwareImages
    #       \- BuildAndArchiveAllImages
    #          (builds recovery image first, then launches functions below)
    #          \- BuildAndArchiveFactoryImages
    #          \- ArchiveStandaloneTarballs
    #             \- ArchiveStandaloneTarball
    #          \- ArchiveZipFiles
    #          \- ArchiveHWQual
    #       \- PushImage (blocks on BuildAndArchiveAllImages)
    #    \- ArchiveStrippedChrome
    #    \- ArchiveImageScripts

    def ArchiveDebugSymbols():
      """Generate debug symbols and upload debug.tgz."""
      if config['archive_build_debug'] or config['vm_tests']:
        success = False
        try:
          commands.GenerateBreakpadSymbols(buildroot, board)
          success = True
        finally:
          self._BreakpadSymbolsGenerated(success)

        # Kick off the symbol upload process in the background.
        if config['upload_symbols']:
          self._upload_symbols_queue.put([])

        # Generate and upload tarball.
        filename = commands.GenerateDebugTarball(
            buildroot, board, archive_path, config['archive_build_debug'])
        self._release_upload_queue.put([filename])
      else:
        self._BreakpadSymbolsGenerated(False)

    def UploadSymbols():
      """Upload generated debug symbols."""
      if self._options.remote_trybot or self.debug:
        # For debug builds, limit ourselves to just uploading 1 symbol.
        # This way trybots and such still exercise this code.
        cnt = 1
        official = False
      else:
        cnt = None
        official = config['chromeos_official']
      commands.UploadSymbols(buildroot, board, official, cnt)

    def BuildAndArchiveFactoryImages():
      """Build and archive the factory zip file.

      The factory zip file consists of the factory test image and the factory
      install image. Both are built here.
      """

      # Build factory test image and create symlink to it.
      factory_test_symlink = None
      if 'factory_test' in config['images']:
        alias = commands.BuildFactoryTestImage(buildroot, board, extra_env)
        factory_test_symlink = self.GetImageDirSymlink(alias)

      # Build factory install image and create a symlink to it.
      factory_install_symlink = None
      if 'factory_install' in config['images']:
        alias = commands.BuildFactoryInstallImage(buildroot, board, extra_env)
        factory_install_symlink = self.GetImageDirSymlink(alias)
        if config['factory_install_netboot']:
          commands.MakeNetboot(buildroot, board, factory_install_symlink)

      # Build and upload factory zip.
      if factory_install_symlink and factory_test_symlink:
        image_root = os.path.dirname(factory_install_symlink)
        filename = commands.BuildFactoryZip(
            buildroot, board, archive_path, image_root)
        self._release_upload_queue.put([filename])

    def ArchiveStandaloneTarball(image_file):
      """Build and upload a single tarball."""
      self._release_upload_queue.put([commands.BuildStandaloneImageTarball(
          archive_path, image_file)])

    def ArchiveStandaloneTarballs():
      """Build and upload standalone tarballs for each image."""
      if config['upload_standalone_images']:
        inputs = []
        for image_file in glob.glob(os.path.join(image_dir, '*.bin')):
          inputs.append([image_file])
        parallel.RunTasksInProcessPool(ArchiveStandaloneTarball, inputs)

    def ArchiveZipFiles():
      """Build and archive zip files.

      This includes:
        - image.zip (all images in one big zip file)
        - the au-generator.zip used for update payload generation.
      """
      # Zip up everything in the image directory.
      image_zip = commands.BuildImageZip(archive_path, image_dir)
      self._release_upload_queue.put([image_zip])

      # Archive au-generator.zip.
      filename = 'au-generator.zip'
      shutil.copy(os.path.join(image_dir, filename), archive_path)
      self._release_upload_queue.put([filename])

    def ArchiveHWQual():
      """Build and archive the HWQual images."""
      # TODO(petermayo): This logic needs to be exported from the BuildTargets
      # stage rather than copied/re-evaluated here.
      autotest_built = config['build_tests'] and self._options.tests and (
          config['upload_hw_test_artifacts'] or config['archive_build_debug'])

      if config['hwqual'] and autotest_built:
        # Build the full autotest tarball for hwqual image. We don't upload it,
        # as it's fairly large and only needed by the hwqual tarball.
        cros_build_lib.Info('Archiving full autotest tarball locally ...')
        tarball = commands.BuildFullAutotestTarball(self._build_root,
                                                    self._current_board,
                                                    image_dir)
        commands.ArchiveFile(tarball, archive_path)

        # Build hwqual image and upload to Google Storage.
        hwqual_name = 'chromeos-hwqual-%s-%s' % (board, self.version)
        filename = commands.ArchiveHWQual(buildroot, hwqual_name, archive_path,
                                          image_dir)
        self._release_upload_queue.put([filename])

    def ArchiveFirmwareImages():
      """Archive firmware images built from source if available."""
      archive = commands.BuildFirmwareArchive(buildroot, board, archive_path)
      if archive:
        self._release_upload_queue.put([archive])

    def BuildAndArchiveAllImages():
      # Generate the recovery image. To conserve loop devices, we try to only
      # run one instance of build_image at a time. TODO(davidjames): Move the
      # image generation out of the archive stage.

      # For recovery image to be generated correctly, BuildRecoveryImage must
      # run before BuildAndArchiveFactoryImages.
      if 'base' in config['images']:
        commands.BuildRecoveryImage(buildroot, board, image_dir, extra_env)
        self._recovery_image_status_queue.put(True)

      if config['images']:
        parallel.RunParallelSteps([BuildAndArchiveFactoryImages,
                                   ArchiveHWQual,
                                   ArchiveStandaloneTarballs,
                                   ArchiveZipFiles])

    def ArchiveImageScripts():
      """Archive tarball of generated image manipulation scripts."""
      target = os.path.join(archive_path, constants.IMAGE_SCRIPTS_TAR)
      files = glob.glob(os.path.join(image_dir, '*.sh'))
      files = [os.path.basename(f) for f in files]
      cros_build_lib.CreateTarball(target, image_dir, inputs=files)
      self._upload_queue.put([constants.IMAGE_SCRIPTS_TAR])

    def PushImage():
      # This helper script is only available on internal manifests currently.
      if not config['internal']:
        return

      # Now that all data has been generated, we can upload the final result to
      # the image server.
      # TODO: When we support branches fully, the friendly name of the branch
      # needs to be used with PushImages
      sign_types = []
      if config['name'].endswith('-%s' % cbuildbot_config.CONFIG_TYPE_FIRMWARE):
        sign_types += ['firmware']
      commands.PushImages(buildroot,
                          board=board,
                          branch_name='master',
                          archive_url=upload_url,
                          dryrun=debug or not config['push_image'],
                          profile=self._options.profile or config['profile'],
                          sign_types=sign_types)

    def ArchiveReleaseArtifacts():
      with self.ArtifactUploader(self._release_upload_queue, archive=False):
        steps = [ArchiveDebugSymbols, BuildAndArchiveAllImages,
                 ArchiveFirmwareImages]
        parallel.RunParallelSteps(steps)
      PushImage()

    def BuildAndArchiveArtifacts():
      # Run archiving steps in parallel.
      steps = [ArchiveReleaseArtifacts]
      if config['images']:
        steps.extend([self.ArchiveStrippedChrome, ArchiveImageScripts])
      if config['create_delta_sysroot']:
        steps.append(self.BuildAndArchiveDeltaSysroot)

      with parallel.BackgroundTaskRunner(
          UploadSymbols, queue=self._upload_symbols_queue, processes=1):
        with self.ArtifactUploader(self._upload_queue, archive=False):
          parallel.RunParallelSteps(steps)

    def MarkAsLatest():
      # Update and upload LATEST file.
      filename = 'LATEST-%s' % self._target_manifest_branch
      latest_path = os.path.join(self.bot_archive_root, filename)
      osutils.WriteFile(latest_path, self.version, mode='w')
      commands.UploadArchivedFile(
          self.bot_archive_root, self._GetGSUtilArchiveDir(), filename, debug)

    try:
      if not self._build_config['pgo_generate']:
        BuildAndArchiveArtifacts()
        MarkAsLatest()
    finally:
      commands.RemoveOldArchives(self.bot_archive_root,
                                 self._options.max_archive_builds)

  def _HandleStageException(self, exception):
    # Tell the HWTestStage not to wait for artifacts to be uploaded
    # in case ArchiveStage throws an exception.
    self._recovery_image_status_queue.put(False)
    return super(ArchiveStage, self)._HandleStageException(exception)


class UploadPrebuiltsStage(BoardSpecificBuilderStage):
  """Uploads binaries generated by this build for developer use."""

  option_name = 'prebuilts'
  config_name = 'prebuilts'

  def __init__(self, options, build_config, board, archive_stage, suffix=None):
    super(UploadPrebuiltsStage, self).__init__(options, build_config, board,
                                               suffix=suffix)
    self._archive_stage = archive_stage

  def GenerateCommonArgs(self):
    """Generate common prebuilt arguments."""
    generated_args = []
    if self._options.debug:
      generated_args.append('--debug')

    profile = self._options.profile or self._build_config['profile']
    if profile:
      generated_args.extend(['--profile', profile])

    # Generate the version if we are a manifest_version build.
    if self._build_config['manifest_version']:
      assert self._archive_stage, 'Archive stage missing for versioned build.'
      version = self._archive_stage.GetVersion()
      generated_args.extend(['--set-version', version])

    if self._build_config['git_sync']:
      # Git sync should never be set for pfq type builds.
      assert not cbuildbot_config.IsPFQType(self._prebuilt_type)
      generated_args.extend(['--git-sync'])

    return generated_args

  @classmethod
  def _AddOptionsForSlave(cls, builder, board):
    """Inner helper method to add upload_prebuilts args for a slave builder.

    Returns:
      An array of options to add to upload_prebuilts array that allow a master
      to submit prebuilt conf modifications on behalf of a slave.
    """
    args = []
    builder_config = cbuildbot_config.config[builder]
    if builder_config['prebuilts']:
      for slave_board in builder_config['boards']:
        if builder_config['master'] and slave_board == board:
          # Ignore self.
          continue

        args.extend(['--slave-board', slave_board])
        slave_profile = builder_config['profile']
        if slave_profile:
          args.extend(['--slave-profile', slave_profile])

    return args

  def PerformStage(self):
    """Uploads prebuilts for master and slave builders."""
    prebuilt_type = self._prebuilt_type
    board = self._current_board
    binhosts = []

    # Whether we publish public prebuilts.
    public = (self._build_config['prebuilts'] == constants.PUBLIC)
    # Common args we generate for all types of builds.
    generated_args = self.GenerateCommonArgs()
    # Args we specifically add for public/private build types.
    public_args, private_args = [], []
    # Public / private builders.
    public_builders, private_builders = [], []

    # Distributed builders that use manifest-versions to sync with one another
    # share prebuilt logic by passing around versions.
    if cbuildbot_config.IsPFQType(prebuilt_type):
      # Public pfqs should upload host preflight prebuilts.
      if prebuilt_type != constants.CHROME_PFQ_TYPE:
        public_args.append('--sync-host')

      # Deduplicate against previous binhosts.
      binhosts.extend(self._GetPortageEnvVar(_PORTAGE_BINHOST, board).split())
      binhosts.extend(self._GetPortageEnvVar(_PORTAGE_BINHOST, None).split())
      for binhost in filter(None, binhosts):
        generated_args.extend(['--previous-binhost-url', binhost])

      if self._build_config['master'] and board == self._boards[-1]:
        # The master builder updates all the binhost conf files, and needs to do
        # so only once so as to ensure it doesn't try to update the same file
        # more than once. As multiple boards can be built on the same builder,
        # we arbitrarily decided to update the binhost conf files when we run
        # upload_prebuilts for the last board. The other boards are treated as
        # slave boards.
        generated_args.append('--sync-binhost-conf')
        for c in self._GetSlavesForMaster(self._build_config):
          if c['prebuilts'] == constants.PUBLIC:
            public_builders.append(c['name'])
            public_args.extend(self._AddOptionsForSlave(c['name'], board))
          elif c['prebuilts'] == constants.PRIVATE:
            private_builders.append(c['name'])
            private_args.extend(self._AddOptionsForSlave(c['name'], board))

    # Upload the public prebuilts, if any.
    if public_builders or public:
      public_board = board if public else None
      commands.UploadPrebuilts(
          category=prebuilt_type, chrome_rev=self._chrome_rev,
          private_bucket=False, buildroot=self._build_root,
          board=public_board, extra_args=generated_args + public_args)

    # Upload the private prebuilts, if any.
    if private_builders or not public:
      private_board = board if not public else None
      commands.UploadPrebuilts(
          category=prebuilt_type, chrome_rev=self._chrome_rev,
          private_bucket=True, buildroot=self._build_root, board=private_board,
          extra_args=generated_args + private_args)


class DevInstallerPrebuiltsStage(UploadPrebuiltsStage):
  config_name = 'dev_installer_prebuilts'

  def PerformStage(self):
    generated_args = generated_args = self.GenerateCommonArgs()
    commands.UploadDevInstallerPrebuilts(
        binhost_bucket=self._build_config['binhost_bucket'],
        binhost_key=self._build_config['binhost_key'],
        binhost_base_url=self._build_config['binhost_base_url'],
        buildroot=self._build_root,
        board=self._current_board,
        extra_args=generated_args)


class PublishUprevChangesStage(NonHaltingBuilderStage):
  """Makes uprev changes from pfq live for developers."""
  def PerformStage(self):
    _, push_overlays = self._ExtractOverlays()
    if push_overlays:
      commands.UprevPush(self._build_root, push_overlays, self._options.debug)


class ReportStage(bs.BuilderStage):
  """Summarize all the builds."""

  _HTML_HEAD = """<html>
<head>
 <title>Archive Index: %(board)s / %(version)s</title>
</head>
<body>
<h2>Artifacts Index: %(board)s / %(version)s (%(config)s config)</h2>"""

  def __init__(self, options, build_config, archive_stages, version,
               sync_instance):
    bs.BuilderStage.__init__(self, options, build_config)
    self._archive_stages = archive_stages
    self._version = version if version else ''
    self._sync_instance = sync_instance

  def PerformStage(self):
    acl = None if self._build_config['internal'] else 'public-read'
    archive_urls = {}

    for board_config, archive_stage in sorted(self._archive_stages.iteritems()):
      board = board_config.board
      head_data = {
          'board': board,
          'config': board_config.name,
          'version': archive_stage.version,
      }
      head = self._HTML_HEAD % head_data

      url = archive_stage.download_url
      path = archive_stage.archive_path
      upload_url = archive_stage.upload_url

      # Generate the final metadata before we look at the uploaded list.
      if results_lib.Results.BuildSucceededSoFar():
        final_status = 'passed'
      else:
        final_status = 'failed'
      archive_stage.UploadMetadata(final_status=final_status,
                                   sync_instance=self._sync_instance)

      # Generate the index page needed for public reading.
      uploaded = os.path.join(path, commands.UPLOADED_LIST_FILENAME)
      if not os.path.exists(uploaded):
        if (not self._build_config['compilecheck'] and
            not self._options.compilecheck):
          # UPLOADED doesn't exist.  Normal if buildboard failed.
          logging.warning('board %s did not make it to the archive stage; '
                          'skipping', board)
        continue

      files = osutils.ReadFile(uploaded).splitlines() + [
          '.|Google Storage Index',
          '..|',
      ]
      index = os.path.join(path, 'index.html')
      commands.GenerateHtmlIndex(index, files, url_base=url, head=head)
      commands.UploadArchivedFile(path, upload_url, os.path.basename(index),
                                  debug=archive_stage.debug, acl=acl)

      archive_urls[board] = archive_stage.download_url + '/index.html'

    results_lib.Results.Report(sys.stdout, archive_urls=archive_urls,
                               current_version=self._version)
