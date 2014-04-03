# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing the various stages that a builder runs."""

import contextlib
import datetime
import functools
import glob
import itertools
import json
import logging
import multiprocessing
import os
import platform
import re
import shutil
import sys
import time
from xml.etree import ElementTree

from chromite.buildbot import builderstage as bs
from chromite.buildbot import cbuildbot_commands as commands
from chromite.buildbot import cbuildbot_config
from chromite.buildbot import cbuildbot_metadata
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

CQ_HWTEST_WAS_ABORTED = ('HWTest was aborted, because another commit '
                         'queue builder failed outside of HWTest.')

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

  def _HandleStageException(self, exc_info):
    """Override and don't set status to FAIL but FORGIVEN instead."""
    return self._HandleExceptionAsWarning(exc_info)


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
  """Builder stage that is specific to a board.

  The following attributes are provided on self:
    _current_board: The active board for this stage.
    board_runattrs: BoardRunAttributes object for this stage.
  """

  def __init__(self, builder_run, board, **kwargs):
    super(BoardSpecificBuilderStage, self).__init__(builder_run, **kwargs)
    self._current_board = board

    self.board_runattrs = builder_run.GetBoardRunAttrs(board)

    if not isinstance(board, basestring):
      raise TypeError('Expected string, got %r' % (board,))

    # Add a board name suffix to differentiate between various boards (in case
    # more than one board is built on a single builder.)
    if len(self._boards) > 1 or self._run.config.grouped:
      self.name = '%s [%s]' % (self.name, board)

  def _RecordResult(self, *args, **kwargs):
    """Record a successful or failed result."""
    kwargs.setdefault('board', self._current_board)
    super(BoardSpecificBuilderStage, self)._RecordResult(*args, **kwargs)

  def GetParallel(self, board_attr, timeout=None, pretty_name=None):
    """Wait for given |board_attr| to show up.

    Args:
      board_attr: A valid board runattribute name.
      timeout: Timeout in seconds.  None value means wait forever.
      pretty_name: Optional name to use instead of raw board_attr in
        log messages.

    Returns:
      Value of board_attr found.

    Raises:
      AttrTimeoutError if timeout occurs.
    """
    timeout_str = 'forever'
    if timeout is not None:
      timeout_str = '%d minutes' % int((timeout / 60) + 0.5)

    if pretty_name is None:
      pretty_name = board_attr

    cros_build_lib.Info('Waiting up to %s for %s ...', timeout_str, pretty_name)
    return self.board_runattrs.GetParallel(board_attr, timeout=timeout)

  def GetImageDirSymlink(self, pointer='latest-cbuildbot'):
    """Get the location of the current image."""
    return os.path.join(self._run.buildroot, 'src', 'build', 'images',
                        self._current_board, pointer)


class ArchivingStageMixin(object):
  """Stage with utilities for uploading artifacts.

  This provides functionality for doing archiving.  All it needs is access
  to the BuilderRun object at self._run.  No __init__ needed.

  Attributes:
    acl: GS ACL to use for uploads.
    archive: Archive object.
    archive_path: Local path where archives are kept for this run.  Also copy
      of self.archive.archive_path.
    download_url: The URL where artifacts for this run can be downloaded.
      Also copy of self.archive.download_url.
    upload_url: The Google Storage location where artifacts for this run should
      be uploaded.  Also copy of self.archive.upload_url.
    version: Copy of self.archive.version.
  """

  PROCESSES = 10

  @property
  def archive(self):
    """Retrieve the Archive object to use."""
    # pylint: disable=W0201
    if not hasattr(self, '_archive'):
      self._archive = self._run.GetArchive()

    return self._archive

  @property
  def acl(self):
    """Retrieve GS ACL to use for uploads."""
    return self.archive.upload_acl

  # TODO(mtennant): Get rid of this property.
  @property
  def version(self):
    """Retrieve the ChromeOS version for the archiving."""
    return self.archive.version

  @property
  def archive_path(self):
    """Local path where archives are kept for this run."""
    return self.archive.archive_path

  # TODO(mtennant): Rename base_archive_path.
  @property
  def bot_archive_root(self):
    """Path of directory one level up from self.archive_path."""
    return os.path.dirname(self.archive_path)

  @property
  def upload_url(self):
    """The GS location where artifacts should be uploaded for this run."""
    return self.archive.upload_url

  @property
  def base_upload_url(self):
    """The GS path one level up from self.upload_url."""
    return os.path.dirname(self.upload_url)

  @property
  def download_url(self):
    """The URL where artifacts for this run can be downloaded."""
    return self.archive.download_url

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

  def PrintDownloadLink(self, filename, prefix='', text_to_display=None):
    """Print a link to an artifact in Google Storage.

    Args:
      filename: The filename of the uploaded file.
      prefix: The prefix to put in front of the filename.
      text_to_display: Text to display. If None, use |prefix| + |filename|.
    """
    url = '%s/%s' % (self.download_url.rstrip('/'), filename)
    if not text_to_display:
      text_to_display = '%s%s' % (prefix, filename)
    cros_build_lib.PrintBuildbotLink(text_to_display, url)

  def _GetUploadUrls(self):
    """Returns a list of all urls to upload artifacts to."""
    urls = [self.upload_url]
    if hasattr(self, '_current_board'):
      custom_artifacts_file = portage_utilities.ReadOverlayFile(
          'scripts/artifacts.json', board=self._current_board)
      if custom_artifacts_file is not None:
        json_file = json.loads(custom_artifacts_file)
        for url in json_file.get('extra_upload_urls', []):
          urls.append('/'.join([url, self._bot_id, self.version]))
    return urls

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
      for url in self._GetUploadUrls():
        commands.UploadArchivedFile(
            self.archive_path, url, filename, self._run.debug,
            update_list=True, acl=self.acl)
    except (cros_build_lib.RunCommandError, timeout_util.TimeoutError):
      cros_build_lib.PrintBuildbotStepText('Upload failed')
      if strict:
        raise

      # Treat gsutil flake as a warning if it's the only problem.
      self._HandleExceptionAsWarning(sys.exc_info())

  def GetMetadata(self, config=None, stage=None, final_status=None,
                  sync_instance=None, completion_instance=None):
    """Constructs the metadata json object.

    Args:
      config: The build config for this run.  Defaults to self._run.config.
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
    builder_run = self._run
    build_root = self._build_root
    config = config or builder_run.config

    commit_queue_stages = (CommitQueueSyncStage, PreCQSyncStage)
    get_changes_from_pool = (sync_instance and
                             isinstance(sync_instance, commit_queue_stages) and
                             sync_instance.pool)

    get_statuses_from_slaves = (config['master'] and
                                completion_instance and
                                isinstance(completion_instance,
                                           MasterSlaveSyncCompletionStage))

    return cbuildbot_metadata.CBuildbotMetadata.GetMetadataDict(
        builder_run, build_root, get_changes_from_pool,
        get_statuses_from_slaves, config, stage, final_status, sync_instance,
        completion_instance)

  def UploadMetadata(self, config=None, stage=None, upload_queue=None,
                     **kwargs):
    """Create and upload JSON of various metadata describing this run.

    Args:
      config: Build config to use.  Passed to GetMetadata.
      stage: Stage to upload metadata for.  If None the metadata is for the
        entire run.
      upload_queue: If specified then put the artifact file to upload on
        this queue.  If None then upload it directly now.
      kwargs: Pass to self.GetMetadata.
    """
    metadata_for = '[no config]'
    if config is not None:
      metadata_for = config.name
      if stage is not None:
        metadata_for = '%s:%s' % (metadata_for, stage)

    cros_build_lib.Info('Creating metadata for %s run now.', metadata_for)
    metadata_dict = self.GetMetadata(config=config, stage=stage, **kwargs)
    self._run.attrs.metadata.UpdateWithDict(metadata_dict)

    filename = constants.METADATA_JSON
    if stage is not None:
      filename = constants.METADATA_STAGE_JSON % { 'stage': stage }
    metadata_json = os.path.join(self.archive_path, filename)

    # Stages may run in parallel, so we have to do atomic updates on this.
    cros_build_lib.Info('Writing metadata for %s to %s.', metadata_for,
                        metadata_json)
    osutils.WriteFile(metadata_json, self._run.attrs.metadata.GetJSON(),
                      atomic=True)

    if upload_queue is not None:
      cros_build_lib.Info('Adding metadata for %s to upload queue.',
                          metadata_for)
      upload_queue.put([filename])
    else:
      cros_build_lib.Info('Uploading metadata for %s now.', metadata_for)
      self.UploadArtifact(filename, archive=False)


# TODO(mtennant): This class continues to exist only for subclasses that still
# need self.archive_stage.  Hopefully, we can get rid of that need, eventually.
class ArchivingStage(BoardSpecificBuilderStage, ArchivingStageMixin):
  """Helper for stages that archive files.

  See ArchivingStageMixin for functionality.

  Attributes:
    archive_stage: The ArchiveStage instance for this board.
  """

  def __init__(self, builder_run, board, archive_stage, **kwargs):
    super(ArchivingStage, self).__init__(builder_run, board, **kwargs)
    self.archive_stage = archive_stage


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
      self.HandleApplyFailures(failures)

  def HandleApplyFailures(self, failures):
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

    # TODO(davidjames): This code actually takes external manifest patches and
    # tries to apply them to the internal manifest. We shouldn't do this.
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

  def HandleApplyFailures(self, failures):
    """Handle the case where patches fail to apply."""
    if self._run.options.pre_cq or self._run.config.pre_cq:
      # Let the PreCQSync stage handle this failure. The PreCQSync stage will
      # comment on CLs with the appropriate message when they fail to apply.
      #
      # WARNING: For manifest patches, the Pre-CQ attempts to apply external
      # patches to the internal manifest, and this means we may flag a conflict
      # here even if the patch applies cleanly. TODO(davidjames): Fix this.
      cros_build_lib.PrintBuildbotStepWarnings()
      cros_build_lib.Error('Failed applying patches: %s',
                           '\n'.join(map(str, failures)))
    else:
      PatchChangesStage.HandleApplyFailures(self, failures)

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
      build_names=self._run.GetBuilderIds(),
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
        build_names=self._run.GetBuilderIds(),
        incr_type=self.VersionIncrementType(),
        force=self._force,
        branch=self._run.manifest_branch,
        dry_run=dry_run,
        master=self._run.config.master))

  def GetNextManifest(self):
    """Uses the initialized manifest manager to get the next manifest."""
    assert self.manifest_manager, \
        'Must run GetStageManager before checkout out build.'

    to_return = self.manifest_manager.GetNextBuildSpec(
        dashboard_url=self.ConstructDashboardURL())
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
        build_names=self._run.GetBuilderIds(),
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
    if (self._run.config.master and self._GetSlaveConfigs()):
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
        MasterSlaveSyncStage.sub_manager.CreateFromManifest(
            manifest, dashboard_url=self.ConstructDashboardURL())
      return manifest
    else:
      return self.manifest_manager.GetLatestCandidate(
          dashboard_url=self.ConstructDashboardURL())


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
      self.pool = validation_pool.ValidationPool.Load(filename,
          metadata=self._run.attrs.metadata)
    else:
      self._SetPoolFromManifest(self.manifest_manager.GetLocalManifest())

    # Because metadata is currently not surviving cbuildbot re-execution,
    # re-record that patches were picked up in the non-skipped run of
    # CommitQueueSync.
    # TODO(akeshet): Remove this code once metadata is being pickled and passed
    # across re-executions. See crbug.com/356930
    if self._run.config.master:
      self._RecordPatchesInMetadata(self.pool)

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
        self._run.config.master, self._run.options.debug,
        metadata=self._run.attrs.metadata)

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
            change_filter=self._ChangeFilter, throttled_ok=True,
            metadata=self._run.attrs.metadata)

        self._RecordPatchesInMetadata(self.pool)

      except validation_pool.TreeIsClosedException as e:
        cros_build_lib.Warning(str(e))
        return None

      manifest = self.manifest_manager.CreateNewCandidate(validation_pool=pool)
      if MasterSlaveSyncStage.sub_manager:
        MasterSlaveSyncStage.sub_manager.CreateFromManifest(
            manifest, dashboard_url=self.ConstructDashboardURL())

      return manifest
    else:
      manifest = self.manifest_manager.GetLatestCandidate(
          dashboard_url=self.ConstructDashboardURL())
      if manifest:
        if self._run.config.build_before_patching:
          pre_build_passed = self._RunPrePatchBuild()
          cros_build_lib.PrintBuildbotStepName(
              'CommitQueueSync : Apply Patches')
          if not pre_build_passed:
            cros_build_lib.PrintBuildbotStepText('Pre-patch build failed.')

        self._SetPoolFromManifest(manifest)
        self.pool.ApplyPoolIntoRepo()

      return manifest

  # TODO(akeshet): Once builder run attributes such as metadata are being
  # pickled and passed across cbuildbot re-executions, move this functionality
  # out of cbuildbot_stages and into validation_pool.ValidationPool.
  # See crbug.com/356930
  def _RecordPatchesInMetadata(self, pool):
    # If possible, use the timestamp at which the pool was acquired.
    timestamp = pool.acquired_at_time or int(time.time())
    for change in pool.changes:
      self._run.attrs.metadata.RecordCLAction(change,
                                              constants.CL_ACTION_PICKED_UP,
                                              timestamp)

  def _RunPrePatchBuild(self):
    """Run through a pre-patch build to prepare for incremental build.

    This function runs though the InitSDKStage, SetupBoardStage, and
    BuildPackagesStage. It is intended to be called before applying
    any patches under test, to prepare the chroot and sysroot in a state
    corresponding to ToT prior to an incremental build.

    Returns:
      True if all stages were successful, False if any of them failed.
    """
    suffix = ' (pre-Patch)'
    try:
      InitSDKStage(self._run, chroot_replace=True, suffix=suffix).Run()
      for builder_run in self._run.GetUngroupedBuilderRuns():
        for board in builder_run.config.boards:
          SetupBoardStage(builder_run, board=board, suffix=suffix).Run()
          BuildPackagesStage(builder_run, board=board, suffix=suffix).Run()
    except results_lib.StepFailure:
      return False

    return True

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
        success=self.success, message=self.message,
        dashboard_url=self.ConstructDashboardURL())


class ImportantBuilderFailedException(results_lib.StepFailure):
  """Exception thrown when an important build fails to build."""


class MasterSlaveSyncCompletionStage(ManifestVersionedSyncCompletionStage):
  """Stage that records whether we passed or failed to build/test manifest."""

  def __init__(self, *args, **kwargs):
    super(MasterSlaveSyncCompletionStage, self).__init__(*args, **kwargs)
    self._slave_statuses = {}

  def _FetchSlaveStatuses(self):
    """Fetch and return build status for slaves of this build.

    If this build is not a master then return just the status of this build.

    Returns:
      A dict with "bot id" keys and BuilderStatus objects for values.  All keys
      will have valid BuilderStatus values, but builders that never started
      will have a BuilderStatus with status MISSING.
    """
    if not self._run.config.master:
      # This is a slave build, so return the status for this build.
      if self._run.options.debug:
        # In debug mode, nothing is uploaded to Google Storage, so we bypass
        # the extra hop and just look at what we have locally.
        status = manifest_version.BuilderStatus.GetCompletedStatus(self.success)
        status_obj = manifest_version.BuilderStatus(status, self.message)
        return {self._bot_id: status_obj}
      else:
        # Slaves only need to look at their own status.
        return self._run.attrs.manifest_manager.GetBuildersStatus(
            [self._bot_id])
    else:
      # This is a master build, so return the statuses for all its slaves.

      # Wait for slaves to finish, unless this is a debug run.
      wait_for_results = not self._run.options.debug

      builders = self._GetSlaveConfigs()
      builder_names = [b['name'] for b in builders]

      manager = self._run.attrs.manifest_manager
      if MasterSlaveSyncStage.sub_manager:
        manager = MasterSlaveSyncStage.sub_manager

      return manager.GetBuildersStatus(builder_names, wait_for_results)

  def _AbortCQHWTests(self):
    """Abort any HWTests started by the CQ."""
    if (cbuildbot_config.IsCQType(self._run.config.build_type) and
        self._run.manifest_branch == 'master'):
      version = self._run.GetVersion()
      if not commands.HaveCQHWTestsBeenAborted(version):
        commands.AbortCQHWTests(version, self._run.options.debug)

  def _HandleStageException(self, exc_info):
    """Decide whether an exception should be treated as fatal."""
    # Besides the master, the completion stages also run on slaves, to report
    # their status back to the master. If the build failed, they throw an
    # exception here. For slave builders, marking this stage 'red' would be
    # redundant, since the build itself would already be red. In this case,
    # report a warning instead.
    # pylint: disable=W0212
    exc_type = exc_info[0]
    if (issubclass(exc_type, ImportantBuilderFailedException) and
        not self._run.config.master):
      return self._HandleExceptionAsWarning(exc_info)
    else:
      # In all other cases, exceptions should be treated as fatal. To
      # implement this, we bypass ForgivingStage and call
      # bs.BuilderStage._HandleStageException explicitly.
      return bs.BuilderStage._HandleStageException(self, exc_info)

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
    no_stat = set(builder for builder, status in statuses.iteritems()
                  if status.Missing())
    failing = set(builder for builder, status in statuses.iteritems()
                  if status.Failed())
    inflight = set(builder for builder, status in statuses.iteritems()
                   if status.Inflight())

    # If all the failing or inflight builders were sanity checkers
    # then ignore the failure.
    fatal = self._IsFailureFatal(failing, inflight, no_stat)

    if fatal:
      self._AnnotateFailingBuilders(failing, inflight, no_stat, statuses)
      self.HandleFailure(failing, inflight)
      raise ImportantBuilderFailedException()
    else:
      self.HandleSuccess()

  def _IsFailureFatal(self, failing, inflight, no_stat):
    """Returns a boolean indicating whether the build should fail.

    Args:
      failing: Set of builder names of slave builders that failed.
      inflight: Set of builder names of slave builders that are inflight
      no_stat: Set of builder names of slave builders that had status None.

    Returns:
      True if any of the failing or inflight builders are not sanity check
      builders for this master, or if there were any non-sanity-check builders
      with status None.
    """
    sanity_builders = self._run.config.sanity_check_slaves or []
    sanity_builders = set(sanity_builders)
    return not sanity_builders.issuperset(failing | inflight | no_stat)

  def _AnnotateFailingBuilders(self, failing, inflight, no_stat, statuses):
    """Add annotations that link to either failing or inflight builders.

    Adds buildbot links to failing builder dashboards. If no builders are
    failing, adds links to inflight builders. Adds step text for builders
    with status None.

    Args:
      failing: Set of builder names of slave builders that failed.
      inflight: Set of builder names of slave builders that are inflight.
      no_stat: Set of builder names of slave builders that had status None.
      statuses: A builder-name->status dictionary, which will provide
                the dashboard_url values for any links.
    """
    builders_to_link = failing or inflight or []
    for builder in builders_to_link:
      if statuses[builder].dashboard_url:
        cros_build_lib.PrintBuildbotLink(builder,
                                         statuses[builder].dashboard_url)
    for builder in no_stat:
      cros_build_lib.PrintBuildbotStepText('%s did not start.' % builder)

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
          if not message or not message.tracebacks:
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
    """Determines whether any of the sanity check slaves failed.

    Args:
      sanity_check_slaves: Names of slave builders that are "sanity check"
        builders for the current master.
      slave_statuses: Dict of BuilderStatus objects by builder name keys.

    Returns:
      True if no sanity builders ran and failed.
    """
    sanity_check_slaves = sanity_check_slaves or []
    return not any([x in slave_statuses and slave_statuses[x].Failed()
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
        success=self.success, message=self.message,
        dashboard_url=self.ConstructDashboardURL())


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
      self.pool = validation_pool.ValidationPool.Load(filename,
          metadata=self._run.attrs.metadata)

  def PerformStage(self):
    super(PreCQSyncStage, self).PerformStage()
    self.pool = validation_pool.ValidationPool.AcquirePreCQPool(
        self._run.config.overlays, self._build_root,
        self._run.buildnumber, self._run.config.name,
        dryrun=self._run.options.debug_forced, changes=self.patches,
        metadata=self._run.attrs.metadata)
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
      cmd += ['-g', cros_patch.AddPrefix(patch, patch.gerrit_number)]
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
        constants.PRE_CQ_LAUNCHER_NAME,
        dryrun=self._run.options.debug,
        changes_query=self._run.options.cq_gerrit_override,
        check_tree_open=False, change_filter=self.ProcessChanges,
        metadata=self._run.attrs.metadata)


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
        # Can not use the default version of get() here since
        # 'upstream' can be a valid key with a None value.
        upstream = checkout.get('upstream')
        if upstream is not None:
          node.attrib['upstream'] = upstream

    for node in root.findall('default'):
      node.attrib['revision'] = new_branch_name

    doc.write(manifest_path)

  def _FixUpManifests(self, repo_manifest):
    """Points the checkouts at the new branch in the manifests.

    Within the branch, make sure all manifests with projects that are
    "branchable" are checked out to "refs/heads/<new_branch>".  Do this
    by updating all manifests in the known manifest projects.
    """
    assert not self._run.options.delete_branch, 'Cannot fix a deleted branch.'

    # Use local branch ref.
    branch_ref = git.NormalizeRef(self.branch_name)

    cros_build_lib.Debug('Fixing manifest projects for new branch.')
    for project in constants.MANIFEST_PROJECTS:
      manifest_checkout = repo_manifest.FindCheckout(project)
      manifest_dir = manifest_checkout['local_path']
      push_remote = manifest_checkout['push_remote']

      # Checkout revision can be either a sha1 or a branch ref.
      src_ref = manifest_checkout['revision']
      if not git.IsSHA1(src_ref):
        src_ref = git.NormalizeRemoteRef(push_remote, src_ref)

      git.CreateBranch(
          manifest_dir, manifest_version.PUSH_BRANCH, src_ref)

      # Look for all manifest files in manifest_dir.
      for manifest_path in glob.glob(os.path.join(manifest_dir, '*.xml')):
        # Accept only non-symlink xml files directly in manifest_dir.
        if not os.path.islink(manifest_path):
          cros_build_lib.Debug('Fixing manifest at %s.', manifest_path)
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

  def __init__(self, builder_run, chroot_replace=False, **kwargs):
    """InitSDK constructor.

    Args:
      builder_run: Builder run instance for this run.
      chroot_replace: If True, force the chroot to be replaced.
    """
    super(InitSDKStage, self).__init__(builder_run, **kwargs)
    self._env = {}
    self.force_chroot_replace = chroot_replace
    if self._run.options.clobber:
      self._env['IGNORE_PREFLIGHT_BINHOST'] = '1'

    self._latest_toolchain = (self._run.config.latest_toolchain or
                              self._run.options.latest_toolchain)
    if self._latest_toolchain and self._run.config.gcc_githash:
      self._env['USE'] = 'git_gcc'
      self._env['GCC_GITHASH'] = self._run.config.gcc_githash

  def PerformStage(self):
    chroot_path = os.path.join(self._build_root, constants.DEFAULT_CHROOT_DIR)
    replace = self._run.config.chroot_replace or self.force_chroot_replace
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

    commands.SetSharedUserPassword(
        self._build_root,
        password=self._run.config.shared_user_password)


class SetupBoardStage(BoardSpecificBuilderStage, InitSDKStage):
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
          extra_env=self._env, chroot_upgrade=chroot_upgrade,
          profile=self._run.options.profile or self._run.config.profile)


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

  def HandleSkip(self):
    """Set run.attrs.chrome_version to chrome version in buildroot now."""
    self._run.attrs.chrome_version = self._run.DetermineChromeVersion()
    cros_build_lib.Debug('Existing chrome version is %s.',
                         self._run.attrs.chrome_version)
    super(SyncChromeStage, self).HandleSkip()

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


class BuildPackagesStage(BoardSpecificBuilderStage, ArchivingStageMixin):
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

    # If we have rietveld patches, always compile Chrome from source.
    noworkon = not self._run.options.rietveld_patches

    commands.Build(self._build_root,
                   self._current_board,
                   build_autotest=self._build_autotest,
                   usepkg=self._run.config.usepkg_build_packages,
                   chrome_binhost_only=self._run.config.chrome_binhost_only,
                   packages=self._run.config.packages,
                   skip_chroot_upgrade=True,
                   chrome_root=self._run.options.chrome_root,
                   noworkon=noworkon,
                   extra_env=self._env)


class ChromeSDKStage(BoardSpecificBuilderStage, ArchivingStageMixin):
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
    chrome_dir = ArchiveStage.SingleMatchGlob(
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
            image_name = 'chromiumos_image.bin'
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


class VMTestStage(BoardSpecificBuilderStage, ArchivingStageMixin):
  """Run autotests in a virtual machine."""

  option_name = 'tests'
  config_name = 'vm_tests'

  def _PrintFailedTests(self, results_path, test_basename):
    """Print links to failed tests.

    Args:
      results_path: Path to directory containing the test results.
      test_basename: The basename that the tests are archived to.
    """
    test_list = commands.ListFailedTests(results_path)
    for test_name, path in test_list:
      self.PrintDownloadLink(
          os.path.join(test_basename, path), text_to_display=test_name)

  def _ArchiveTestResults(self, test_results_dir, test_basename):
    """Archives test results to Google Storage.

    Args:
      test_results_dir: Name of the directory containing the test results.
      test_basename: The basename to archive the tests.
    """
    results_path = commands.GetTestResultsDir(
        self._build_root, test_results_dir)

    # Skip archiving if results_path does not exist or is an empty directory.
    if not os.path.isdir(results_path) or not os.listdir(results_path):
      return

    archived_results_dir = os.path.join(self.archive_path, test_basename)
    # Copy relevant files to archvied_results_dir.
    commands.ArchiveTestResults(results_path, archived_results_dir)
    upload_paths = [os.path.basename(archived_results_dir)]
    # Create the compressed tarball to upload.
    # TODO: We should revisit whether uploading the tarball is necessary.
    test_tarball = commands.BuildAndArchiveTestResultsTarball(
        archived_results_dir, self._build_root)
    upload_paths.append(test_tarball)

    got_symbols = False
    if self._run.options.archive:
      got_symbols = self.GetParallel('breakpad_symbols_generated',
                                     pretty_name='breakpad symbols')
    upload_paths += commands.GenerateStackTraces(
        self._build_root, self._current_board, test_results_dir,
        self.archive_path, got_symbols)

    self._Upload(upload_paths)
    self._PrintFailedTests(results_path, test_basename)

    # Remove the test results directory.
    osutils.RmDir(results_path, ignore_missing=True, sudo=True)

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

  def _RunTest(self, test_type, test_results_dir):
    """Run a VM test.

    Args:
      test_type: Any test in constants.VALID_VM_TEST_TYPES
      test_results_dir: The base directory to store the results.
    """
    if test_type == constants.CROS_VM_TEST_TYPE:
      commands.RunCrosVMTest(self._current_board, self.GetImageDirSymlink())
    elif test_type == constants.DEV_MODE_TEST_TYPE:
      commands.RunDevModeTest(
        self._build_root, self._current_board, self.GetImageDirSymlink())
    else:
      commands.RunTestSuite(self._build_root,
                            self._current_board,
                            self.GetImageDirSymlink(),
                            os.path.join(test_results_dir,
                                         'test_harness'),
                            test_type=test_type,
                            whitelist_chrome_crashes=self._chrome_rev is None,
                            archive_dir=self.bot_archive_root)

  def PerformStage(self):
    # These directories are used later to archive test artifacts.
    test_results_dir = commands.CreateTestRoot(self._build_root)
    test_basename = constants.VM_TEST_RESULTS % dict(attempt=self._attempt)
    try:
      for test_type in self._run.config.vm_tests:
        cros_build_lib.Info('Running VM test %s.', test_type)
        self._RunTest(test_type, test_results_dir)

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


class HWTestStage(BoardSpecificBuilderStage, ArchivingStageMixin):
  """Stage that runs tests in the Autotest lab."""

  option_name = 'tests'
  config_name = 'hw_tests'

  PERF_RESULTS_EXTENSION = 'results'

  def __init__(self, builder_run, board, suite_config, **kwargs):
    super(HWTestStage, self).__init__(builder_run, board,
                                      suffix=' [%s]' % suite_config.suite,
                                      **kwargs)
    self.suite_config = suite_config
    self.wait_for_results = True

  def _PrintFile(self, filename):
    with open(filename) as f:
      print f.read()


  def _CheckAborted(self):
    """Checks with GS to see if HWTest for this build's release_tag was aborted.

    We currently only support aborting HWTests for the CQ, so this method only
    returns True for paladin builders.

    Returns:
      True if HWTest have been aborted for this build's release_tag.
      False otherwise.
    """
    aborted = (cbuildbot_config.IsCQType(self._run.config.build_type) and
               commands.HaveCQHWTestsBeenAborted(self._run.GetVersion()))
    return aborted

  # Disable complaint about calling _HandleStageException.
  # pylint: disable=W0212
  def _HandleStageException(self, exc_info):
    """Override and don't set status to FAIL but FORGIVEN instead."""
    exc_type, exc_value = exc_info[:2]

    # Deal with timeout errors specially.
    if issubclass(exc_type, timeout_util.TimeoutError):
      return self._HandleStageTimeoutException(exc_info)

    # 2 for warnings returned by run_suite.py, or CLIENT_HTTP_CODE error
    # returned by autotest_rpc_client.py. It is the former that we care about.
    # 11, 12, 13 for cases when rpc is down, see autotest_rpc_errors.py.
    codes_handled_as_warning = (2, 11, 12, 13)

    if self.suite_config.critical:
      return super(HWTestStage, self)._HandleStageException(exc_info)
    is_lab_down = (issubclass(exc_type, lab_status.LabIsDownException) or
                   issubclass(exc_type, lab_status.BoardIsDisabledException))
    is_warning_code = (issubclass(exc_type, cros_build_lib.RunCommandError) and
                       exc_value.result.returncode in codes_handled_as_warning)
    if is_lab_down:
      cros_build_lib.Warning('HWTest was skipped because the lab was down.')
      return self._HandleExceptionAsWarning(exc_info)
    elif is_warning_code:
      cros_build_lib.Warning('HWTest failed with warning code.')
      return self._HandleExceptionAsWarning(exc_info)
    elif self._CheckAborted():
      cros_build_lib.Warning(CQ_HWTEST_WAS_ABORTED)
      return self._HandleExceptionAsWarning(exc_info)
    else:
      return super(HWTestStage, self)._HandleStageException(exc_info)

  def _HandleStageTimeoutException(self, exc_info):
    if not self.suite_config.critical and not self.suite_config.fatal_timeouts:
      return self._HandleExceptionAsWarning(exc_info)

    return super(HWTestStage, self)._HandleStageException(exc_info)

  def PerformStage(self):
    if self._CheckAborted():
      cros_build_lib.PrintBuildbotStepText('aborted')
      cros_build_lib.Warning(CQ_HWTEST_WAS_ABORTED)
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

  # Poll for new results every 30 seconds.
  SIGNING_PERIOD = 30

  # Abort, if the signing takes longer than 2 hours.
  #   2 hours * 60 minutes * 60 seconds
  SIGNING_TIMEOUT = 7200

  FINISHED = 'finished'

  def __init__(self, *args, **kwargs):
    super(SignerResultsStage, self).__init__(*args, **kwargs)
    self.signing_results = {}

  def _HandleStageException(self, exc_info):
    """Convert SignerResultsException exceptions to WARNING (for now)."""
    exc_type = exc_info[0]

    # If we raise an exception, make sure nobody keeps waiting on our results.
    self.archive_stage.AnnounceChannelSigned(None)

    # This stage is experimental, don't trust it yet.
    if issubclass(exc_type, SignerResultsException):
      return self._HandleExceptionAsWarning(exc_info)

    return super(SignerResultsStage, self)._HandleStageException(exc_info)

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
    return (signer_json or {}).get('status', {}).get('status', '')

  def _CheckForResults(self, gs_ctx, instruction_urls_per_channel):
    """timeout_util.WaitForSuccess func to check a list of signer results.

    Args:
      gs_ctx: Google Storage Context.
      instruction_urls_per_channel: Urls of the signer result files
                                    we're expecting.

    Returns:
      Number of results not yet collected.
    """
    COMPLETED_STATUS = ('passed', 'failed')

    # Assume we are done, then try to prove otherwise.
    results_completed = True

    for channel in instruction_urls_per_channel:
      self.signing_results.setdefault(channel, {})

      if (len(self.signing_results[channel]) ==
          len(instruction_urls_per_channel[channel])):
        continue

      for url in instruction_urls_per_channel[channel]:
        # Convert from instructions URL to instructions result URL.
        url += '.json'

        # We already have a result for this URL.
        if url in self.signing_results[channel]:
          continue

        signer_json = self._JsonFromUrl(gs_ctx, url)
        if self._SigningStatusFromJson(signer_json) in COMPLETED_STATUS:
          # If we find a completed result, remember it.
          self.signing_results[channel][url] = signer_json

      if (len(self.signing_results[channel]) ==
          len(instruction_urls_per_channel[channel])):
        # If we just completed the channel, inform paygen.
        self.archive_stage.AnnounceChannelSigned(channel)
      else:
        results_completed = False

    return results_completed

  def PerformStage(self):
    """Do the work of waiting for signer results and logging them.

    Raises:
      ValueError: If the signer result isn't valid json.
      RunCommandError: If we are unable to download signer results.
    """
    # These results are expected to contain:
    # { 'channel': ['gs://instruction_uri1', 'gs://signer_instruction_uri2'] }
    instruction_urls_per_channel = self.archive_stage.WaitForPushImage()
    if instruction_urls_per_channel is None:
      raise MissingInstructionException('PushImage results not available.')

    gs_ctx = gs.GSContext(dry_run=self._run.debug)

    try:
      cros_build_lib.Info('Waiting for signer results.')
      timeout_util.WaitForReturnTrue(
          self._CheckForResults,
          func_args=(gs_ctx, instruction_urls_per_channel),
          timeout=self.SIGNING_TIMEOUT, period=self.SIGNING_PERIOD)
    except timeout_util.TimeoutError:
      msg = 'Image signing timed out.'
      cros_build_lib.Error(msg)
      cros_build_lib.PrintBuildbotStepText(msg)
      raise SignerResultsTimeout(msg)

    # Log all signer results, then handle any signing failures.
    failures = []
    for url_results in self.signing_results.values():
      for url, signer_result in url_results.iteritems():
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

    # If we completed successfully, announce all channels compeleted.
    self.archive_stage.AnnounceChannelSigned(self.FINISHED)


class PaygenSigningRequirementsError(Exception):
  """Paygen stage can't run if signing failed."""


class PaygenStage(ArchivingStage):
  """Stage that generates release payloads.

  If this stage is created with a 'channels' argument, it can run
  independantly. Otherwise, it's dependent on values queued up by
  the SignerResultsStage.
  """
  option_name = 'paygen'
  config_name = 'paygen'

  def __init__(self, builder_run, board, archive_stage, channels=None,
               **kwargs):
    """Init that accepts the channels argument, if present.

    Args:
      builder_run: See builder_run on ArchivingStage.
      board: See board on ArchivingStage.
      archive_stage: See archive_stage on ArchivingStage.
      channels: Explicit list of channels to generate payloads for.
                If empty, will instead wait on values from SignerResultsStage.
                Channels is normally None in release builds, and normally set
                for trybot 'payloads' builds.
    """
    super(PaygenStage, self).__init__(builder_run, board, archive_stage,
                                      **kwargs)
    self.channels = channels

  def _HandleStageException(self, exc_info):
    """Override and don't set status to FAIL but FORGIVEN instead."""
    exc_type = exc_info[0]

    # TODO(dgarrett): Constrain this to only expected exceptions.
    if issubclass(exc_type, Exception):
      return self._HandleExceptionAsWarning(exc_info)

    return super(PaygenStage, self)._HandleStageException(exc_info)

  def PerformStage(self):
    """Do the work of generating our release payloads."""

    board = self._current_board
    version = self._run.attrs.release_tag

    assert version, "We can't generate payloads without a release_tag."
    logging.info("Generating payloads for: %s, %s", board, version)

    with parallel.BackgroundTaskRunner(self._RunPaygenInProcess) as per_channel:
      if self.channels:
        logging.info("Using explicit channels: %s", self.channels)
        # If we have an explicit list of channels, use it.
        for channel in self.channels:
          per_channel.put((channel, board, version, self._run.debug,
                           self._run.config.perform_paygen_testing))
      else:
        # Otherwise, wait for SignerResults to tell us which channels are ready.
        while True:
          channel = self.archive_stage.WaitForChannelSigning()

          if channel is None:
            # This signals a signer error, or that signing never ran.
            raise PaygenSigningRequirementsError()

          if channel == SignerResultsStage.FINISHED:
            break

          logging.info("Notified of channel: %s", channel)
          per_channel.put((channel, board, version, self._run.debug,
                           self._run.config.perform_paygen_testing))

  def _RunPaygenInProcess(self, channel, board, version, debug, test_payloads):
    """Helper for PaygenStage that invokes payload generation.

    This method is intended to be safe to invoke inside a process.

    Args:
      channel: Channel of payloads to generate ('stable', 'beta', etc)
      board: Board of payloads to generate ('x86-mario', 'x86-alex-he', etc)
      version: Version of payloads to generate.
      debug: Flag telling if this is a real run, or a test run.
      test_payloads: Generate test payloads, and schedule auto testing.
    """
    # TODO(dgarrett): Remove when crbug.com/341152 is fixed.
    # These modules are imported here because they aren't always available at
    # cbuildbot startup.
    # pylint: disable=F0401
    from crostools.lib import gspaths
    from crostools.lib import paygen_build_lib

    # Convert to release tools naming for channels.
    if not channel.endswith('-channel'):
      channel += '-channel'

    # Convert to release tools naming for boards.
    board = board.replace('_', '-')

    with osutils.TempDir(sudo_rm=True) as tempdir:
      # Create the definition of the build to generate payloads for.
      build = gspaths.Build(channel=channel,
                            board=board,
                            version=version)

      try:
        # Generate the payloads.
        self._PrintLoudly('Starting %s, %s, %s' % (channel, version, board))
        paygen_build_lib.CreatePayloads(build,
                                        work_dir=tempdir,
                                        dry_run=debug,
                                        run_parallel=True,
                                        run_on_builder=True,
                                        skip_test_payloads=not test_payloads,
                                        skip_autotest=not test_payloads)
      except (paygen_build_lib.BuildFinished,
              paygen_build_lib.BuildLocked,
              paygen_build_lib.BuildSkip) as e:
        # These errors are normal if it's possible for another process to
        # work on the same build. This process could be a Paygen server, or
        # another builder (perhaps by a trybot generating payloads on request).
        #
        # This means the build was finished by the other process, is already
        # being processed (so the build is locked), or that it's been marked
        # to skip (probably done manually).
        cros_build_lib.Info('Paygen skipped because: %s', e)


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


class ArchiveStage(BoardSpecificBuilderStage, ArchivingStageMixin):
  """Archives build and test artifacts for developer consumption.

  Attributes:
    release_tag: The release tag. E.g. 2981.0.0
    version: The full version string, including the milestone.
        E.g. R26-2981.0.0-b123
  """

  option_name = 'archive'

  # This stage is intended to run in the background, in parallel with tests.
  def __init__(self, builder_run, board, chrome_version=None, **kwargs):
    super(ArchiveStage, self).__init__(builder_run, board, **kwargs)
    self.chrome_version = chrome_version

    # TODO(mtennant): Places that use this release_tag attribute should
    # move to use self._run.attrs.release_tag directly.
    self.release_tag = getattr(self._run.attrs, 'release_tag', None)

    self._recovery_image_status_queue = multiprocessing.Queue()
    self._release_upload_queue = multiprocessing.Queue()
    self._upload_queue = multiprocessing.Queue()
    self._push_image_status_queue = multiprocessing.Queue()
    self._wait_for_channel_signing = multiprocessing.Queue()
    self.artifacts = []

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
      None on error, or if pushimage didn't run.
    """
    cros_build_lib.Info('Waiting for PushImage...')
    urls = self._push_image_status_queue.get()
    # Put the status back so other processes don't starve.
    self._push_image_status_queue.put(urls)
    return urls

  def AnnounceChannelSigned(self, channel):
    """Announce that image signing has compeleted for a given channel.

    Args:
      channel: Either a channel name ('stable', 'dev', etc).
               SignerResultsStage.FINISHED if all channels are finished.
               None if there was an error, and no channels will be announced.
    """
    self._wait_for_channel_signing.put(channel)

  def WaitForChannelSigning(self):
    """Wait until ChannelSigning completes for a given channel.

    This method is expected to return once for each channel, and return
    the name of the channel that was signed. When all channels are signed,
    it should return again with None.

    Returns:
      The name of the channel for which images have been signed.
      None when all channels are signed, or on error.
    """
    cros_build_lib.Info('Waiting for channel images to be signed...')
    return self._wait_for_channel_signing.get()

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

  def LoadArtifactsList(self, board, image_dir):
    """Load the list of artifacts to upload for this board.

    It attempts to load a JSON file, scripts/artifacts.json, from the
    overlay directories for this board. This file specifies the artifacts
    to generate, if it can't be found, it will use a default set that
    uploads every .bin file as a .tar.xz file except for
    chromiumos_qemu_image.bin.

    See BuildStandaloneArchive in cbuildbot_commands.py for format docs.
    """
    custom_artifacts_file = portage_utilities.ReadOverlayFile(
        'scripts/artifacts.json', board=board)
    artifacts = None

    if custom_artifacts_file is not None:
      json_file = json.loads(custom_artifacts_file)
      artifacts = json_file.get('artifacts')

    if artifacts is None:
      artifacts = []
      for image_file in glob.glob(os.path.join(image_dir, '*.bin')):
        basename = os.path.basename(image_file)
        if basename != constants.VM_IMAGE_BIN:
          info = {'input': [basename], 'archive': 'tar', 'compress': 'xz'}
          artifacts.append(info)

    for artifact in artifacts:
      # Resolve the (possible) globs in the input list, and store
      # the actual set of files to use in 'paths'
      paths = []
      for s in artifact['input']:
        glob_paths = glob.glob(os.path.join(image_dir, s))
        if not glob_paths:
          logging.warning('No artifacts generated for input: %s', s)
        else:
          for path in glob_paths:
            paths.append(os.path.relpath(path, image_dir))
      artifact['paths'] = paths
    self.artifacts = artifacts

  def IsArchivedFile(self, filename):
    """Return True if filename is the name of a file being archived."""
    for artifact in self.artifacts:
      for path in itertools.chain(artifact['paths'], artifact['input']):
        if os.path.basename(path) == filename:
          return True
    return False

  def PerformStage(self):
    buildroot = self._build_root
    config = self._run.config
    board = self._current_board
    debug = self._run.debug
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
    #          \- ArchiveStandaloneArtifacts
    #             \- ArchiveStandaloneArtifact
    #          \- ArchiveZipFiles
    #          \- ArchiveHWQual
    #       \- PushImage (blocks on BuildAndArchiveAllImages)
    #    \- ArchiveManifest
    #    \- ArchiveStrippedChrome
    #    \- ArchiveImageScripts

    def ArchiveManifest():
      """Create manifest.xml snapshot of the built code."""
      output_manifest = os.path.join(archive_path, 'manifest.xml')
      cmd = ['repo', 'manifest', '-r', '-o', output_manifest]
      cros_build_lib.RunCommand(cmd, cwd=buildroot, capture_output=True)
      self._upload_queue.put(['manifest.xml'])

    def BuildAndArchiveFactoryImages():
      """Build and archive the factory zip file.

      The factory zip file consists of the factory toolkit and the factory
      install image. Both are built here.
      """
      # Build factory install image and create a symlink to it.
      factory_install_symlink = None
      if 'factory_install' in config['images']:
        alias = commands.BuildFactoryInstallImage(buildroot, board, extra_env)
        factory_install_symlink = self.GetImageDirSymlink(alias)
        if config['factory_install_netboot']:
          commands.MakeNetboot(buildroot, board, factory_install_symlink)

      # Build the factory toolkit.
      chroot_dir = os.path.join(buildroot, constants.DEFAULT_CHROOT_DIR)
      chroot_tmp_dir = os.path.join(chroot_dir, 'tmp')
      with osutils.TempDir(base_dir=chroot_tmp_dir) as tempdir:
        # Build the factory toolkit.
        if config['factory_toolkit']:
          toolkit_dir = os.path.join(tempdir, 'factory_toolkit')
          os.makedirs(toolkit_dir)
          commands.MakeFactoryToolkit(
              buildroot, board, toolkit_dir, self._run.attrs.release_tag)

        # Build and upload factory zip if needed.
        if factory_install_symlink or config['factory_toolkit']:
          filename = commands.BuildFactoryZip(
              buildroot, board, archive_path, factory_install_symlink,
              toolkit_dir, self._run.attrs.release_tag)
          self._release_upload_queue.put([filename])

    def ArchiveStandaloneArtifact(artifact_info):
      """Build and upload a single archive."""
      if artifact_info['paths']:
        for path in commands.BuildStandaloneArchive(archive_path, image_dir,
                                                    artifact_info):
          self._release_upload_queue.put([path])

    def ArchiveStandaloneArtifacts():
      """Build and upload standalone archives for each image."""
      if config['upload_standalone_images']:
        parallel.RunTasksInProcessPool(ArchiveStandaloneArtifact,
                                       [[x] for x in self.artifacts])

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
      self.LoadArtifactsList(self._current_board, image_dir)

      # For recovery image to be generated correctly, BuildRecoveryImage must
      # run before BuildAndArchiveFactoryImages.
      if self.IsArchivedFile(constants.BASE_IMAGE_BIN):
        commands.BuildRecoveryImage(buildroot, board, image_dir, extra_env)
        self._recovery_image_status_queue.put(True)
        # Re-generate the artifacts list so we include the newly created
        # recovery image.
        self.LoadArtifactsList(self._current_board, image_dir)

      if config['images']:
        parallel.RunParallelSteps([BuildAndArchiveFactoryImages,
                                   ArchiveHWQual,
                                   ArchiveStandaloneArtifacts,
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

      self.GetParallel('debug_tarball_generated', pretty_name='debug tarball')

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
      steps = [ArchiveReleaseArtifacts, ArchiveManifest]
      if config['images']:
        steps.extend([self.ArchiveStrippedChrome, ArchiveImageScripts])
      if config['create_delta_sysroot']:
        steps.append(self.BuildAndArchiveDeltaSysroot)

      with self.ArtifactUploader(self._upload_queue, archive=False):
        parallel.RunParallelSteps(steps)

    try:
      if not self._run.config.pgo_generate:
        BuildAndArchiveArtifacts()
    finally:
      commands.RemoveOldArchives(self.bot_archive_root,
                                 self._run.options.max_archive_builds)

  def _HandleStageException(self, exc_info):
    # Tell the HWTestStage not to wait for artifacts to be uploaded
    # in case ArchiveStage throws an exception.
    self._recovery_image_status_queue.put(False)
    self._push_image_status_queue.put(None)
    self._wait_for_channel_signing.put(None)
    return super(ArchiveStage, self)._HandleStageException(exc_info)


class CPEExportStage(BoardSpecificBuilderStage, ArchivingStageMixin):
  """Handles generation & upload of package CPE information."""

  def PerformStage(self):
    """Generate debug symbols and upload debug.tgz."""
    buildroot = self._build_root
    board = self._current_board
    useflags = self._run.config.useflags

    logging.info('Generating CPE export.')
    result = commands.GenerateCPEExport(buildroot, board, useflags)

    logging.info('Writing CPE export to files for archive.')
    warnings_filename = os.path.join(self.archive_path,
                                     'cpe-warnings-chromeos-%s.txt' % board)
    results_filename = os.path.join(self.archive_path,
                                    'cpe-chromeos-%s.json' % board)

    osutils.WriteFile(warnings_filename, result.error)
    osutils.WriteFile(results_filename, result.output)

    logging.info('Uploading CPE files.')
    self.UploadArtifact(os.path.basename(warnings_filename), archive=False)
    self.UploadArtifact(os.path.basename(results_filename), archive=False)


class DebugSymbolsStage(BoardSpecificBuilderStage, ArchivingStageMixin):
  """Handles generation & upload of debug symbols."""

  def PerformStage(self):
    """Generate debug symbols and upload debug.tgz."""
    buildroot = self._build_root
    board = self._current_board

    commands.GenerateBreakpadSymbols(buildroot, board, self._run.debug)
    self.board_runattrs.SetParallel('breakpad_symbols_generated', True)

    steps = [self.UploadDebugTarball]
    failed_list = os.path.join(self.archive_path, 'failed_upload_symbols.list')
    if self._run.config.upload_symbols:
      steps.append(lambda: self.UploadSymbols(buildroot, board, failed_list))

    parallel.RunParallelSteps(steps)

  def UploadDebugTarball(self):
    """Generate and upload the debug tarball."""
    filename = commands.GenerateDebugTarball(
        self._build_root, self._current_board, self.archive_path,
        self._run.config.archive_build_debug)
    self.UploadArtifact(filename, archive=False)

    cros_build_lib.Info('Announcing availability of debug tarball now.')
    self.board_runattrs.SetParallel('debug_tarball_generated', True)

  def UploadSymbols(self, buildroot, board, failed_list):
    """Upload generated debug symbols."""
    if self._run.options.remote_trybot or self._run.debug:
      # For debug builds, limit ourselves to just uploading 1 symbol.
      # This way trybots and such still exercise this code.
      cnt = 1
      official = False
    else:
      cnt = None
      official = self._run.config.chromeos_official

    commands.UploadSymbols(buildroot, board, official, cnt, failed_list)

    if os.path.exists(failed_list):
      self.UploadArtifact(os.path.basename(failed_list), archive=False)

  def _HandleStageException(self, exc_info):
    """Tell other stages to not wait on us if we die for some reason."""
    self.board_runattrs.SetParallelDefault('breakpad_symbols_generated', False)
    self.board_runattrs.SetParallelDefault('debug_tarball_generated', False)

    return super(DebugSymbolsStage, self)._HandleStageException(exc_info)


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
    for slave_config in self._GetSlaveConfigs():
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

  def __init__(self, builder_run, board, **kwargs):
    super(UploadPrebuiltsStage, self).__init__(builder_run, board, **kwargs)

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
        for c in self._GetSlaveConfigs():
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


class ReportStage(bs.BuilderStage, ArchivingStageMixin):
  """Summarize all the builds."""

  _HTML_HEAD = """<html>
<head>
 <title>Archive Index: %(board)s / %(version)s</title>
</head>
<body>
<h2>Artifacts Index: %(board)s / %(version)s (%(config)s config)</h2>"""

  def __init__(self, builder_run, sync_instance, completion_instance, **kwargs):
    super(ReportStage, self).__init__(builder_run, **kwargs)

    # TODO(mtennant): All these should be retrieved from builder_run instead.
    # Or, more correctly, the info currently retrieved from these stages should
    # be stored and retrieved from builder_run instead.
    self._sync_instance = sync_instance
    self._completion_instance = completion_instance

  def _UpdateRunStreak(self, builder_run, final_status):
    """Update the streak counter for this builder, if applicable, and notify.

    Update the pass/fail streak counter for the builder.  If the new
    streak should trigger a notification email then send it now.

    Args:
      builder_run: BuilderRun for this run.
      final_status: Final status string for this run.
    """

    # Exclude tryjobs from streak counting.
    if not builder_run.options.remote_trybot and not builder_run.options.local:
      streak_value = self._UpdateStreakCounter(
          final_status=final_status, counter_name=builder_run.config.name,
          dry_run=self._run.debug)
      cros_build_lib.Info('New pass/fail streak value for %s is: %s',
                          builder_run.config.name, streak_value)
      # See if updated streak should trigger a notification email.
      if (builder_run.config.health_alert_recipients and
          streak_value <= -builder_run.config.health_threshold):
        cros_build_lib.Info(
          'Builder failed %i consecutive times, sending health alert email '
          'to %s.',
          -streak_value,
          builder_run.config.health_alert_recipients)

        if not self._run.debug:
          alerts.SendEmail('%s health alert' % builder_run.config.name,
                           builder_run.config.health_alert_recipients,
                           message=self._HealthAlertMessage(-streak_value),
                           smtp_server=constants.GOLO_SMTP_SERVER,
                           extra_fields={'X-cbuildbot-alert': 'cq-health'})

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

  def _HealthAlertMessage(self, fail_count):
    """Returns the body of a health alert email message."""
    return 'The builder named %s has failed %i consecutive times. See %s' % (
        self._run.config['name'], fail_count, self.ConstructDashboardURL())

  def _UploadMetadataForRun(self, builder_run, final_status):
    """Upload metadata.json for this entire run.

    Args:
      builder_run: BuilderRun object for this run.
      final_status: Final status string for this run.
    """
    self.UploadMetadata(
        config=builder_run.config, final_status=final_status,
        sync_instance=self._sync_instance,
        completion_instance=self._completion_instance)

  def _UploadArchiveIndex(self, builder_run):
    """Upload an HTML index for the artifacts at remote archive location.

    If there are no artifacts in the archive then do nothing.

    Args:
      builder_run: BuilderRun object for this run.

    Returns:
      If an index file is uploaded then a dict is returned where each value
        is the same (the URL for the uploaded HTML index) and the keys are
        the boards it applies to, including None if applicable.  If no index
        file is uploaded then this returns None.
    """
    archive = builder_run.GetArchive()
    archive_path = archive.archive_path

    config = builder_run.config
    boards = config.boards
    if boards:
      board_names = ' '.join(boards)
    else:
      boards = [None]
      board_names = '<no board>'

    # See if there are any artifacts found for this run.
    uploaded = os.path.join(archive_path, commands.UPLOADED_LIST_FILENAME)
    if not os.path.exists(uploaded):
      # UPLOADED doesn't exist.  Normal if Archive stage never ran, which
      # is possibly normal.  Regardless, no archive index is needed.
      logging.info('No archived artifacts found for %s run (%s)',
                   builder_run.config.name, board_names)

    else:
      # Prepare html head.
      head_data = {
          'board': board_names,
          'config': config.name,
          'version': builder_run.GetVersion(),
      }
      head = self._HTML_HEAD % head_data

      files = osutils.ReadFile(uploaded).splitlines() + [
          '.|Google Storage Index',
          '..|',
      ]
      index = os.path.join(archive_path, 'index.html')
      commands.GenerateHtmlIndex(index, files, url_base=archive.download_url,
                                 head=head)
      commands.UploadArchivedFile(
          archive_path, archive.upload_url, os.path.basename(index),
          debug=self._run.debug, acl=self.acl)

      return dict((b, archive.download_url + '/index.html') for b in boards)

  def PerformStage(self):
    # Make sure local archive directory is prepared, if it was not already.
    # TODO(mtennant): It is not clear how this happens, but a CQ master run
    # that never sees an open tree somehow reaches Report stage without a
    # set up archive directory.
    if not os.path.exists(self.archive_path):
      self.archive.SetupArchivePath()

    if results_lib.Results.BuildSucceededSoFar():
      final_status = constants.FINAL_STATUS_PASSED
    else:
      final_status = constants.FINAL_STATUS_FAILED

    # Upload metadata, and update the pass/fail streak counter for the main
    # run only. These aren't needed for the child builder runs.
    self._UploadMetadataForRun(self._run, final_status)
    self._UpdateRunStreak(self._run, final_status)

    # Iterate through each builder run, whether there is just the main one
    # or multiple child builder runs.
    archive_urls = {}
    for builder_run in self._run.GetUngroupedBuilderRuns():
      # Generate an index for archived artifacts if there are any.  All the
      # archived artifacts for one run/config are in one location, so the index
      # is only specific to each run/config.  In theory multiple boards could
      # share that archive, but in practice it is usually one board.  A
      # run/config without a board will also usually not have artifacts to
      # archive, but that restriction is not assumed here.
      run_archive_urls = self._UploadArchiveIndex(builder_run)
      if run_archive_urls:
        archive_urls.update(run_archive_urls)

        # Also update the LATEST files, since this run did archive something.
        archive = builder_run.GetArchive()
        archive.UpdateLatestMarkers(builder_run.manifest_branch,
                                    builder_run.debug)

    version = getattr(self._run.attrs, 'release_tag', '')
    results_lib.Results.Report(sys.stdout, archive_urls=archive_urls,
                               current_version=version)
