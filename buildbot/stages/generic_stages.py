# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing the generic stages."""

import contextlib
import fnmatch
import json
import os
import re
import sys
import time
import traceback

# We import mox so that we can identify mox exceptions and pass them through
# in our exception handling code.
try:
  # pylint: disable=F0401
  import mox
except ImportError:
  mox = None

from chromite.buildbot import cbuildbot_config
from chromite.buildbot import cbuildbot_commands as commands
from chromite.buildbot import cbuildbot_failures as failures_lib
from chromite.buildbot import cbuildbot_results as results_lib
from chromite.buildbot import constants
from chromite.buildbot import portage_utilities
from chromite.buildbot import repository
from chromite.lib import cros_build_lib
from chromite.lib import gs
from chromite.lib import osutils
from chromite.lib import parallel
from chromite.lib import retry_util
from chromite.lib import timeout_util


class BuilderStage(object):
  """Parent class for stages to be performed by a builder."""
  # Used to remove 'Stage' suffix of stage class when generating stage name.
  name_stage_re = re.compile(r'(\w+)Stage')

  # TODO(sosa): Remove these once we have a SEND/RECIEVE IPC mechanism
  # implemented.
  overlays = None
  push_overlays = None

  # Class should set this if they have a corresponding no<stage> option that
  # skips their stage.
  # TODO(mtennant): Rename this something like skip_option_name.
  option_name = None

  # Class should set this if they have a corresponding setting in
  # the build_config that skips their stage.
  # TODO(mtennant): Rename this something like skip_config_name.
  config_name = None

  @classmethod
  def StageNamePrefix(cls):
    """Return cls.__name__ with any 'Stage' suffix removed."""
    match = cls.name_stage_re.match(cls.__name__)
    assert match, 'Class name %s does not end with Stage' % cls.__name__
    return match.group(1)

  def __init__(self, builder_run, suffix=None, attempt=None, max_retry=None):
    """Create a builder stage.

    Args:
      builder_run: The BuilderRun object for the run this stage is part of.
      suffix: The suffix to append to the buildbot name. Defaults to None.
      attempt: If this build is to be retried, the current attempt number
        (starting from 1). Defaults to None. Is only valid if |max_retry| is
        also specified.
      max_retry: The maximum number of retries. Defaults to None. Is only valid
        if |attempt| is also specified.
    """
    self._run = builder_run

    self._attempt = attempt
    self._max_retry = max_retry

    # Construct self.name, the name string for this stage instance.
    self.name = self._prefix = self.StageNamePrefix()
    if suffix:
      self.name += suffix

    # Construct the first buildbot name for this stage.
    self._bb_name = self.name

    # TODO(mtennant): Phase this out and use self._run.bot_id directly.
    self._bot_id = self._run.bot_id

    # self._boards holds list of boards involved in this run.
    # TODO(mtennant): Replace self._boards with a self._run.boards?
    self._boards = self._run.config.boards

    # TODO(mtennant): Try to rely on just self._run.buildroot directly, if
    # the os.path.abspath can be applied there instead.
    self._build_root = os.path.abspath(self._run.buildroot)
    self._prebuilt_type = None
    if self._run.ShouldUploadPrebuilts():
      self._prebuilt_type = self._run.config.build_type

    # Determine correct chrome_rev.
    self._chrome_rev = self._run.config.chrome_rev
    if self._run.options.chrome_rev:
      self._chrome_rev = self._run.options.chrome_rev

    # USE and enviroment variable settings.
    self._portage_extra_env = {}
    useflags = self._run.config.useflags[:]

    if self._run.options.clobber:
      self._portage_extra_env['IGNORE_PREFLIGHT_BINHOST'] = '1'

    if self._run.options.chrome_root:
      self._portage_extra_env['CHROME_ORIGIN'] = 'LOCAL_SOURCE'

    self._latest_toolchain = (self._run.config.latest_toolchain or
                              self._run.options.latest_toolchain)
    if self._latest_toolchain and self._run.config.gcc_githash:
      useflags.append('git_gcc')
      self._portage_extra_env['GCC_GITHASH'] = self._run.config.gcc_githash

    if useflags:
      self._portage_extra_env['USE'] = ' '.join(useflags)

  def GetStageNames(self):
    """Get a list of the places where this stage has recorded results."""
    return [self.name]

  # TODO(akeshet): Eliminate this method and update the callers to use
  # builder run directly.
  def ConstructDashboardURL(self, stage=None):
    """Return the dashboard URL

    This is the direct link to buildbot logs as seen in build.chromium.org

    Args:
      stage: Link to a specific |stage|, otherwise the general buildbot log

    Returns:
      The fully formed URL
    """
    return self._run.ConstructDashboardURL(stage=stage)

  def _ExtractOverlays(self):
    """Extracts list of overlays into class."""
    overlays = portage_utilities.FindOverlays(
        self._run.config.overlays, buildroot=self._build_root)
    push_overlays = portage_utilities.FindOverlays(
        self._run.config.push_overlays, buildroot=self._build_root)

    # Sanity checks.
    # We cannot push to overlays that we don't rev.
    assert set(push_overlays).issubset(set(overlays))
    # Either has to be a master or not have any push overlays.
    assert self._run.config.master or not push_overlays

    return overlays, push_overlays

  def GetRepoRepository(self, **kwargs):
    """Create a new repo repository object."""
    manifest_url = self._run.options.manifest_repo_url
    if manifest_url is None:
      manifest_url = self._run.config.manifest_repo_url

    kwargs.setdefault('referenced_repo', self._run.options.reference_repo)
    kwargs.setdefault('branch', self._run.manifest_branch)
    kwargs.setdefault('manifest', self._run.config.manifest)

    return repository.RepoRepository(manifest_url, self._build_root, **kwargs)

  def _Print(self, msg):
    """Prints a msg to stderr."""
    sys.stdout.flush()
    print >> sys.stderr, msg
    sys.stderr.flush()

  def _PrintLoudly(self, msg):
    """Prints a msg with loudly."""

    border_line = '*' * 60
    edge = '*' * 2

    sys.stdout.flush()
    print >> sys.stderr, border_line

    msg_lines = msg.split('\n')

    # If the last line is whitespace only drop it.
    if not msg_lines[-1].rstrip():
      del msg_lines[-1]

    for msg_line in msg_lines:
      print >> sys.stderr, '%s %s' % (edge, msg_line)

    print >> sys.stderr, border_line
    sys.stderr.flush()

  def _GetPortageEnvVar(self, envvar, board):
    """Get a portage environment variable for the configuration's board.

    Args:
      envvar: The environment variable to get. E.g. 'PORTAGE_BINHOST'.
      board: The board to apply, if any.  Specify None to use host.

    Returns:
      The value of the environment variable, as a string. If no such variable
      can be found, return the empty string.
    """
    cwd = os.path.join(self._build_root, 'src', 'scripts')
    if board:
      portageq = 'portageq-%s' % board
    else:
      portageq = 'portageq'
    binhost = cros_build_lib.RunCommand(
        [portageq, 'envvar', envvar], cwd=cwd, redirect_stdout=True,
        enter_chroot=True, error_code_ok=True)
    return binhost.output.rstrip('\n')

  def _GetSlaveConfigs(self):
    """Get the slave configs for the current build config.

    This assumes self._run.config is a master config.

    Returns:
      A list of build configs corresponding to the slaves for the master
        build config at self._run.config.

    Raises:
      See cbuildbot_config.GetSlavesForMaster for details.
    """
    return cbuildbot_config.GetSlavesForMaster(self._run.config)

  def _Begin(self):
    """Can be overridden.  Called before a stage is performed."""

    # Tell the buildbot we are starting a new step for the waterfall
    # and late bind the buildbot name, because renames before this should
    # take effect.
    self._bb_name = self.name
    cros_build_lib.PrintBuildbotStepName(self._bb_name)

    self._PrintLoudly('Start Stage %s - %s\n\n%s' % (
        self.name, cros_build_lib.UserDateTimeFormat(), self.__doc__))

  def _Finish(self):
    """Can be overridden.  Called after a stage has been performed."""
    self._PrintLoudly('Finished Stage %s - %s' %
                      (self.name, cros_build_lib.UserDateTimeFormat()))

    # Tell the buildbot we finished the step for the waterfall
    cros_build_lib.PrintBuildbotStepClose(self._bb_name)

  def _RenameStep(self, name):
    """Finishes being known by one buildbot name, and starts a new one."""
    cros_build_lib.PrintBuildbotStepClose(self._bb_name)
    self._bb_name = name
    cros_build_lib.PrintBuildbotStepName(self._bb_name)

  def PerformStage(self):
    """Subclassed stages must override this function to perform what they want
    to be done.
    """

  def _HandleExceptionAsSuccess(self, _exc_info):
    """Use instead of HandleStageException to ignore an exception."""
    return results_lib.Results.SUCCESS, None

  @staticmethod
  def _StringifyException(exc_info):
    """Convert an exception into a string.

    Args:
      exc_info: A (type, value, traceback) tuple as returned by sys.exc_info().

    Returns:
      A string description of the exception.
    """
    exc_type, exc_value = exc_info[:2]
    if issubclass(exc_type, failures_lib.StepFailure):
      return str(exc_value)
    else:
      return ''.join(traceback.format_exception(*exc_info))

  @classmethod
  def _HandleExceptionAsWarning(cls, exc_info, retrying=False):
    """Use instead of HandleStageException to treat an exception as a warning.

    This is used by the ForgivingBuilderStage's to treat any exceptions as
    warnings instead of stage failures.
    """
    description = cls._StringifyException(exc_info)
    cros_build_lib.PrintBuildbotStepWarnings()
    cros_build_lib.Warning(description)
    return (results_lib.Results.FORGIVEN, description, retrying)

  @classmethod
  def _HandleExceptionAsError(cls, exc_info):
    """Handle an exception as an error, but ignore stage retry settings.

    Meant as a helper for _HandleStageException code only.

    Args:
      exc_info: A (type, value, traceback) tuple as returned by sys.exc_info().

    Returns:
      Result tuple of (exception, description, retrying).
    """
    # Tell the user about the exception, and record it.
    retrying = False
    description = cls._StringifyException(exc_info)
    cros_build_lib.PrintBuildbotStepFailure()
    cros_build_lib.Error(description)
    return (exc_info[1], description, retrying)

  def _HandleStageException(self, exc_info):
    """Called when PerformStage throws an exception.  Can be overriden.

    Args:
      exc_info: A (type, value, traceback) tuple as returned by sys.exc_info().

    Returns:
      Result tuple of (exception, description, retrying).  If it isn't an
      exception, then description will be None.
    """
    if self._attempt and self._max_retry and self._attempt <= self._max_retry:
      return self._HandleExceptionAsWarning(exc_info, retrying=True)
    else:
      return self._HandleExceptionAsError(exc_info)

  def _TopHandleStageException(self):
    """Called when PerformStage throws an unhandled exception.

    Should only be called by the Run function.  Provides a wrapper around
    _HandleStageException to handle buggy handlers.  We must go deeper...
    """
    exc_info = sys.exc_info()
    try:
      return self._HandleStageException(exc_info)
    except Exception:
      cros_build_lib.Error(
          'An exception was thrown while running _HandleStageException')
      cros_build_lib.Error('The original exception was:', exc_info=exc_info)
      cros_build_lib.Error('The new exception is:', exc_info=True)
      return self._HandleExceptionAsError(exc_info)

  def HandleSkip(self):
    """Run if the stage is skipped."""
    pass

  def _RecordResult(self, *args, **kwargs):
    """Record a successful or failed result."""
    results_lib.Results.Record(*args, **kwargs)

  def Run(self):
    """Have the builder execute the stage."""
    # See if this stage should be skipped.
    if (self.option_name and not getattr(self._run.options, self.option_name) or
        self.config_name and not getattr(self._run.config, self.config_name)):
      self._PrintLoudly('Not running Stage %s' % self.name)
      self.HandleSkip()
      self._RecordResult(self.name, results_lib.Results.SKIPPED,
                         prefix=self._prefix)
      return

    record = results_lib.Results.PreviouslyCompletedRecord(self.name)
    if record:
      # Success is stored in the results log for a stage that completed
      # successfully in a previous run.
      self._PrintLoudly('Stage %s processed previously' % self.name)
      self.HandleSkip()
      self._RecordResult(self.name, results_lib.Results.SUCCESS,
                         prefix=self._prefix, board=record.board,
                         time=float(record.time))
      return

    start_time = time.time()

    # Set default values
    result = results_lib.Results.SUCCESS
    description = None

    sys.stdout.flush()
    sys.stderr.flush()
    self._Begin()
    try:
      # TODO(davidjames): Verify that PerformStage always returns None. See
      # crbug.com/264781
      self.PerformStage()
    except SystemExit as e:
      if e.code != 0:
        result, description, retrying = self._TopHandleStageException()

      raise
    except Exception as e:
      if mox is not None and isinstance(e, mox.Error):
        raise

      # Tell the build bot this step failed for the waterfall.
      result, description, retrying = self._TopHandleStageException()
      if result not in (results_lib.Results.FORGIVEN,
                        results_lib.Results.SUCCESS):
        raise failures_lib.StepFailure()
      elif retrying:
        raise failures_lib.RetriableStepFailure()
    except BaseException:
      result, description, retrying = self._TopHandleStageException()
      raise
    finally:
      elapsed_time = time.time() - start_time
      self._RecordResult(self.name, result, description, prefix=self._prefix,
                         time=elapsed_time)
      self._Finish()
      sys.stdout.flush()
      sys.stderr.flush()


class NonHaltingBuilderStage(BuilderStage):
  """Build stage that fails a build but finishes the other steps."""

  def Run(self):
    try:
      super(NonHaltingBuilderStage, self).Run()
    except failures_lib.StepFailure:
      name = self.__class__.__name__
      cros_build_lib.Error('Ignoring StepFailure in %s', name)


class ForgivingBuilderStage(BuilderStage):
  """Build stage that turns a build step red but not a build."""

  def _HandleStageException(self, exc_info):
    """Override and don't set status to FAIL but FORGIVEN instead."""
    return self._HandleExceptionAsWarning(exc_info)


class RetryStage(object):
  """Retry a given stage multiple times to see if it passes."""

  def __init__(self, builder_run, max_retry, stage, *args, **kwargs):
    """Create a RetryStage object.

    Args:
      builder_run: See arguments to BuilderStage.__init__()
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
        failures_lib.RetriableStepFailure, self.max_retry, self._PerformStage)


class RepeatStage(object):
  """Run a given stage multiple times to see if it fails."""

  def __init__(self, builder_run, count, stage, *args, **kwargs):
    """Create a RepeatStage object.

    Args:
      builder_run: See arguments to BuilderStage.__init__()
      count: The number of times to try the given stage.
      stage: The stage class to create.
      *args: A list of arguments to pass to the stage constructor.
      **kwargs: A list of keyword arguments to pass to the stage constructor.
    """
    self._run = builder_run
    self.count = count
    self.stage = stage
    self.args = (builder_run,) + args
    self.kwargs = kwargs
    self.names = []
    self.attempt = None

  def GetStageNames(self):
    """Get a list of the places where this stage has recorded results."""
    return self.names[:]

  def _PerformStage(self):
    """Run the stage once."""
    suffix = ' (attempt %d)' % (self.attempt,)
    stage_obj = self.stage(
        *self.args, attempt=self.attempt, suffix=suffix, **self.kwargs)
    self.names.extend(stage_obj.GetStageNames())
    stage_obj.Run()

  def Run(self):
    """Retry the given stage multiple times to see if it passes."""
    for i in range(self.count):
      self.attempt = i + 1
      self._PerformStage()


class BoardSpecificBuilderStage(BuilderStage):
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

  def _IsInUploadBlacklist(self, filename):
    """Check if this file is blacklisted to go into a board's extra buckets.

    Args:
      filename: The filename of the file we want to check is in the blacklist.

    Returns:
      True if the file is blacklisted, False otherwise.
    """
    for blacklisted_file in constants.EXTRA_BUCKETS_FILES_BLACKLIST:
      if fnmatch.fnmatch(filename, blacklisted_file):
        return True
    return False

  def _GetUploadUrls(self, filename):
    """Returns a list of all urls for which to upload filename to.

    Args:
      filename: The filename of the file we want to upload.
    """
    urls = [self.upload_url]
    if (not self._IsInUploadBlacklist(filename) and
        hasattr(self, '_current_board')):
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
    upload_urls = self._GetUploadUrls(filename)
    try:
      commands.UploadArchivedFile(
          self.archive_path, upload_urls, filename, self._run.debug,
          update_list=True, acl=self.acl)
    except (gs.GSContextException, timeout_util.TimeoutError):
      cros_build_lib.PrintBuildbotStepText('Upload failed')
      if strict:
        raise

      # Treat gsutil flake as a warning if it's the only problem.
      self._HandleExceptionAsWarning(sys.exc_info())

  def UploadMetadata(self, upload_queue=None, filename=None):
    """Create and upload JSON file of the builder run's metadata.

    This uses the existing metadata stored in the builder run. The default
    metadata.json file should only be uploaded once, at the end of the run,
    and considered immutable. During the build, intermediate metadata snapshots
    can be uploaded to other files, such as partial-metadata.json.

    Args:
      upload_queue: If specified then put the artifact file to upload on
        this queue.  If None then upload it directly now.
      filename: Name of file to dump metadata to.
                Defaults to constants.METADATA_JSON
    """
    filename = filename or constants.METADATA_JSON

    metadata_json = os.path.join(self.archive_path, filename)

    # Stages may run in parallel, so we have to do atomic updates on this.
    cros_build_lib.Info('Writing metadata to %s.', metadata_json)
    osutils.WriteFile(metadata_json, self._run.attrs.metadata.GetJSON(),
                      atomic=True, makedirs=True)

    if upload_queue is not None:
      cros_build_lib.Info('Adding metadata file %s to upload queue.',
                          metadata_json)
      upload_queue.put([filename])
    else:
      cros_build_lib.Info('Uploading metadata file %s now.', metadata_json)
      self.UploadArtifact(filename, archive=False)
