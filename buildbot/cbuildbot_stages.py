# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
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
try:
  import Queue
except ImportError:
  # Python-3 renamed to "queue".  We still use Queue to avoid collisions
  # with naming variables as "queue".  Maybe we'll transition at some point.
  # pylint: disable=F0401
  import queue as Queue
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
from chromite.lib import alerts
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import git
from chromite.lib import gs
from chromite.lib import toolchain
from chromite.lib import osutils
from chromite.lib import parallel
from chromite.lib import patch as cros_patch
from chromite.lib import retry_util
from chromite.lib import timeout_util

_FULL_BINHOST = 'FULL_BINHOST'
_PORTAGE_BINHOST = 'PORTAGE_BINHOST'
_CROS_ARCHIVE_URL = 'CROS_ARCHIVE_URL'
_PRINT_INTERVAL = 1
_VM_TEST_ERROR_MSG = """
!!!VMTests failed!!!

Logs are uploaded in the corresponding %(vm_test_results)s. This can be found
by clicking on the artifacts link in the "Report" Stage. Specifically look
for the test_harness/failed for the failing tests. For more
particulars, please refer to which test failed i.e. above see the
individual test that failed -- or if an update failed, check the
corresponding update directory.
"""
PRE_CQ = validation_pool.PRE_CQ


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


class RetryStage(object):
  """Retry a given stage multiple times to see if it passes."""

  def __init__(self, builder_run, max_retry, stage, *args, **kwargs):
    """Create a RetryStage object.

    Args:
      builder_run: See arguments to bs.BuilderStage.__init__()
      max_retry: The number of times to try the given stage.
      stage: The stage class to create.
      *args: A list of arguments to pass to the stage constructor.
      **kwargs: A list of keyword arguments to pass to the stage constructor.
    """
    self._run = builder_run
    self.max_retry = max_retry
    self.stage = stage
    self.args = (builder_run,) + args
    self.kwargs = kwargs
    self.names = []
    self.attempt = None

  def GetStageNames(self):
    """Get a list of the places where this stage has recorded results."""
    return self.names[:]

  def _PerformStage(self):
    """Run the stage once, incrementing the attempt number as needed."""
    suffix = ' (attempt %d)' % (self.attempt,)
    stage_obj = self.stage(
        *self.args, attempt=self.attempt, max_retry=self.max_retry,
        suffix=suffix, **self.kwargs)
    self.names.extend(stage_obj.GetStageNames())
    self.attempt += 1
    stage_obj.Run()

  def Run(self):
    """Retry the given stage multiple times to see if it passes."""
    self.attempt = 1
    retry_util.RetryException(
        results_lib.RetriableStepFailure, self.max_retry, self._PerformStage)


class BoardSpecificBuilderStage(bs.BuilderStage):
  """Builder stage that is specific to a board."""

  def __init__(self, builder_run, board, **kwargs):
    super(BoardSpecificBuilderStage, self).__init__(builder_run, **kwargs)
    self._current_board = board

    if not isinstance(board, basestring):
      raise TypeError('Expected string, got %r' % (board,))

    # Add a board name suffix to differentiate between various boards (in case
    # more than one board is built on a single builder.)
    if len(self._boards) > 1 or self._run.config.grouped:
      self.name = '%s [%s]' % (self.name, board)

  def GetImageDirSymlink(self, pointer='latest-cbuildbot'):
    """Get the location of the current image."""
    return os.path.join(self._run.buildroot, 'src', 'build', 'images',
                        self._current_board, pointer)


class ArchivingStage(BoardSpecificBuilderStage):
  """Helper for stages that archive files.

  Attributes:
    archive_stage: The ArchiveStage instance for this board.
    bot_archive_root: The root path where output from this builder is stored.
    download_url: The URL where we can download artifacts.
    upload_url: The Google Storage location where we should upload artifacts.
  """

  PROCESSES = 10

  @classmethod
  def GetUploadACL(cls, config):
    """Get the ACL we should use to upload artifacts for a given config."""
    if config['internal']:
      # Use the bucket default ACL.
      return None
    return 'public-read'

  def __init__(self, builder_run, board, archive_stage, **kwargs):
    super(ArchivingStage, self).__init__(builder_run, board, **kwargs)
    self.acl = self.GetUploadACL(self._run.config)
    self.archive_stage = archive_stage

    self.debug = self._run.debug

    self.archive = builder_run.GetArchive()

    # TODO(mtennant): Do away with deprecated access below.
    self.version = self.archive.version
    self.archive_path = self.archive.archive_path
    self.bot_archive_root = os.path.dirname(self.archive_path)
    self.upload_url = self.archive.upload_url
    self.base_upload_url = os.path.dirname(self.upload_url)
    self.download_url = self.archive.download_url

  @contextlib.contextmanager
  def ArtifactUploader(self, queue=None, archive=True, strict=True):
    """Upload each queued input in the background.

    This context manager starts a set of workers in the background, who each
    wait for input on the specified queue. These workers run
    self.UploadArtifact(*args, archive=archive) for each input in the queue.

    Args:
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

    Args:
      path: Path of local file to upload to Google Storage.
      archive: Whether to automatically copy files to the archive dir.
      strict: Whether to treat upload errors as fatal.
    """
    filename = path
    if archive:
      filename = commands.ArchiveFile(path, self.archive_path)
    try:
      commands.UploadArchivedFile(
          self.archive_path, self.upload_url, filename, self.debug,
          update_list=True, acl=self.acl)
    except (cros_build_lib.RunCommandError, timeout_util.TimeoutError) as e:
      cros_build_lib.PrintBuildbotStepText('Upload failed')
      if strict:
        raise
      # Treat gsutil flake as a warning if it's the only problem.
      self._HandleExceptionAsWarning(e)

  def GetMetadata(self, stage=None, final_status=None, sync_instance=None,
                  completion_instance=None):
    """Constructs the metadata json object.

    Args:
      stage: The stage name that this metadata file is being uploaded for.
      final_status: Whether the build passed or failed. If None, the build
        will be treated as still running.
      sync_instance: The stage instance that was used for syncing the source
        code. This should be a derivative of SyncStage. If None, the list of
        commit queue patches will not be included in the metadata.
      completion_instance: The stage instance that was used to wait for slave
        completion. Used to add slave build information to master builder's
        metadata. If None, no such status information will be included. It not
        None, this should be a derivative of MasterSlaveSyncCompletionStage.
    """
    config = self._run.config

    start_time = results_lib.Results.start_time
    current_time = datetime.datetime.now()
    start_time_stamp = cros_build_lib.UserDateTimeFormat(timeval=start_time)
    current_time_stamp = cros_build_lib.UserDateTimeFormat(timeval=current_time)
    duration = '%s' % (current_time - start_time,)

    sdk_verinfo = cros_build_lib.LoadKeyValueFile(
        os.path.join(self._build_root, constants.SDK_VERSION_FILE),
        ignore_missing=True)
    verinfo = self._run.GetVersionInfo(self._build_root)
    metadata = {
        # Version of the metadata format.
        'metadata-version': '2',
        # Data for this build.
        'bot-config': config['name'],
        'bot-hostname': cros_build_lib.GetHostName(fully_qualified=True),
        'boards': config['boards'],
        'build-number': self._run.buildnumber,
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
    for entry in results_lib.Results.Get():
      timestr = datetime.timedelta(seconds=math.ceil(entry.time))
      if entry.result in results_lib.Results.NON_FAILURE_TYPES:
        status = constants.FINAL_STATUS_PASSED
      else:
        status = constants.FINAL_STATUS_FAILED
      metadata['results'].append({
          'name': entry.name,
          'status': status,
          # The result might be a custom exception.
          'summary': str(entry.result),
          'duration': '%s' % timestr,
          'description': entry.description,
          'log': self.ConstructDashboardURL(stage=entry.name),
      })

    commit_queue_stages = (CommitQueueSyncStage, PreCQSyncStage)
    if (sync_instance and isinstance(sync_instance, commit_queue_stages) and
        sync_instance.pool):
      changes = []
      pool = sync_instance.pool
      for change in pool.changes:
        details = {'gerrit_number': change.gerrit_number,
                   'patch_number': change.patch_number,
                   'internal': change.internal}
        for latest_patchset_only in (False, True):
          prefix = '' if latest_patchset_only else 'total_'
          for status in (pool.STATUS_FAILED, pool.STATUS_PASSED):
            count = pool.GetCLStatusCount(pool.bot, change, status,
                                          latest_patchset_only)
            details['%s%s' % (prefix, status.lower())] = count
        changes.append(details)
      metadata['changes'] = changes

    # If we were a CQ master, then include a summary of the status of slave cq
    # builders in metadata
    if config['master']:
      if (completion_instance and
          isinstance(completion_instance, MasterSlaveSyncCompletionStage)):
        statuses = completion_instance.GetSlaveStatuses()
        if not statuses:
          logging.warning('completion_instance did not have any statuses '
                          'to report. Will not add slave status to metadata.')
        metadata['slave_targets'] = {}
        for builder, status in statuses.iteritems():
          metadata['slave_targets'][builder] = status.AsFlatDict()

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
    archive_root = self._run.GetArchive().GetLocalArchiveRoot(trybot=True)
    osutils.RmDir(archive_root, ignore_missing=True)

  def _DeleteArchivedPerfResults(self):
    """Clear any previously stashed perf results from hw testing."""
    for result in glob.glob(os.path.join(
        self._run.options.log_dir,
        '*.%s' % HWTestStage.PERF_RESULTS_EXTENSION)):
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


class PatchChangesStage(bs.BuilderStage):
  """Stage that patches a set of Gerrit changes to the buildroot source tree."""
  def __init__(self, builder_run, patch_pool, **kwargs):
    """Construct a PatchChangesStage.

    Args:
      builder_run: BuilderRun object.
      patch_pool: A TrybotPatchPool object containing the different types of
                  patches to apply.
    """
    super(PatchChangesStage, self).__init__(builder_run, **kwargs)
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

  def _PatchSeriesFilter(self, series, changes):
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
        cros_internal=self._run.config.internal, cros=True)

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

  def __init__(self, builder_run, chromite_patch_pool,
               manifest_patch_pool=None, **kwargs):
    super(BootstrapStage, self).__init__(
        builder_run, trybot_patch_pool.TrybotPatchPool(), **kwargs)
    self.chromite_patch_pool = chromite_patch_pool
    self.manifest_patch_pool = manifest_patch_pool
    self.returncode = None

  def _ApplyManifestPatches(self, patch_pool):
    """Apply a pool of manifest patches to a temp manifest checkout.

    Args:
      patch_pool: The pool to apply.

    Returns:
      The path to the patched manifest checkout.

    Raises:
      Exception, if the new patched manifest cannot be parsed.
    """
    checkout_dir = os.path.join(self.tempdir, 'manfest-checkout')
    repository.CloneGitRepo(checkout_dir,
                            self._run.config.manifest_repo_url)

    patch_series = validation_pool.PatchSeries.WorkOnSingleRepo(
        checkout_dir, deps_filter_fn=trybot_patch_pool.ManifestFilter,
        tracking_branch=self._run.manifest_branch)

    self._ApplyPatchSeries(patch_series, patch_pool)
    # Create the branch that 'repo init -b <target_branch> -u <patched_repo>'
    # will look for.
    cmd = ['branch', '-f', self._run.manifest_branch,
           constants.PATCH_BRANCH]
    git.RunGit(checkout_dir, cmd)

    # Verify that the patched manifest loads properly. Propagate any errors as
    # exceptions.
    manifest = os.path.join(checkout_dir, self._run.config.manifest)
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
    filter_branch = self._run.manifest_branch
    if self._run.options.test_bootstrap:
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
    # pylint: disable=W0212
    cmd = self.FilterArgsForTargetCbuildbot(self.tempdir, cbuildbot_path,
                                            self._run.options)

    extra_params = ['--sourceroot=%s' % self._run.options.sourceroot]
    extra_params.extend(self._run.options.bootstrap_args)
    if self._run.options.test_bootstrap:
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

  def __init__(self, builder_run, **kwargs):
    super(SyncStage, self).__init__(builder_run, **kwargs)
    self.repo = None
    self.skip_sync = False

    # TODO(mtennant): Why keep a duplicate copy of this config value
    # at self.internal when it can always be retrieved from config?
    self.internal = self._run.config.internal

  def _GetManifestVersionsRepoUrl(self, read_only=False):
    return cbuildbot_config.GetManifestVersionsRepoUrl(
        self.internal,
        read_only=read_only)

  def Initialize(self):
    self._InitializeRepo()

  def _InitializeRepo(self):
    """Set up the RepoRepository object."""
    self.repo = self.GetRepoRepository()

  def GetNextManifest(self):
    """Returns the manifest to use."""
    return self._run.config.manifest

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
      if os.path.exists(self.repo.directory) and not self._run.options.clobber:
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
      elif self._run.options.buildbot:
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
      build_name=self._bot_id,
      incr_type='build',
      force=False,
      branch=self._run.manifest_branch)

    manifest_manager.BootstrapFromVersion(chrome_lkgm)
    return manifest_manager.GetLocalManifest(chrome_lkgm)


class ManifestVersionedSyncStage(SyncStage):
  """Stage that generates a unique manifest file, and sync's to it."""

  # TODO(mtennant): Make this into a builder run value.
  output_manifest_sha1 = False

  def __init__(self, builder_run, **kwargs):
    # Perform the sync at the end of the stage to the given manifest.
    super(ManifestVersionedSyncStage, self).__init__(builder_run, **kwargs)
    self.repo = None
    self.manifest_manager = None

    # If a builder pushes changes (even with dryrun mode), we need a writable
    # repository. Otherwise, the push will be rejected by the server.
    self.manifest_repo = self._GetManifestVersionsRepoUrl(read_only=False)

    # 1. If we're uprevving Chrome, Chrome might have changed even if the
    #    manifest has not, so we should force a build to double check. This
    #    means that we'll create a new manifest, even if there are no changes.
    # 2. If we're running with --debug, we should always run through to
    #    completion, so as to ensure a complete test.
    self._force = self._chrome_rev or self._run.options.debug

  def HandleSkip(self):
    """Initializes a manifest manager to the specified version if skipped."""
    super(ManifestVersionedSyncStage, self).HandleSkip()
    if self._run.options.force_version:
      self.Initialize()
      self.ForceVersion(self._run.options.force_version)

  def ForceVersion(self, version):
    """Creates a manifest manager from given version and returns manifest."""
    cros_build_lib.PrintBuildbotStepText(version)
    return self.manifest_manager.BootstrapFromVersion(version)

  def VersionIncrementType(self):
    """Return which part of the version number should be incremented."""
    if self._run.manifest_branch == 'master':
      return 'build'

    return 'branch'

  def RegisterManifestManager(self, manifest_manager):
    """Save the given manifest manager for later use in this run.

    Args:
      manifest_manager: Expected to be a BuildSpecsManager.
    """
    self._run.attrs.manifest_manager = self.manifest_manager = manifest_manager

  def Initialize(self):
    """Initializes a manager that manages manifests for associated stages."""

    dry_run = self._run.options.debug

    self._InitializeRepo()

    # If chrome_rev is somehow set, fail.
    assert not self._chrome_rev, \
        'chrome_rev is unsupported on release builders.'

    self.RegisterManifestManager(manifest_version.BuildSpecsManager(
        source_repo=self.repo,
        manifest_repo=self.manifest_repo,
        manifest=self._run.config.manifest,
        build_name=self._bot_id,
        incr_type=self.VersionIncrementType(),
        force=self._force,
        branch=self._run.manifest_branch,
        dry_run=dry_run,
        master=self._run.config.master))

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
    """Remove restricted checkouts from the manifest if needed.

    Args:
      manifest: The manifest to localize.
      filter_cros: If set, then only checkouts with a remote of 'cros' or
        'cros-internal' are kept, and the rest are filtered out.
    """
    if filter_cros:
      with osutils.TempDir() as tempdir:
        filtered_manifest = os.path.join(tempdir, 'filtered.xml')
        doc = ElementTree.parse(manifest)
        root = doc.getroot()
        for node in root.findall('project'):
          remote = node.attrib.get('remote')
          if remote and remote not in constants.GIT_REMOTES:
            root.remove(node)
        doc.write(filtered_manifest)
        yield filtered_manifest
    else:
      yield manifest

  def PerformStage(self):
    self.Initialize()
    if self._run.options.force_version:
      next_manifest = self.ForceVersion(self._run.options.force_version)
    else:
      next_manifest = self.GetNextManifest()

    if not next_manifest:
      cros_build_lib.Info('Found no work to do.')
      if self._run.attrs.manifest_manager.DidLastBuildFail():
        raise results_lib.StepFailure('The previous build failed.')
      else:
        sys.exit(0)

    # Log this early on for the release team to grep out before we finish.
    if self.manifest_manager:
      self._Print('\nRELEASETAG: %s\n' % (
          self.manifest_manager.current_version))

    # To keep local trybots working, remove restricted checkouts from the
    # official manifest we get from manifest-versions.
    with self.LocalizeManifest(
        next_manifest, filter_cros=self._run.options.local) as new_manifest:
      self.ManifestCheckout(new_manifest)


class MasterSlaveSyncStage(ManifestVersionedSyncStage):
  """Stage that generates a unique manifest file candidate, and sync's to it."""

  # TODO(mtennant): Turn this into self._run.attrs.sub_manager or similar.
  # An instance of lkgm_manager.LKGMManager for slave builds.
  sub_manager = None

  def __init__(self, builder_run, **kwargs):
    super(MasterSlaveSyncStage, self).__init__(builder_run, **kwargs)
    # lkgm_manager deals with making sure we're synced to whatever manifest
    # we get back in GetNextManifest so syncing again is redundant.
    self.skip_sync = True

  def _GetInitializedManager(self, internal):
    """Returns an initialized lkgm manager.

    Args:
      internal: Boolean.  True if this is using an internal manifest.

    Returns:
      lkgm_manager.LKGMManager.
    """
    increment = self.VersionIncrementType()
    return lkgm_manager.LKGMManager(
        source_repo=self.repo,
        manifest_repo=cbuildbot_config.GetManifestVersionsRepoUrl(
            internal, read_only=False),
        manifest=self._run.config.manifest,
        build_name=self._bot_id,
        build_type=self._run.config.build_type,
        incr_type=increment,
        force=self._force,
        branch=self._run.manifest_branch,
        dry_run=self._run.options.debug,
        master=self._run.config.master)

  def Initialize(self):
    """Override: Creates an LKGMManager rather than a ManifestManager."""
    self._InitializeRepo()
    self.RegisterManifestManager(self._GetInitializedManager(self.internal))
    if (self._run.config.master and
        self._GetSlavesForMaster(self._run.config)):
      assert self.internal, 'Unified masters must use an internal checkout.'
      MasterSlaveSyncStage.sub_manager = self._GetInitializedManager(False)

  def ForceVersion(self, version):
    manifest = super(MasterSlaveSyncStage, self).ForceVersion(version)
    if MasterSlaveSyncStage.sub_manager:
      MasterSlaveSyncStage.sub_manager.BootstrapFromVersion(version)

    return manifest

  def GetNextManifest(self):
    """Gets the next manifest using LKGM logic."""
    assert self.manifest_manager, \
        'Must run Initialize before we can get a manifest.'
    assert isinstance(self.manifest_manager, lkgm_manager.LKGMManager), \
        'Manifest manager instantiated with wrong class.'

    if self._run.config.master:
      manifest = self.manifest_manager.CreateNewCandidate()
      if MasterSlaveSyncStage.sub_manager:
        MasterSlaveSyncStage.sub_manager.CreateFromManifest(manifest)
      return manifest
    else:
      return self.manifest_manager.GetLatestCandidate()


class CommitQueueSyncStage(MasterSlaveSyncStage):
  """Commit Queue Sync stage that handles syncing and applying patches.

  This stage handles syncing to a manifest, passing around that manifest to
  other builders and finding the Gerrit Reviews ready to be committed and
  applying them into its own checkout.
  """

  def __init__(self, builder_run, **kwargs):
    super(CommitQueueSyncStage, self).__init__(builder_run, **kwargs)
    # Figure out the builder's name from the buildbot waterfall.
    builder_name = self._run.config.paladin_builder_name
    self.builder_name = builder_name if builder_name else self._run.config.name

    # The pool of patches to be picked up by the commit queue.
    # - For the master commit queue, it's initialized in GetNextManifest.
    # - For slave commit queues, it's initialized in _SetPoolFromManifest.
    #
    # In all cases, the pool is saved to disk, and refreshed after bootstrapping
    # by HandleSkip.
    self.pool = None

  def HandleSkip(self):
    """Handles skip and initializes validation pool from manifest."""
    super(CommitQueueSyncStage, self).HandleSkip()
    filename = self._run.options.validation_pool
    if filename:
      self.pool = validation_pool.ValidationPool.Load(filename)
    else:
      self._SetPoolFromManifest(self.manifest_manager.GetLocalManifest())

  def _ChangeFilter(self, pool, changes, non_manifest_changes):
    # First, look for changes that were tested by the Pre-CQ.
    changes_to_test = []
    for change in changes:
      status = pool.GetCLStatus(PRE_CQ, change)
      if status == manifest_version.BuilderStatus.STATUS_PASSED:
        changes_to_test.append(change)

    # If we only see changes that weren't verified by Pre-CQ, try all of the
    # changes. This ensures that the CQ continues to work even if the Pre-CQ is
    # down.
    if not changes_to_test:
      changes_to_test = changes

    return changes_to_test, non_manifest_changes

  def _SetPoolFromManifest(self, manifest):
    """Sets validation pool based on manifest path passed in."""
    # Note that GetNextManifest() calls GetLatestCandidate() in this case,
    # so the repo will already be sync'd appropriately. This means that
    # AcquirePoolFromManifest does not need to sync.
    self.pool = validation_pool.ValidationPool.AcquirePoolFromManifest(
        manifest, self._run.config.overlays, self.repo,
        self._run.buildnumber, self.builder_name,
        self._run.config.master, self._run.options.debug)

  def GetNextManifest(self):
    """Gets the next manifest using LKGM logic."""
    assert self.manifest_manager, \
        'Must run Initialize before we can get a manifest.'
    assert isinstance(self.manifest_manager, lkgm_manager.LKGMManager), \
        'Manifest manager instantiated with wrong class.'

    if self._run.config.master:
      try:
        # In order to acquire a pool, we need an initialized buildroot.
        if not git.FindRepoDir(self.repo.directory):
          self.repo.Initialize()
        self.pool = pool = validation_pool.ValidationPool.AcquirePool(
            self._run.config.overlays, self.repo,
            self._run.buildnumber, self.builder_name,
            self._run.options.debug,
            check_tree_open=not self._run.options.debug or
                            self._run.options.mock_tree_status,
            changes_query=self._run.options.cq_gerrit_override,
            change_filter=self._ChangeFilter, throttled_ok=True)

      except validation_pool.TreeIsClosedException as e:
        cros_build_lib.Warning(str(e))
        return None

      manifest = self.manifest_manager.CreateNewCandidate(validation_pool=pool)
      if MasterSlaveSyncStage.sub_manager:
        MasterSlaveSyncStage.sub_manager.CreateFromManifest(manifest)

      return manifest
    else:
      manifest = self.manifest_manager.GetLatestCandidate()
      if manifest:
        self._SetPoolFromManifest(manifest)
        self.pool.ApplyPoolIntoRepo()

      return manifest

  def PerformStage(self):
    """Performs normal stage and prints blamelist at end."""
    if self._run.options.force_version:
      self.HandleSkip()
    else:
      ManifestVersionedSyncStage.PerformStage(self)


class ManifestVersionedSyncCompletionStage(ForgivingBuilderStage):
  """Stage that records board specific results for a unique manifest file."""

  option_name = 'sync'

  def __init__(self, builder_run, sync_stage, success, **kwargs):
    super(ManifestVersionedSyncCompletionStage, self).__init__(
        builder_run, **kwargs)
    self.sync_stage = sync_stage
    self.success = success
    # Message that can be set that well be sent along with the status in
    # UpdateStatus.
    self.message = None

  def PerformStage(self):
    self._run.attrs.manifest_manager.UpdateStatus(
        success=self.success, message=self.message)


class ImportantBuilderFailedException(results_lib.StepFailure):
  """Exception thrown when an important build fails to build."""


class MasterSlaveSyncCompletionStage(ManifestVersionedSyncCompletionStage):
  """Stage that records whether we passed or failed to build/test manifest."""

  def __init__(self, *args, **kwargs):
    super(MasterSlaveSyncCompletionStage, self).__init__(*args, **kwargs)
    self._slave_statuses = {}

  def _FetchSlaveStatuses(self):
    """Fetch and return build status for this build and any of its slaves.

    Returns:
      A build-names->status dictionary of build statuses.
    """

    # TODO(mtennant): When testing a master in debug mode, it is actually very
    # helpful to allow this code to check on slave status IF the run was with
    # a specified manifest version (--version argument).  In such a case, the
    # master fetches the statuses of previously finished slaves (from the real
    # runs, presumably completed earlier), nicely executing more of this code.
    # I suggest allowing a master with --debug and --version to run this code.
    if self._run.options.debug:
      # In debug mode, nothing is uploaded to Google Storage, so we bypass
      # the extra hop and just look at what we have locally.
      status = manifest_version.BuilderStatus.GetCompletedStatus(self.success)
      status_obj = manifest_version.BuilderStatus(status, self.message)
      return {self._bot_id: status_obj}
    elif not self._run.config.master:
      # Slaves only need to look at their own status.
      return self._run.attrs.manifest_manager.GetBuildersStatus([self._bot_id])
    else:
      builders = self._GetSlavesForMaster(self._run.config)
      manager = self._run.attrs.manifest_manager
      sub_manager = MasterSlaveSyncStage.sub_manager
      if sub_manager:
        # TODO(build): There appears to be no reason the public and private
        # statuses cannot be gathered at the same time.  This would avoid
        # having two separate long timeout periods involved.
        public_builders = [b['name'] for b in builders if not b['internal']]
        statuses = sub_manager.GetBuildersStatus(public_builders)
        private_builders = [b['name'] for b in builders if b['internal']]
        statuses.update(manager.GetBuildersStatus(private_builders))
      else:
        statuses = manager.GetBuildersStatus([b['name'] for b in builders])
      return statuses

  def _AbortCQHWTests(self):
    """Abort any HWTests started by the CQ."""
    manifest_manager = self._run.attrs.manifest_manager
    if (cbuildbot_config.IsCQType(self._run.config.build_type) and
        manifest_manager is not None and
        self._run.manifest_branch == 'master'):
      release_tag = manifest_manager.current_version
      if release_tag and not commands.HaveHWTestsBeenAborted(release_tag):
        commands.AbortHWTests(release_tag, self._run.options.debug)

  def _HandleStageException(self, exception):
    """Decide whether an exception should be treated as fatal."""
    # Besides the master, the completion stages also run on slaves, to report
    # their status back to the master. If the build failed, they throw an
    # exception here. For slave builders, marking this stage 'red' would be
    # redundant, since the build itself would already be red. In this case,
    # report a warning instead.
    # pylint: disable=W0212
    if (isinstance(exception, ImportantBuilderFailedException) and
        not self._run.config.master):
      return self._HandleExceptionAsWarning(exception)
    else:
      # In all other cases, exceptions should be treated as fatal. To
      # implement this, we bypass ForgivingStage and call
      # bs.BuilderStage._HandleStageException explicitly.
      return bs.BuilderStage._HandleStageException(self, exception)

  def HandleSuccess(self):
    """Handle a successful build.

    This function is called whenever the cbuildbot run is successful.
    For the master, this will only be called when all slave builders
    are also successful. This function may be overridden by subclasses.
    """
    # We only promote for the pfq, not chrome pfq.
    # TODO(build): Run this logic in debug mode too.
    if (not self._run.options.debug and
        cbuildbot_config.IsPFQType(self._run.config.build_type) and
        self._run.config.master and
        self._run.manifest_branch == 'master' and
        self._run.config.build_type != constants.CHROME_PFQ_TYPE):
      self._run.attrs.manifest_manager.PromoteCandidate()
      if MasterSlaveSyncStage.sub_manager:
        MasterSlaveSyncStage.sub_manager.PromoteCandidate()

  def HandleFailure(self, failing, inflight):
    """Handle a build failure.

    This function is called whenever the cbuildbot run fails.
    For the master, this will be called when any slave fails or times
    out. This function may be overridden by subclasses.

    Args:
      failing: The names of the failing builders.
      inflight: The names of the builders that are still running.
    """
    if failing:
      self.HandleValidationFailure(failing)
    elif inflight:
      self.HandleValidationTimeout(inflight)

  def HandleValidationFailure(self, failing):
    cros_build_lib.PrintBuildbotStepWarnings()
    cros_build_lib.Warning('\n'.join([
        'The following builders failed with this manifest:',
        ', '.join(sorted(failing)),
        'Please check the logs of the failing builders for details.']))

  def HandleValidationTimeout(self, inflight_statuses):
    cros_build_lib.PrintBuildbotStepWarnings()
    cros_build_lib.Warning('\n'.join([
        'The following builders took too long to finish:',
        ', '.join(sorted(inflight_statuses)),
        'Please check the logs of these builders for details.']))

  def PerformStage(self):
    # Upload our pass/fail status to Google Storage.
    self._run.attrs.manifest_manager.UploadStatus(
        success=self.success, message=self.message,
        dashboard_url=self.ConstructDashboardURL())

    statuses = self._FetchSlaveStatuses()
    self._slave_statuses = statuses
    failing = set(builder for builder, status in statuses.iteritems()
                  if status.Failed())
    inflight = set(builder for builder, status in statuses.iteritems()
                   if status.Inflight())

    # If all the failing or inflight builders were sanity checkers
    # then ignore the failure.
    fatal = self._IsFailureFatal(failing, inflight)

    if fatal:
      self._AnnotateFailingBuilders(failing, inflight, statuses)
      self.HandleFailure(failing, inflight)
      raise ImportantBuilderFailedException()
    else:
      self.HandleSuccess()

  def _IsFailureFatal(self, failing, inflight):
    """Returns a boolean indicating whether the build should fail.

    Args:
      failing: Set of builder names of slave builders that failed.
      inflight: Set of builder names of slave builders that are inflight

    Returns:
      True if any of the failing or inflight builders are not sanity check
      builders for this master.
    """
    sanity_builders = self._run.config.sanity_check_slaves or []
    sanity_builders = set(sanity_builders)
    return not sanity_builders.issuperset(failing | inflight)

  def _AnnotateFailingBuilders(self, failing, inflight, statuses):
    """Add annotations that link to either failing or inflight builders.

    Adds buildbot links to failing builder dashboards. If no builders are
    failing, adds links to inflight builders.

    Args:
      failing: Set of builder names of slave builders that failed.
      inflight: Set of builder names of slave builders that are inflights.
      statuses: A builder-name->status dictionary, which will provide
                the dashboard_url values for any links.
    """
    builders_to_link = failing or inflight or []
    for builder in builders_to_link:
      if statuses[builder].dashboard_url:
        cros_build_lib.PrintBuildbotLink(builder,
                                         statuses[builder].dashboard_url)

  def GetSlaveStatuses(self):
    """Returns cached slave status results.

    Cached results are populated during PerformStage, so this function
    should only be called after PerformStage has returned.

    Returns:
      A dictionary from build names to manifest_version.BuilderStatus
      builder status objects.
    """
    return self._slave_statuses


class CommitQueueCompletionStage(MasterSlaveSyncCompletionStage):
  """Commits or reports errors to CL's that failed to be validated."""

  def HandleSuccess(self):
    if self._run.config.master:
      self.sync_stage.pool.SubmitPool()
      # After submitting the pool, update the commit hashes for uprevved
      # ebuilds.
      manifest = git.ManifestCheckout.Cached(self._build_root)
      portage_utilities.EBuild.UpdateCommitHashesForChanges(
          self.sync_stage.pool.changes, self._build_root, manifest)
      if cbuildbot_config.IsPFQType(self._run.config.build_type):
        super(CommitQueueCompletionStage, self).HandleSuccess()

  def HandleFailure(self, failing, inflight):
    """Handle a build failure or timeout in the Commit Queue.

    This function performs any tasks that need to happen when the Commit Queue
    fails:
      - Abort the HWTests if necessary.
      - Push any CLs that indicate that they don't care about this failure.
      - Reject the rest of the changes, but only if the sanity check builders
        did NOT fail.

    See MasterSlaveSyncCompletionStage.HandleFailure.

    Args:
      failing: Names of the builders that failed.
      inflight: Names of the builders that timed out.
    """
    # Print out the status about what builds failed or not.
    MasterSlaveSyncCompletionStage.HandleFailure(self, failing, inflight)

    # Abort hardware tests to save time if we have already seen a failure,
    # except in the case where the only failure is a hardware test failure.
    #
    # When we're debugging hardware test failures, it's useful to see the
    # results on all platforms, to see if the failure is platform-specific.
    tracebacks = results_lib.Results.GetTracebacks()
    if not self.success and self._run.config['important']:
      if len(tracebacks) != 1 or tracebacks[0].failed_prefix != 'HWTest':
        self._AbortCQHWTests()

    if self._run.config.master:
      # Even if there was a failure, we can submit the changes that indicate
      # that they don't care about this failure.
      messages = [self._slave_statuses[x].message for x in failing]

      if failing and not inflight:
        tracebacks = set()
        for message in messages:
          # If there are no tracebacks, that means that the builder did not
          # report its status properly. Don't submit anything.
          if not message.tracebacks:
            break
          tracebacks.update(message.tracebacks)
        else:
          rejected = self.sync_stage.pool.SubmitPartialPool(tracebacks)
          self.sync_stage.pool.changes = rejected

      sanity = self._WasBuildSane(
          self._run.config.sanity_check_slaves, self._slave_statuses)
      if not sanity:
        logging.info('Detected that a sanity-check builder failed. Will not '
                     'reject patches.')
      if failing:
        self.sync_stage.pool.HandleValidationFailure(messages, sanity=sanity)
      elif inflight:
        self.sync_stage.pool.HandleValidationTimeout(sanity=sanity)

  @staticmethod
  def _WasBuildSane(sanity_check_slaves, slave_statuses):
    """Determines weather any of the sanity check slaves failed."""
    sanity_check_slaves = sanity_check_slaves or []
    # Ignore any sanity_check_slaves builders for which we do not have a
    # status (perhaps because they timed out or never ran).
    # Of those that do have a status, if any of them failed,
    # call the build not sane.
    return not any([slave_statuses.has_key(x) and slave_statuses[x].Failed()
                    for x in sanity_check_slaves])

  def PerformStage(self):
    # - If the build failed, and the builder was important, fetch a message
    # listing the patches which failed to be validated. This message is sent
    # along with the failed status to the master to indicate a failure.
    # - This is skipped when sync_stage did not apply a validation pool. For
    # instance on builders with do_not_apply_cq_patches=True, sync_stage will
    # be a MasterSlaveSyncStage and not have a |pool| attribute.
    if (not self.success and self._run.config.important
        and hasattr(self.sync_stage, 'pool')):
      self.message = self.sync_stage.pool.GetValidationFailedMessage()

    super(CommitQueueCompletionStage, self).PerformStage()

    self._run.attrs.manifest_manager.UpdateStatus(
        success=self.success, message=self.message)


class PreCQSyncStage(SyncStage):
  """Sync and apply patches to test if they compile."""

  def __init__(self, builder_run, patches, **kwargs):
    super(PreCQSyncStage, self).__init__(builder_run, **kwargs)

    # The list of patches to test.
    self.patches = patches

    # The ValidationPool of patches to test. Initialized in PerformStage, and
    # refreshed after bootstrapping by HandleSkip.
    self.pool = None

  def HandleSkip(self):
    """Handles skip and loads validation pool from disk."""
    super(PreCQSyncStage, self).HandleSkip()
    filename = self._run.options.validation_pool
    if filename:
      self.pool = validation_pool.ValidationPool.Load(filename)

  def PerformStage(self):
    super(PreCQSyncStage, self).PerformStage()
    self.pool = validation_pool.ValidationPool.AcquirePreCQPool(
        self._run.config.overlays, self._build_root,
        self._run.buildnumber, self._run.config.name,
        dryrun=self._run.options.debug_forced, changes=self.patches)
    self.pool.ApplyPoolIntoRepo()


class PreCQCompletionStage(bs.BuilderStage):
  """Reports the status of a trybot run to Google Storage and Gerrit."""

  def __init__(self, builder_run, sync_stage, success, **kwargs):
    super(PreCQCompletionStage, self).__init__(builder_run, **kwargs)
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
  LAUNCH_DELAY = 30

  # The maximum number of patches we will allow in a given trybot run. This is
  # needed because our trybot infrastructure can only handle so many patches at
  # once.
  MAX_PATCHES_PER_TRYBOT_RUN = 50

  def __init__(self, builder_run, **kwargs):
    super(PreCQLauncherStage, self).__init__(builder_run, **kwargs)
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
      status = pool.GetCLStatus(PRE_CQ, change)

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
            msg = ('We were not able to launch a pre-cq trybot for your change.'
                   '\n\n'
                   'This problem can happen if the trybot waterfall is very '
                   'busy, or if there is an infrastructure issue. Please '
                   'notify the sheriff and mark your change as ready again. If '
                   'this problem occurs multiple times in a row, please file a '
                   'bug.')

            pool.SendNotification(change, '%(details)s', details=msg)
            pool.RemoveCommitReady(change)
            pool.UpdateCLStatus(PRE_CQ, change, self.STATUS_FAILED,
                                self._run.options.debug)
            self.retried.discard(change)
          else:
            # Try the change again.
            self.retried.add(change)
            pool.UpdateCLStatus(PRE_CQ, change, self.STATUS_WAITING,
                                self._run.options.debug)
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
        pool.UpdateCLStatus(PRE_CQ, change, self.STATUS_WAITING,
                            self._run.options.debug)
      elif status == self.STATUS_PASSED:
        passed.add(change)

    return busy, passed

  def LaunchTrybot(self, pool, plan):
    """Launch a Pre-CQ run with the provided list of CLs.

    Args:
      pool: ValidationPool corresponding to |plan|.
      plan: The list of patches to test in the Pre-CQ run.
    """
    cmd = ['cbuildbot', '--remote', constants.PRE_CQ_BUILDER_NAME]
    if self._run.options.debug:
      cmd.append('--debug')
    for patch in plan:
      number = cros_patch.FormatGerritNumber(
          patch.gerrit_number, force_internal=patch.internal)
      cmd += ['-g', number]
    cros_build_lib.RunCommand(cmd, cwd=self._build_root)
    for patch in plan:
      if pool.GetCLStatus(PRE_CQ, patch) != self.STATUS_PASSED:
        pool.UpdateCLStatus(PRE_CQ, patch, self.STATUS_LAUNCHING,
                            self._run.options.debug)

  def GetDisjointTransactionsToTest(self, pool, changes):
    """Get the list of disjoint transactions to test.

    Returns:
      A list of disjoint transactions to test. Each transaction should be sent
      to a different Pre-CQ trybot.
    """
    busy, passed = self.GetPreCQStatus(pool, changes)

    # Create a list of disjoint transactions to test.
    manifest = git.ManifestCheckout.Cached(self._build_root)
    plans = pool.CreateDisjointTransactions(
        manifest, max_txn_length=self.MAX_PATCHES_PER_TRYBOT_RUN)
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
    if timeout_util.IsTreeOpen(
        validation_pool.ValidationPool.STATUS_URL):
      pool.SubmitNonManifestChanges(check_tree_open=False)

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
        self._run.config.overlays, self.repo,
        self._run.buildnumber,
        urllib.quote(constants.PRE_CQ_LAUNCHER_NAME),
        dryrun=self._run.options.debug,
        changes_query=self._run.options.cq_gerrit_override,
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
  remotely for each checkout in the manifest at the specified hash.

  Once a branch is created however, the branch component of the version on the
  newly created branch needs to be incremented.  Additionally, in some cases
  the Chrome major version (i.e, R29) and/or the Chrome OS version (i.e.,
  4319.0.0) of the source branch must be incremented
  (see _IncrementVersionOnDiskForSourceBranch docstring).  Finally, the external
  and internal manifests of the new branch need to be fixed up (see
  FixUpManifests docstring).
  """

  COMMIT_MESSAGE = 'Bump %(target)s after branching %(branch)s'

  def __init__(self, builder_run, **kwargs):
    super(BranchUtilStage, self).__init__(builder_run, **kwargs)
    self.dryrun = self._run.options.debug_forced
    self.branch_name = self._run.options.branch_name
    self.rename_to = self._run.options.rename_to

  def _RunPush(self, checkout, src_ref, dest_ref, force=False):
    """Perform a git push for a checkout.

    Args:
      checkout: A dictionary of checkout manifest attributes.
      src_ref: The source local ref to push to the remote.
      dest_ref: The local remote ref that correspond to destination ref name.
      force: Whether to override non-fastforward checks.
    """
    # Convert local tracking ref to refs/heads/* on a remote:
    # refs/remotes/<remote name>/<branch> to refs/heads/<branch>.
    # If dest_ref is already refs/heads/<branch> it's a noop.
    dest_ref = git.NormalizeRef(git.StripRefs(dest_ref))
    push_to = git.RemoteRef(checkout['push_remote'], dest_ref)
    git.GitPush(checkout['local_path'], src_ref, push_to, dryrun=self.dryrun,
                force=force)

  def _FetchAndCheckoutTo(self, checkout_dir, remote_ref):
    """Fetch a remote ref and check out to it.

    Args:
      checkout_dir: Path to git repo to operate on.
      remote_ref: A git.RemoteRef object.
    """
    git.RunGit(checkout_dir, ['fetch', remote_ref.remote, remote_ref.ref],
               print_cmd=True)
    git.RunGit(checkout_dir, ['checkout', 'FETCH_HEAD'], print_cmd=True)

  def _GetBranchSuffix(self, manifest, checkout):
    """Return the branch suffix for the given checkout.

    If a given project is checked out to multiple locations, it is necessary
    to append a branch suffix. To be safe, we append branch suffixes for all
    repositories that use a non-standard branch name (e.g., if our default
    revision is "master", then any repository which does not use "master"
    has a non-standard branch name.)

    Args:
      manifest: The associated ManifestCheckout.
      checkout: The associated ProjectCheckout.
    """
    # Get the default and tracking branch.
    suffix = ''
    if len(manifest.FindCheckouts(checkout['name'])) > 1:
      default_branch = git.StripRefs(manifest.default['revision'])
      tracking_branch = git.StripRefs(checkout['tracking_branch'])
      suffix = '-%s' % (tracking_branch,)
      if default_branch != 'master':
        suffix = re.sub('^-%s-' % re.escape(default_branch), '-', suffix)
    return suffix

  def _GetSHA1(self, checkout, branch):
    """Get the SHA1 for the specified |branch| in the specified |checkout|.

    Args:
      checkout: The ProjectCheckout to look in.
      branch: Remote branch to look for.

    Returns:
      If the branch exists, returns the SHA1 of the branch. Otherwise, returns
      the empty string.  If branch is None, return None.
    """
    if branch:
      cmd = ['show-ref', branch]
      result = git.RunGit(checkout['local_path'], cmd, error_code_ok=True)
      if result.returncode == 0:
        # Output looks like:
        # a00733b...30ee40e0c2c1 refs/remotes/cros/test-4980.B
        return result.output.strip().split()[0]

      return ''

  def _CopyBranch(self, src_checkout, src_branch, dst_branch, force=False):
    """Copy the given |src_branch| to |dst_branch|.

    Args:
      src_checkout: The ProjectCheckout to work in.
      src_branch: The remote branch ref to copy from.
      dst_branch: The remote branch ref to copy to.
      force: If True then execute the copy even if dst_branch exists.
    """
    cros_build_lib.Info('Creating new branch "%s" for %s.',
                        dst_branch, src_checkout['name'])
    self._RunPush(src_checkout, src_ref=src_branch, dest_ref=dst_branch,
                  force=force)

  def _DeleteBranch(self, src_checkout, branch):
    """Delete the given |branch| in the given |src_checkout|.

    Args:
      src_checkout: The ProjectCheckout to work in.
      branch: The branch ref to delete.  Must be a remote branch.
    """
    cros_build_lib.Info('Deleting branch "%s" for %s.',
                        branch, src_checkout['name'])
    self._RunPush(src_checkout, src_ref='', dest_ref=branch)

  def _ProcessCheckout(self, src_manifest, src_checkout):
    """Performs per-checkout push operations.

    Args:
      src_manifest: The ManifestCheckout object for the current manifest.
      src_checkout: The ProjectCheckout object to process.
    """
    if not src_checkout.IsBranchableProject():
      # We don't have the ability to push branches to this repository. Just
      # use TOT instead.
      return

    checkout_name = src_checkout['name']
    remote = src_checkout['push_remote']
    src_ref = src_checkout['revision']
    suffix = self._GetBranchSuffix(src_manifest, src_checkout)

    # The source/destination branches depend on options.
    if self.rename_to:
      # Rename flow.  Both src and dst branches exist.
      src_branch = '%s%s' % (self.branch_name, suffix)
      dst_branch = '%s%s' % (self.rename_to, suffix)
    elif self._run.options.delete_branch:
      # Delete flow.  Only dst branch exists.
      src_branch = None
      dst_branch = '%s%s' % (self.branch_name, suffix)
    else:
      # Create flow (default).  Only dst branch exists.  Source
      # for the branch will just be src_ref.
      src_branch = None
      dst_branch = '%s%s' % (self.branch_name, suffix)

    # Normalize branch refs to remote.  We only process remote branches.
    src_branch = git.NormalizeRemoteRef(remote, src_branch)
    dst_branch = git.NormalizeRemoteRef(remote, dst_branch)

    # Determine whether src/dst branches exist now, by getting their sha1s.
    if src_branch:
      src_sha1 = self._GetSHA1(src_checkout, src_branch)
    elif git.IsSHA1(src_ref):
      src_sha1 = src_ref
    dst_sha1 = self._GetSHA1(src_checkout, dst_branch)

    # Complain if the branch already exists, unless that is expected.
    force = self._run.options.force_create or self._run.options.delete_branch
    if dst_sha1 and not force:
      # We are either creating a branch or renaming a branch, and the
      # destination branch unexpectedly exists.  Accept this only if the
      # destination branch is already at the revision we want.
      if src_sha1 != dst_sha1:
        raise BranchError('Checkout %s already contains branch %s.  Run with '
                          '--force-create to overwrite.'
                          % (checkout_name, dst_branch))

      cros_build_lib.Info('Checkout %s already contains branch %s and it '
                          'already points to revision %s', checkout_name,
                          dst_branch, dst_sha1)

    elif self._run.options.delete_branch:
      # Delete the dst_branch, if it exists.
      if dst_sha1:
        self._DeleteBranch(src_checkout, dst_branch)
      else:
        raise BranchError('Checkout %s does not contain branch %s to delete.'
                          % (checkout_name, dst_branch))

    elif self.rename_to:
      # Copy src_branch to dst_branch, if it exists, then delete src_branch.
      if src_sha1:
        self._CopyBranch(src_checkout, src_branch, dst_branch)
        self._DeleteBranch(src_checkout, src_branch)
      else:
        raise BranchError('Checkout %s does not contain branch %s to rename.'
                          % (checkout_name, src_branch))

    else:
      # Copy src_ref to dst_branch.
      self._CopyBranch(src_checkout, src_ref, dst_branch,
                       force=self._run.options.force_create)

  def _UpdateManifest(self, manifest_path):
    """Rewrite |manifest_path| to point at the right branch.

    Args:
      manifest_path: The path to the manifest file.
    """
    src_manifest = git.ManifestCheckout.Cached(self._build_root,
                                               manifest_path=manifest_path)
    doc = ElementTree.parse(manifest_path)
    root = doc.getroot()

    # Use the local branch ref.
    new_branch_name = self.rename_to if self.rename_to else self.branch_name
    new_branch_name = git.NormalizeRef(new_branch_name)

    for node in root.findall('project'):
      path = node.attrib['path']
      checkout = src_manifest.FindCheckoutFromPath(path)
      if checkout.IsBranchableProject():
        # Point at the new branch.
        node.attrib.pop('revision', None)
        node.attrib.pop('upstream', None)
        suffix = self._GetBranchSuffix(src_manifest, checkout)
        if suffix:
          node.attrib['revision'] = '%s%s' % (new_branch_name, suffix)
      else:
        # We can't branch this repository. Just use TOT of the repository.
        node.attrib['revision'] = checkout['revision']
        upstream = checkout.get('upstream')
        if upstream is not None:
          node.attrib['upstream'] = upstream

    for node in root.findall('default'):
      node.attrib['revision'] = new_branch_name

    doc.write(manifest_path)

  def _FixUpManifests(self, repo_manifest):
    """Points the checkouts at the new branch in the manifests.

    The 'master' branch manifest (full.xml) contains checkouts that are checked
    out to branches other than 'refs/heads/master'.  But in the new branch,
    these should all be checked out to 'refs/heads/<new_branch>', so we go
    through the manifest and fix those checkouts.
    """
    assert not self._run.options.delete_branch, 'Cannot fix a deleted branch.'

    # Use local branch ref.
    branch_ref = git.NormalizeRef(self.branch_name)

    cros_build_lib.Debug('Fixing manifest projects for new branch.')
    for project in constants.MANIFEST_PROJECTS:
      manifest_checkout = repo_manifest.FindCheckout(project)
      manifest_dir = manifest_checkout['local_path']
      manifest_path = os.path.join(manifest_dir, 'full.xml')

      push_remote = manifest_checkout['push_remote']

      # Checkout revision can be either a sha1 or a branch ref.
      src_ref = manifest_checkout['revision']
      if not git.IsSHA1(src_ref):
        src_ref = git.NormalizeRemoteRef(push_remote, src_ref)

      git.CreateBranch(
          manifest_dir, manifest_version.PUSH_BRANCH, src_ref)
      self._UpdateManifest(manifest_path)
      git.RunGit(manifest_dir, ['add', '-A'], print_cmd=True)
      message = 'Fix up manifest after branching %s.' % branch_ref
      git.RunGit(manifest_dir, ['commit', '-m', message], print_cmd=True)
      push_to = git.RemoteRef(push_remote, branch_ref)
      git.GitPush(manifest_dir, manifest_version.PUSH_BRANCH, push_to,
                  dryrun=self.dryrun, force=self.dryrun)

  def _IncrementVersionOnDisk(self, incr_type, push_to, message):
    """Bumps the version found in chromeos_version.sh on a branch.

    Args:
      incr_type: See docstring for manifest_version.VersionInfo.
      push_to: A git.RemoteRef object.
      message: The message to give the git commit that bumps the version.
    """
    version_info = manifest_version.VersionInfo.from_repo(
        self._build_root, incr_type=incr_type)
    version_info.IncrementVersion()
    version_info.UpdateVersionFile(message, dry_run=self.dryrun,
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

  def _IncrementVersionOnDiskForNewBranch(self, push_remote):
    """Bumps the version found in chromeos_version.sh on the new branch

    When a new branch is created, the branch component of the new branch's
    version needs to bumped.

    For example, say 'stabilize-link' is created from a the 4230.0.0 manifest.
    The new branch's version needs to be bumped to 4230.1.0.

    Args:
      push_remote: a git remote name where the new branch lives.
    """
    # This needs to happen before the source branch version bumping above
    # because we rely on the fact that since our current overlay checkout
    # is what we just pushed to the new branch, we don't need to do another
    # sync.  This also makes it easier to implement dryrun functionality (the
    # new branch doesn't actually get created in dryrun mode).

    # Use local branch ref.
    branch_ref = git.NormalizeRef(self.branch_name)
    push_to = git.RemoteRef(push_remote, branch_ref)
    version_info = manifest_version.VersionInfo(
        version_string=self._run.options.force_version)
    incr_type, incr_target = self.DetermineBranchIncrParams(version_info)
    message = self.COMMIT_MESSAGE % {
        'target': incr_target,
        'branch': branch_ref,
    }
    self._IncrementVersionOnDisk(incr_type, push_to, message)

  def _IncrementVersionOnDiskForSourceBranch(self, overlay_dir, push_remote,
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
    self._FetchAndCheckoutTo(overlay_dir, push_to)

    # Use local branch ref.
    branch_ref = git.NormalizeRef(self.branch_name)
    tot_version_info = manifest_version.VersionInfo.from_repo(self._build_root)
    if (branch_ref.startswith('refs/heads/release-') or
        tot_version_info.VersionString() == self._run.options.force_version):
      incr_type, incr_target = self.DetermineSourceIncrParams(
          source_branch, branch_ref)
      message = self.COMMIT_MESSAGE % {
          'target': incr_target,
          'branch': branch_ref,
      }
      try:
        self._IncrementVersionOnDisk(incr_type, push_to, message)
      except cros_build_lib.RunCommandError:
        # There's a chance we are racing against the buildbots for this
        # increment.  We shouldn't quit the script because of this.  Instead, we
        # print a warning.
        self._FetchAndCheckoutTo(overlay_dir, push_to)
        new_version =  manifest_version.VersionInfo.from_repo(self._build_root)
        if new_version.VersionString() != tot_version_info.VersionString():
          logging.warning('Version number for branch %s was bumped by another '
                          'bot.', push_to.ref)
        else:
          raise

  def PerformStage(self):
    """Run the branch operation."""
    # Setup and initialize the repo.
    super(BranchUtilStage, self).PerformStage()

    repo_manifest = git.ManifestCheckout.Cached(self._build_root)
    checkouts = repo_manifest.ListCheckouts()

    cros_build_lib.Debug(
        'Processing %d checkouts from manifest in parallel.', len(checkouts))
    args = [[repo_manifest, x] for x in checkouts]
    parallel.RunTasksInProcessPool(self._ProcessCheckout, args, processes=16)

    if not self._run.options.delete_branch:
      self._FixUpManifests(repo_manifest)

    # Increment versions for a new branch.
    if not (self._run.options.delete_branch or self.rename_to):
      overlay_name = 'chromiumos/overlays/chromiumos-overlay'
      overlay_checkout = repo_manifest.FindCheckout(overlay_name)
      overlay_dir = overlay_checkout['local_path']
      push_remote = overlay_checkout['push_remote']
      self._IncrementVersionOnDiskForNewBranch(push_remote)

      source_branch = repo_manifest.default['revision']
      self._IncrementVersionOnDiskForSourceBranch(overlay_dir, push_remote,
                                                  source_branch)


class RefreshPackageStatusStage(bs.BuilderStage):
  """Stage for refreshing Portage package status in online spreadsheet."""
  def PerformStage(self):
    commands.RefreshPackageStatus(buildroot=self._build_root,
                                  boards=self._boards,
                                  debug=self._run.options.debug)


class InitSDKStage(bs.BuilderStage):
  """Stage that is responsible for initializing the SDK."""

  option_name = 'build'

  def __init__(self, builder_run, **kwargs):
    super(InitSDKStage, self).__init__(builder_run, **kwargs)
    self._env = {}
    if self._run.options.clobber:
      self._env['IGNORE_PREFLIGHT_BINHOST'] = '1'

    self._latest_toolchain = (self._run.config.latest_toolchain or
                              self._run.options.latest_toolchain)
    if self._latest_toolchain and self._run.config.gcc_githash:
      self._env['USE'] = 'git_gcc'
      self._env['GCC_GITHASH'] = self._run.config.gcc_githash

  def PerformStage(self):
    chroot_path = os.path.join(self._build_root, constants.DEFAULT_CHROOT_DIR)
    replace = self._run.config.chroot_replace
    pre_ver = post_ver = None
    if os.path.isdir(self._build_root) and not replace:
      try:
        pre_ver = cros_build_lib.GetChrootVersion(chroot=chroot_path)
        commands.RunChrootUpgradeHooks(self._build_root)
      except results_lib.BuildScriptFailure:
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
          extra_env=self._env)

    post_ver = cros_build_lib.GetChrootVersion(chroot=chroot_path)
    if pre_ver is not None and pre_ver != post_ver:
      cros_build_lib.PrintBuildbotStepText('%s->%s' % (pre_ver, post_ver))
    else:
      cros_build_lib.PrintBuildbotStepText(post_ver)


class SetupBoardStage(InitSDKStage):
  """Stage that is responsible for building host pkgs and setting up a board."""

  option_name = 'build'

  def __init__(self, builder_run, boards=None, **kwargs):
    super(SetupBoardStage, self).__init__(builder_run, **kwargs)
    if boards is not None:
      self._boards = boards

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
    for board_to_build in self._boards:
      # Only update the board if we need to do so.
      board_path = os.path.join(chroot_path, 'build', board_to_build)
      if os.path.isdir(board_path) and not chroot_upgrade:
        continue

      commands.SetupBoard(
          self._build_root, board=board_to_build, usepkg=usepkg,
          chrome_binhost_only=self._run.config.chrome_binhost_only,
          force=self._run.config.board_replace,
          extra_env=self._env, chroot_upgrade=chroot_upgrade,
          profile=self._run.options.profile or self._run.config.profile)
      chroot_upgrade = False

    commands.SetSharedUserPassword(
        self._build_root,
        password=self._run.config.shared_user_password)


class UprevStage(bs.BuilderStage):
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


class SyncChromeStage(bs.BuilderStage):
  """Stage that syncs Chrome sources if needed."""

  option_name = 'managed_chrome'

  def __init__(self, builder_run, **kwargs):
    super(SyncChromeStage, self).__init__(builder_run, **kwargs)
    # PerformStage() will fill this out for us.
    # TODO(mtennant): Replace with a run param.
    self.chrome_version = None

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
      cpv = None
      cros_build_lib.PrintBuildbotStepText('revision %s' % kwargs['revision'])
      self.chrome_version = self._run.options.chrome_version
    else:
      cpv = portage_utilities.BestVisible(constants.CHROME_CP,
                                          buildroot=self._build_root)
      kwargs['tag'] = cpv.version_no_rev.partition('_')[0]
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

  def _Finish(self):
    """Provide chrome_version to the rest of the run."""
    # Even if the stage failed, a None value for chrome_version still
    # means something.  In other words, this stage tried to run.
    self._run.attrs.chrome_version = self.chrome_version

    super(SyncChromeStage, self)._Finish()


class PatchChromeStage(bs.BuilderStage):
  """Stage that applies Chrome patches if needed."""

  option_name = 'rietveld_patches'

  def PerformStage(self):
    for patch in ' '.join(self._run.options.rietveld_patches).split():
      patch, colon, subdir = patch.partition(':')
      if not colon:
        subdir = 'src'
      commands.PatchChrome(self._run.options.chrome_root, patch, subdir)


class BuildPackagesStage(ArchivingStage):
  """Build Chromium OS packages."""

  option_name = 'build'
  def __init__(self, builder_run, board, archive_stage,
               pgo_generate=False, pgo_use=False, **kwargs):
    super(BuildPackagesStage, self).__init__(builder_run, board,
                                             archive_stage, **kwargs)
    self._pgo_generate, self._pgo_use = pgo_generate, pgo_use
    assert not pgo_generate or not pgo_use

    useflags = self._run.config.useflags[:]
    if pgo_generate:
      self.name += ' [%s]' % constants.USE_PGO_GENERATE
      useflags.append(constants.USE_PGO_GENERATE)
    elif pgo_use:
      self.name += ' [%s]' % constants.USE_PGO_USE
      useflags.append(constants.USE_PGO_USE)

    self._env = {}
    if useflags:
      self._env['USE'] = ' '.join(useflags)

    if self._run.options.chrome_root:
      self._env['CHROME_ORIGIN'] = 'LOCAL_SOURCE'

    if self._run.options.clobber:
      self._env['IGNORE_PREFLIGHT_BINHOST'] = '1'

    self._build_autotest = (self._run.config.build_tests and
                            self._run.options.tests)

  def _GetArchitectures(self):
    """Get the list of architectures built by this builder."""
    return set(self._GetPortageEnvVar('ARCH', b) for b in self._boards)

  def PerformStage(self):
    # Wait for PGO data to be ready if needed.
    if self._pgo_use:
      cpv = portage_utilities.BestVisible(constants.CHROME_CP,
                                          buildroot=self._build_root)
      commands.WaitForPGOData(self._GetArchitectures(), cpv)

    commands.Build(self._build_root,
                   self._current_board,
                   build_autotest=self._build_autotest,
                   usepkg=self._run.config.usepkg_build_packages,
                   chrome_binhost_only=self._run.config.chrome_binhost_only,
                   packages=self._run.config.packages,
                   skip_chroot_upgrade=True,
                   chrome_root=self._run.options.chrome_root,
                   extra_env=self._env)


class ChromeSDKStage(ArchivingStage):
  """Run through the simple chrome workflow."""

  def __init__(self, *args, **kwargs):
    super(ChromeSDKStage, self).__init__(*args, **kwargs)
    self._upload_queue = multiprocessing.Queue()
    self._pkg_dir = os.path.join(
        self._build_root, constants.DEFAULT_CHROOT_DIR,
        'build', self._current_board, 'var', 'db', 'pkg')
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
            goma=self._run.config.chrome_sdk_goma,
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
    images_to_build = set(self._run.config.images).intersection(
        images_can_build)

    version = self.archive_stage.release_tag
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
    if self._run.config.vm_tests and not self._pgo_generate:
      commands.BuildVMImageForTesting(
          self._build_root,
          self._current_board,
          disk_layout=self._run.config.disk_vm_layout,
          extra_env=self._env)

  def ArchivePayloads(self):
    """Archives update payloads when they are ready."""
    with osutils.TempDir(prefix='cbuildbot-payloads') as tempdir:
      with self.ArtifactUploader() as queue:
        if self._run.config.upload_hw_test_artifacts:
          if 'test' in self._run.config.images:
            image_name = 'chromiumos_test_image.bin'
          elif 'dev' in self._run.config.images:
            image_name = 'chromiumos_dev_image.bin'
          else:
            image_name = 'chromiumos_base_image.bin'

          logging.info('Generating payloads to upload for %s', image_name)
          image_path = os.path.join(self.GetImageDirSymlink(), image_name)
          # For non release builds, we are only interested in generating
          # payloads for the purpose of imaging machines. This means we
          # shouldn't generate delta payloads for n-1->n testing.
          # TODO: Add a config flag for generating delta payloads instead.
          if (self._run.config.build_type == constants.CANARY_TYPE and
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
      with self.ArtifactUploader(strict=True) as queue:
        cwd = os.path.abspath(
            os.path.join(self._build_root, 'chroot', 'build',
                         self._current_board, constants.AUTOTEST_BUILD_PATH,
                         '..'))

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
    if (self._run.config.upload_hw_test_artifacts or
        self._run.config.archive_build_debug) and self._build_autotest:
      steps.append(self._BuildAutotestTarballs)

    if self._run.config.images:
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
    with timeout_util.Timeout(self.SIGNER_TEST_TIMEOUT):
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
    if self._run.config.useflags:
      extra_env['USE'] = ' '.join(self._run.config.useflags)
    with timeout_util.Timeout(self.UNIT_TEST_TIMEOUT):
      commands.RunUnitTests(self._build_root,
                            self._current_board,
                            full=(not self._run.config.quick_unit),
                            blacklist=self._run.config.unittest_blacklist,
                            extra_env=extra_env)

    if os.path.exists(os.path.join(self.GetImageDirSymlink(),
                                   'au-generator.zip')):
      commands.TestAuZip(self._build_root,
                         self.GetImageDirSymlink())


class VMTestStage(ArchivingStage):
  """Run autotests in a virtual machine."""

  option_name = 'tests'
  config_name = 'vm_tests'

  def _ArchiveTestResults(self, test_results_dir, test_basename):
    """Archives test results to Google Storage."""
    test_tarball = commands.ArchiveTestResults(
        self._build_root, test_results_dir, test_basename)

    # Wait for breakpad symbols.
    got_symbols = False
    if self._run.options.archive:
      got_symbols = self.archive_stage.WaitForBreakpadSymbols()
    filenames = commands.GenerateStackTraces(
        self._build_root, self._current_board, test_tarball, self.archive_path,
        got_symbols)
    filenames.append(commands.ArchiveFile(test_tarball, self.archive_path))
    self._Upload(filenames)

  def _ArchiveVMFiles(self, test_results_dir):
    vm_files = commands.ArchiveVMFiles(
        self._build_root, os.path.join(test_results_dir, 'test_harness'),
        self.archive_path)
    # We use paths relative to |self.archive_path|, for prettier
    # formatting on the web page.
    self._Upload([os.path.basename(image) for image in vm_files])

  def _Upload(self, filenames):
    cros_build_lib.Info('Uploading artifacts to Google Storage...')
    with self.ArtifactUploader(archive=False, strict=False) as queue:
      for filename in filenames:
        queue.put([filename])
        if filename.endswith('.dmp.txt'):
          prefix = 'crash: '
        elif constants.VM_DISK_PREFIX in os.path.basename(filename):
          prefix = 'vm_disk: '
        elif constants.VM_MEM_PREFIX in os.path.basename(filename):
          prefix = 'vm_memory: '
        else:
          prefix = ''
        self.PrintDownloadLink(filename, prefix)

  def PerformStage(self):
    # These directories are used later to archive test artifacts.
    test_results_dir = commands.CreateTestRoot(self._build_root)
    test_basename = constants.VM_TEST_RESULTS % dict(attempt=self._attempt)
    try:
      test_type = self._run.config.vm_tests
      commands.RunTestSuite(self._build_root,
                            self._current_board,
                            self.GetImageDirSymlink(),
                            os.path.join(test_results_dir,
                                         'test_harness'),
                            test_type=test_type,
                            whitelist_chrome_crashes=self._chrome_rev is None,
                            archive_dir=self.bot_archive_root)

      if self._run.config.build_type == constants.CANARY_TYPE:
        commands.RunDevModeTest(
            self._build_root, self._current_board, self.GetImageDirSymlink())

    except Exception:
      cros_build_lib.Error(_VM_TEST_ERROR_MSG %
                           dict(vm_test_results=test_basename))
      self._ArchiveVMFiles(test_results_dir)
      raise
    finally:
      self._ArchiveTestResults(test_results_dir, test_basename)


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

  def __init__(self, builder_run, board, archive_stage, suite_config, **kwargs):
    super(HWTestStage, self).__init__(builder_run, board, archive_stage,
                                      suffix=' [%s]' % suite_config.suite,
                                      **kwargs)
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
    gs_context.Copy(gs_results_file, self._run.options.log_dir)
    # Prints out the actual result from gs_context.Copy.
    logging.info('Copy of %s completed. Printing below:', result_file_name)
    self._PrintFile(os.path.join(self._run.options.log_dir, result_file_name))

  def _CheckAborted(self):
    """Checks with GS to see if HWTest for this build's release_tag was aborted.

    Returns:
      True if HWTest have been aborted for this build's release_tag.
      False otherwise.
    """
    aborted = (self.archive_stage.release_tag and
               commands.HaveHWTestsBeenAborted(self.archive_stage.release_tag))
    return aborted

  # Disable complaint about calling _HandleStageException.
  # pylint: disable=W0212
  def _HandleStageException(self, exception):
    """Override and don't set status to FAIL but FORGIVEN instead."""

    # Deal with timeout errors specially.
    if isinstance(exception, timeout_util.TimeoutError):
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
      cros_build_lib.PrintBuildbotStepText('aborted')
      cros_build_lib.Warning('Skipping HWTests as they have been aborted.')
      return

    build = '/'.join([self._bot_id, self.version])
    if self._run.options.remote_trybot and self._run.options.hwtest:
      debug = self._run.options.debug_forced
    else:
      debug = self._run.options.debug
    lab_status.CheckLabStatus(self._current_board)
    with timeout_util.Timeout(
        self.suite_config.timeout  + constants.HWTEST_TIMEOUT_EXTENSION):
      commands.RunHWTestSuite(build,
                              self.suite_config.suite,
                              self._current_board,
                              self.suite_config.pool,
                              self.suite_config.num,
                              self.suite_config.file_bugs,
                              self.wait_for_results,
                              self.suite_config.priority,
                              self.suite_config.timeout_mins,
                              debug)

      if self.suite_config.copy_perf_results:
        self._SendPerfResults()


class SignerResultsException(Exception):
  """An expected failure from the SignerResultsStage."""


class SignerResultsTimeout(SignerResultsException):
  """The signer did not produce any results inside the expected time."""


class SignerFailure(SignerResultsException):
  """The signer returned an error result."""


class MissingInstructionException(SignerResultsException):
  """We didn't receive the list of signing instructions PushImage uploaded."""


class MalformedResultsException(SignerResultsException):
  """The Signer results aren't formatted as we expect."""


class SignerResultsStage(ArchivingStage):
  """Stage that blocks on and retrieves signer results."""

  option_name = 'signer_results'
  config_name = 'signer_results'

  # If the signing takes longer than 30 minutes, abort.
  SIGNING_TIMEOUT = 1800

  def __init__(self, *args, **kwargs):
    super(SignerResultsStage, self).__init__(*args, **kwargs)
    self.final_results = {}

  def _HandleStageException(self, exception):
    """Override and don't set status to FAIL but FORGIVEN instead."""
    # This stage is experimental, don't trust it yet.
    if isinstance(exception, SignerResultsException):
      return self._HandleExceptionAsWarning(exception)

    return super(SignerResultsStage, self)._HandleStageException(exception)


  def _JsonFromUrl(self, gs_ctx, url):
    """Fetch a GS Url, and parse it as Json.

    Args:
      gs_ctx: GS Context.
      url: Url to fetch and parse.

    Returns:
      None if the Url doesn't exist.
      Parsed Json structure if it did.

    Raises:
      MalformedResultsException if it failed to parse.
    """
    try:
      signer_txt = gs_ctx.Cat(url).output
    except gs.GSNoSuchKey:
      return None

    try:
      return json.loads(signer_txt)
    except ValueError:
      # We should never see malformed Json, even for intermediate statuses.
      raise MalformedResultsException(signer_txt)

  def _SigningStatusFromJson(self, signer_json):
    """Extract a signing status from a signer result Json DOM.

    Args:
      signer_json: The parsed json status from a signer operation.

    Returns:
      string with a simple status: 'passed', 'failed', 'downloading', etc,
      or '' if the json doesn't contain a status.
    """
    return signer_json.get('status', {}).get('status', '')

  def _CheckForResults(self, gs_ctx, result_urls):
    """timeout_util.WaitForSuccess func to check a list of signer results.

    Args:
      gs_ctx: Google Storage Context.
      result_urls: Urls of the signer result files we're expecting.

    Returns:
      Number of results not yet collected.
    """
    COMPLETED_STATUS = ('passed', 'failed')

    for url in result_urls:
      # We already have a result for this URL.
      if url in self.final_results:
        continue

      signer_json = self._JsonFromUrl(gs_ctx, url)
      if self._SigningStatusFromJson(signer_json) in COMPLETED_STATUS:
        # If we find a completed result, remember it.
        self.final_results[url] = signer_json

    # How many results are we still waiting on?
    remaining = len(result_urls) - len(self.final_results)
    return remaining

  def _Retry(self, remaining):
    """Should timeout_util.WaitForSuccess, try again?

    Args:
      remaining: number of incomplete signing operations.

    Returns:
      True to keep trying, False if we have all results.
    """
    return remaining > 0

  def PerformStage(self):
    """Do the work of waiting for signer results and logging them.

    Raises:
      ValueError: If the signer result isn't valid json.
      RunCommandError: If we are unable to download signer results.
    """
    # These results are expected to contain:
    # { 'channel': ['gs://instruction_uri1', 'gs://signer_instruction_uri2'] }
    instruction_urls_per_channel = self.archive_stage.WaitForPushImage()
    if not instruction_urls_per_channel:
      raise MissingInstructionException('No signer requests to validate.')

    # We will want seperate parallel handling for each channel in time, but for
    # now, just lump all of the channels together.
    result_urls = [
        '%s.json' % url for url in
        cros_build_lib.iflatten_instance(instruction_urls_per_channel.values())]

    gs_ctx = gs.GSContext(dry_run=self.debug)

    try:
      cros_build_lib.Info('Waiting for signer results.')
      timeout_util.WaitForSuccess(self._Retry, self._CheckForResults,
                                  func_args=(gs_ctx, result_urls),
                                  timeout=self.SIGNING_TIMEOUT, period=30)
    except timeout_util.TimeoutError:
      msg = 'Image signing timed out.'
      cros_build_lib.Error(msg)
      cros_build_lib.PrintBuildbotStepText(msg)
      raise SignerResultsTimeout(msg)

    # Log all signer results, then handle any signing failures.
    failures = []
    for url, signer_result in self.final_results.iteritems():
      result_description = os.path.basename(url)
      cros_build_lib.PrintBuildbotStepText(result_description)
      cros_build_lib.Info('Received results for: %s', result_description)
      cros_build_lib.Info(json.dumps(signer_result, indent=4))

      status = self._SigningStatusFromJson(signer_result)
      if status != 'passed':
        failures.append(result_description)
        cros_build_lib.Error('Signing failed for: %s', result_description)

    if failures:
      cros_build_lib.Error('Failure summary:')
      for failure in failures:
        cros_build_lib.Error('  %s', failure)
      raise SignerFailure(failures)


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

  def __init__(self, *args, **kwargs):
    super(ASyncHWTestStage, self).__init__(*args, **kwargs)
    self.wait_for_results = False


class QATestStage(HWTestStage, ForgivingBuilderStage):
  """Stage that runs qav suite in lab, blocking build but forgiving failures.
  """

  def __init__(self, *args, **kwargs):
    super(QATestStage, self).__init__(*args, **kwargs)


class SDKPackageStage(bs.BuilderStage):
  """Stage that performs preparing and packaging SDK files"""

  # Version of the Manifest file being generated. Should be incremented for
  # Major format changes.
  MANIFEST_VERSION = '1'
  _EXCLUDED_PATHS = ('usr/lib/debug', constants.AUTOTEST_BUILD_PATH,
                     'packages', 'tmp')

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
  def __init__(self, builder_run, board, chrome_version=None, **kwargs):
    super(ArchiveStage, self).__init__(builder_run, board, self, **kwargs)
    self.chrome_version = chrome_version

    # TODO(mtennant): Places that use this release_tag attribute should
    # move to use self._run.attrs.release_tag directly.
    self.release_tag = getattr(self._run.attrs, 'release_tag', None)

    self._breakpad_symbols_queue = multiprocessing.Queue()
    self._recovery_image_status_queue = multiprocessing.Queue()
    self._release_upload_queue = multiprocessing.Queue()
    self._upload_queue = multiprocessing.Queue()
    self._push_image_status_queue = multiprocessing.Queue()

    # Setup the archive path. This is used by other stages.
    self._SetupArchivePath()

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

  def WaitForPushImage(self):
    """Wait until PushImage compeletes.

    Returns:
      On success: The pushimage results.
      None otherwise.
    """
    cros_build_lib.Info('Waiting for PushImage...')
    urls = self._push_image_status_queue.get()
    # Put the status back so other processes don't starve.
    self._push_image_status_queue.put(urls)
    return urls

  def BreakpadSymbolsGenerated(self, success):
    """Signal that breakpad symbols have been generated.

    Args:
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
    if self._run.options.buildbot:
      # Buildbot: Clear out any leftover build artifacts, if present.
      osutils.RmDir(self.archive_path, ignore_missing=True)
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

    # If chrome is not installed, skip archiving.
    chroot_path = os.path.join(self._build_root, constants.DEFAULT_CHROOT_DIR)
    board_path = os.path.join(chroot_path, 'build', self._current_board)
    if not portage_utilities.IsPackageInstalled(constants.CHROME_CP,
                                                board_path):
      return

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
    if self._run.config.useflags:
      extra_env['USE'] = ' '.join(self._run.config.useflags)
    in_chroot_path = git.ReinterpretPathForChroot(self.archive_path)
    cmd = ['generate_delta_sysroot', '--out-dir', in_chroot_path,
           '--board', self._current_board]
    # TODO(mtennant): Make this condition into one run param.
    if not self._run.config.build_tests or not self._run.options.tests:
      cmd.append('--skip-tests')
    cros_build_lib.RunCommand(cmd, cwd=self._build_root, enter_chroot=True,
                              extra_env=extra_env)
    self._upload_queue.put([constants.DELTA_SYSROOT_TAR])

  def PerformStage(self):
    buildroot = self._build_root
    config = self._run.config
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
          if os.path.basename(image_file) != 'chromiumos_qemu_image.bin':
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
      # TODO(mtennant): Make this autotest_built concept into a run param.
      autotest_built = config['build_tests'] and self._run.options.tests and (
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
      urls = commands.PushImages(
          board=board,
          archive_url=upload_url,
          dryrun=debug or not config['push_image'],
          profile=self._run.options.profile or config['profile'],
          sign_types=sign_types)
      self._push_image_status_queue.put(urls)


    def ArchiveReleaseArtifacts():
      with self.ArtifactUploader(self._release_upload_queue, archive=False):
        steps = [BuildAndArchiveAllImages, ArchiveFirmwareImages]
        parallel.RunParallelSteps(steps)
      PushImage()

    def BuildAndArchiveArtifacts():
      # Run archiving steps in parallel.
      steps = [ArchiveReleaseArtifacts]
      if config['images']:
        steps.extend([self.ArchiveStrippedChrome, ArchiveImageScripts])
      if config['create_delta_sysroot']:
        steps.append(self.BuildAndArchiveDeltaSysroot)

      with self.ArtifactUploader(self._upload_queue, archive=False):
        parallel.RunParallelSteps(steps)

    def MarkAsLatest():
      # Update and upload LATEST file.
      verinfo = self._run.GetVersionInfo(self._build_root)
      calc_version = self.release_tag or verinfo.VersionString()
      filenames = ('LATEST-%s' % self._run.manifest_branch,
                   'LATEST-%s' % calc_version)
      for filename in filenames:
        latest_path = os.path.join(self.bot_archive_root, filename)
        osutils.WriteFile(latest_path, self.version, mode='w')
        commands.UploadArchivedFile(
            self.bot_archive_root, self.base_upload_url, filename,
            debug, acl=self.acl)

    try:
      if not self._run.config.pgo_generate:
        BuildAndArchiveArtifacts()
        MarkAsLatest()
    finally:
      commands.RemoveOldArchives(self.bot_archive_root,
                                 self._run.options.max_archive_builds)

  def _HandleStageException(self, exception):
    # Tell the HWTestStage not to wait for artifacts to be uploaded
    # in case ArchiveStage throws an exception.
    self._recovery_image_status_queue.put(False)
    self._push_image_status_queue.put(None)
    return super(ArchiveStage, self)._HandleStageException(exception)


class DebugSymbolsStage(ArchivingStage):
  """Handles generation & upload of debug symbols."""

  def PerformStage(self):
    """Generate debug symbols and upload debug.tgz."""
    buildroot = self._build_root
    config = self._run.config
    board = self._current_board
    debug = self.debug
    archive_path = self.archive_path

    if config['archive_build_debug'] or config['vm_tests']:
      self._GenerateBreakpadSymbols(buildroot, board, debug)

      # Kick off the symbol upload process in the background.
      if config['upload_symbols']:
        self.UploadSymbols(buildroot, board)

      # Generate and upload tarball.
      with self.ArtifactUploader(archive=False) as queue:
        filename = commands.GenerateDebugTarball(
            buildroot, board, archive_path,
            config['archive_build_debug'])
        queue.put([filename])
    else:
      self.archive_stage.BreakpadSymbolsGenerated(False)

  def UploadSymbols(self, buildroot, board):
    """Upload generated debug symbols."""
    if self._run.options.remote_trybot or self.debug:
      # For debug builds, limit ourselves to just uploading 1 symbol.
      # This way trybots and such still exercise this code.
      cnt = 1
      official = False
    else:
      cnt = None
      official = self._run.config['chromeos_official']
    commands.UploadSymbols(buildroot, board, official, cnt)

  def _GenerateBreakpadSymbols(self, buildroot, board, debug):
    """Generate the breakpad symbols"""
    success = False
    try:
      commands.GenerateBreakpadSymbols(buildroot, board, debug)
      success = True
    finally:
      self.archive_stage.BreakpadSymbolsGenerated(success)


class MasterUploadPrebuiltsStage(bs.BuilderStage):
  """Syncs prebuilt binhost files across slaves."""
  # TODO(mtennant): This class represents logic spun out from
  # UploadPrebuiltsStage that is specific to a master builder.  It will
  # be used by commit queue to start, but could be used by other master
  # builders that upload prebuilts.  Current examples are
  # x86-alex-pre-flight-branch and x86-generic-chromium-pfq.  When
  # completed the UploadPrebuiltsStage code can be thinned significantly.
  option_name = 'prebuilts'
  config_name = 'prebuilts'

  def _GenerateCommonArgs(self):
    """Generate common prebuilt arguments."""
    generated_args = []
    if self._run.options.debug:
      generated_args.append('--debug')

    profile = self._run.options.profile or self._run.config['profile']
    if profile:
      generated_args.extend(['--profile', profile])

    # Generate the version if we are a manifest_version build.
    if self._run.config.manifest_version:
      version = self._run.GetVersion()
      generated_args.extend(['--set-version', version])

    return generated_args

  @staticmethod
  def _AddOptionsForSlave(slave_config):
    """Private helper method to add upload_prebuilts args for a slave builder.

    Args:
      slave_config: The build config of a slave builder.

    Returns:
      An array of options to add to upload_prebuilts array that allow a master
      to submit prebuilt conf modifications on behalf of a slave.
    """
    args = []
    if slave_config['prebuilts']:
      for slave_board in slave_config['boards']:
        args.extend(['--slave-board', slave_board])
        slave_profile = slave_config['profile']
        if slave_profile:
          args.extend(['--slave-profile', slave_profile])

    return args

  def PerformStage(self):
    """Syncs prebuilt binhosts for slave builders."""
    # Common args we generate for all types of builds.
    generated_args = self._GenerateCommonArgs()
    # Args we specifically add for public/private build types.
    public_args, private_args = [], []
    # Gather public/private (slave) builders.
    public_builders, private_builders = [], []

    # Distributed builders that use manifest-versions to sync with one another
    # share prebuilt logic by passing around versions.
    assert cbuildbot_config.IsPFQType(self._prebuilt_type)
    assert self._prebuilt_type != constants.CHROME_PFQ_TYPE

    # Public pfqs should upload host preflight prebuilts.
    public_args.append('--sync-host')

    # Update all the binhost conf files.
    generated_args.append('--sync-binhost-conf')
    for slave_config in self._GetSlavesForMaster(self._run.config):
      if slave_config['prebuilts'] == constants.PUBLIC:
        public_builders.append(slave_config['name'])
        public_args.extend(self._AddOptionsForSlave(slave_config))
      elif slave_config['prebuilts'] == constants.PRIVATE:
        private_builders.append(slave_config['name'])
        private_args.extend(self._AddOptionsForSlave(slave_config))

    # Upload the public prebuilts, if any.
    if public_builders:
      commands.UploadPrebuilts(
          category=self._prebuilt_type, chrome_rev=self._chrome_rev,
          private_bucket=False, buildroot=self._build_root, board=None,
          extra_args=generated_args + public_args)

    # Upload the private prebuilts, if any.
    if private_builders:
      commands.UploadPrebuilts(
          category=self._prebuilt_type, chrome_rev=self._chrome_rev,
          private_bucket=True, buildroot=self._build_root, board=None,
          extra_args=generated_args + private_args)


class UploadPrebuiltsStage(BoardSpecificBuilderStage):
  """Uploads binaries generated by this build for developer use."""

  option_name = 'prebuilts'
  config_name = 'prebuilts'

  def __init__(self, builder_run, board, archive_stage, **kwargs):
    super(UploadPrebuiltsStage, self).__init__(builder_run, board, **kwargs)
    # TODO(mtennant): I think this self._archive_stage is already unused.
    self._archive_stage = archive_stage

  def GenerateCommonArgs(self):
    """Generate common prebuilt arguments."""
    generated_args = []
    if self._run.options.debug:
      generated_args.append('--debug')

    profile = self._run.options.profile or self._run.config.profile
    if profile:
      generated_args.extend(['--profile', profile])

    # Generate the version if we are a manifest_version build.
    if self._run.config.manifest_version:
      assert self._archive_stage, 'Archive stage missing for versioned build.'
      version = self._run.GetVersion()
      generated_args.extend(['--set-version', version])

    if self._run.config.git_sync:
      # Git sync should never be set for pfq type builds.
      assert not cbuildbot_config.IsPFQType(self._prebuilt_type)
      generated_args.extend(['--git-sync'])

    return generated_args

  @classmethod
  def _AddOptionsForSlave(cls, slave_config, board):
    """Private helper method to add upload_prebuilts args for a slave builder.

    Args:
      slave_config: The build config of a slave builder.
      board: The name of the "master" board on the master builder.

    Returns:
      An array of options to add to upload_prebuilts array that allow a master
      to submit prebuilt conf modifications on behalf of a slave.
    """
    args = []
    if slave_config['prebuilts']:
      for slave_board in slave_config['boards']:
        if slave_config['master'] and slave_board == board:
          # Ignore self.
          continue

        args.extend(['--slave-board', slave_board])
        slave_profile = slave_config['profile']
        if slave_profile:
          args.extend(['--slave-profile', slave_profile])

    return args

  def PerformStage(self):
    """Uploads prebuilts for master and slave builders."""
    prebuilt_type = self._prebuilt_type
    board = self._current_board
    binhosts = []

    # Whether we publish public or private prebuilts.
    public = self._run.config.prebuilts == constants.PUBLIC
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

      if self._run.config.master and board == self._boards[-1]:
        # The master builder updates all the binhost conf files, and needs to do
        # so only once so as to ensure it doesn't try to update the same file
        # more than once. As multiple boards can be built on the same builder,
        # we arbitrarily decided to update the binhost conf files when we run
        # upload_prebuilts for the last board. The other boards are treated as
        # slave boards.
        generated_args.append('--sync-binhost-conf')
        for c in self._GetSlavesForMaster(self._run.config):
          if c['prebuilts'] == constants.PUBLIC:
            public_builders.append(c['name'])
            public_args.extend(self._AddOptionsForSlave(c, board))
          elif c['prebuilts'] == constants.PRIVATE:
            private_builders.append(c['name'])
            private_args.extend(self._AddOptionsForSlave(c, board))

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
  """Stage that uploads DevInstaller prebuilts."""

  config_name = 'dev_installer_prebuilts'

  def PerformStage(self):
    generated_args = generated_args = self.GenerateCommonArgs()
    commands.UploadDevInstallerPrebuilts(
        binhost_bucket=self._run.config.binhost_bucket,
        binhost_key=self._run.config.binhost_key,
        binhost_base_url=self._run.config.binhost_base_url,
        buildroot=self._build_root,
        board=self._current_board,
        extra_args=generated_args)


class PublishUprevChangesStage(bs.BuilderStage):
  """Makes uprev changes from pfq live for developers."""

  def __init__(self, builder_run, success, **kwargs):
    """Constructor.

    Args:
      builder_run: BuilderRun object.
      success: Boolean indicating whether the build succeeded.
    """
    super(PublishUprevChangesStage, self).__init__(builder_run, **kwargs)
    self.success = success

  def PerformStage(self):
    overlays, push_overlays = self._ExtractOverlays()
    assert push_overlays, 'push_overlays must be set to run this stage'

    # If the build failed, we don't want to push our local changes, because
    # they might include some CLs that failed. Instead, clean up our local
    # changes and do a fresh uprev.
    if not self.success:
      # Clean up our root and sync down the latest changes that were
      # submitted.
      commands.BuildRootGitCleanup(self._build_root)

      # Sync down the latest changes we have submitted.
      if self._run.options.sync:
        next_manifest = self._run.config.manifest
        repo = self.GetRepoRepository()
        repo.Sync(next_manifest)

      # Commit an uprev locally.
      if self._run.options.uprev and self._run.config.uprev:
        commands.UprevPackages(self._build_root, self._boards, overlays)

    # Push the uprev commit.
    commands.UprevPush(self._build_root, push_overlays, self._run.options.debug)


class ReportStage(bs.BuilderStage):
  """Summarize all the builds."""

  _HTML_HEAD = """<html>
<head>
 <title>Archive Index: %(board)s / %(version)s</title>
</head>
<body>
<h2>Artifacts Index: %(board)s / %(version)s (%(config)s config)</h2>"""

  def __init__(self, builder_run, archive_stages, sync_instance,
               completion_instance=None, **kwargs):
    super(ReportStage, self).__init__(builder_run, **kwargs)
    # TODO(mtennant): All these should be retrieved from builder_run instead.
    # Or, more correctly, the info currently retrieved from these stages should
    # be stored and retrieved from builder_run instead.
    self._archive_stages = archive_stages
    self._sync_instance = sync_instance
    self._completion_instance = completion_instance

    # TODO(mtennant): In the future, we should avoid accessing run attributes
    # in stage constructors, but instead do so at PerformStage time.  This
    # is so that the Builder can construct all stage objects for a flow
    # and then run them.  In other words, a stage object should not assume
    # that it is ready to run at construction time.
    self._version = getattr(self._run.attrs, 'release_tag', '')

  def _UpdateStreakCounter(self, final_status, counter_name,
                           dry_run=False):
    """Update the given streak counter based on the final status of build.

    A streak counter counts the number of consecutive passes or failures of
    a particular builder. Consecutive passes are indicated by a positive value,
    consecutive failures by a negative value.

    Args:
      final_status: String indicating final status of build,
                    constants.FINAL_STATUS_PASSED indicating success.
      counter_name: Name of counter to increment, typically the name of the
                    build config.
      dry_run: Pretend to update counter only. Default: False.

    Returns:
      The new value of the streak counter.
    """
    gs_ctx = gs.GSContext(dry_run=dry_run)
    counter_url = os.path.join(constants.MANIFEST_VERSIONS_GS_URL,
                               constants.STREAK_COUNTERS,
                               counter_name)
    gs_counter = gs.GSCounter(gs_ctx, counter_url)

    if final_status == constants.FINAL_STATUS_PASSED:
      streak_value = gs_counter.StreakIncrement()
    else:
      streak_value = gs_counter.StreakDecrement()

    return streak_value

  def _HealthAlertMessage(self):
    """Returns the body of a health alert email message."""
    return 'The builder named %s has failed %i consecutive times. See %s' % (
        self._run.config['name'], self._run.config['health_threshold'],
        self.ConstructDashboardURL())

  def PerformStage(self):
    acl = ArchivingStage.GetUploadACL(self._run.config)
    archive_urls = {}

    debug = self._run.debug

    archive = self._run.GetArchive()
    download_url = archive.download_url
    archive_path = archive.archive_path
    upload_url = archive.upload_url

    for builder_run in self._run.GetUngroupedBuilderRuns():
      config = builder_run.config

      # Soon neither a board nor an archive stage will be strictly required
      # to run the code below, but for now the requirement remains in place.
      # TODO(mtennant): boards = config.boards or [None]
      boards = config.boards
      for board in boards:
        # This is temporary.  We are almost done with the need to have access
        # to the archive stage object, but for now fetch it.
        archive_stage = None
        for board_config in self._archive_stages:
          if board_config.board == board and board_config.name == config.name:
            archive_stage = self._archive_stages[board_config]

        # Without an Archive stage there is no archive information to prepare.
        if not archive_stage:
          continue

        # TODO(mtennant): Move this block to new self.UploadMetadata, and
        # move the logic from ArchiveStage.UploadMetadata there.
        # Generate the final metadata before we look at the uploaded list.
        if results_lib.Results.BuildSucceededSoFar():
          final_status = constants.FINAL_STATUS_PASSED
        else:
          final_status = constants.FINAL_STATUS_FAILED
        archive_stage.UploadMetadata(
            final_status=final_status, sync_instance=self._sync_instance,
            completion_instance=self._completion_instance)

        # TODO(mtennant): Move this block to new self.UpdateStreak.
        # If this was a Commit Queue build, update the streak counter
        if (self._sync_instance and
            isinstance(self._sync_instance, CommitQueueSyncStage)):
          streak_value = self._UpdateStreakCounter(
              final_status=final_status, counter_name=builder_run.config.name,
              dry_run=debug)
          if (builder_run.config['health_alert_recipients'] and
              streak_value == -builder_run.config['health_threshold']):
            logging.info('Builder failed %i consecutive times, sending health '
                         'alert email to %s.',
                         builder_run.config['health_threshold'],
                         builder_run.config['health_alert_recipients'])
            alerts.SendEmail('%s health alert' % builder_run.config['name'],
                             builder_run.config['health_alert_recipients'],
                             message=self._HealthAlertMessage(),
                             smtp_server=constants.GOLO_SMTP_SERVER,
                             extra_fields={'X-cbuildbot-alert': 'cq-health'})

        # TODO(mtennant: Move the rest to new self.UploadArchiveIndex.
        # Generate the index page needed for public reading.
        uploaded = os.path.join(archive_path, commands.UPLOADED_LIST_FILENAME)
        if not os.path.exists(uploaded):
          # UPLOADED doesn't exist.  Normal if buildboard failed.
          if (board and not self._run.config.compilecheck and
              not self._run.options.compilecheck):
            logging.warning('Board %s did not make it to the archive stage; '
                            'skipping', board)

          # No HTML index is needed.
          continue

        # Prepare html head.
        head_data = {
            'board': board,
            'config': builder_run.config.name,
            'version': builder_run.GetVersion(),
        }
        head = self._HTML_HEAD % head_data

        files = osutils.ReadFile(uploaded).splitlines() + [
            '.|Google Storage Index',
            '..|',
        ]
        index = os.path.join(archive_path, 'index.html')
        commands.GenerateHtmlIndex(index, files, url_base=download_url,
                                   head=head)
        commands.UploadArchivedFile(
            archive_path, upload_url, os.path.basename(index), debug=debug,
            acl=acl)

        archive_urls[board] = download_url + '/index.html'

    results_lib.Results.Report(sys.stdout, archive_urls=archive_urls,
                               current_version=self._version)
