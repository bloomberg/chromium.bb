# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing the base class for the stages that a builder runs."""

import os
import re
import sys
import time
import traceback

# We import mox so that we can identify mox exceptions and pass them through
# in our exception handling code.
try:
  import mox
except ImportError:
  mox = None

from chromite.buildbot import cbuildbot_config
from chromite.buildbot import cbuildbot_results as results_lib
from chromite.buildbot import portage_utilities
from chromite.buildbot import repository
from chromite.lib import cros_build_lib


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

    if bool(attempt) != bool(max_retry):
      raise ValueError('max_retry and attempt must be specified together.')

    self._attempt = attempt
    self._max_retry = max_retry

    # Construct self.name, the name string for this stage instance.
    self.name = self._prefix = self.StageNamePrefix()
    if suffix:
      self.name += suffix

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

  @staticmethod
  def _GetSlavesForMaster(build_config, configs=None):
    """Gets the important slave builds corresponding to this master.

    The master itself is eligible to be a slave (of itself) if it has boards.

    Args:
      build_config: A build config for a master builder.
      configs: Option override of cbuildbot_config.config for the list
        of build configs to look through for slaves.

    Returns:
      A list of build configs corresponding to the slaves for the master
        represented by build_config.
    """
    if configs is None:
      configs = cbuildbot_config.config

    builders = []
    assert build_config['manifest_version']
    assert build_config['master']
    for config in configs.itervalues():
      if (config['important'] and
          config['manifest_version'] and
          (not config['master'] or config['boards']) and
          config['build_type'] == build_config['build_type'] and
          config['chrome_rev'] == build_config['chrome_rev'] and
          config['branch'] == build_config['branch']):
        builders.append(config)

    return builders

  def _Begin(self):
    """Can be overridden.  Called before a stage is performed."""

    # Tell the buildbot we are starting a new step for the waterfall
    cros_build_lib.PrintBuildbotStepName(self.name)

    self._PrintLoudly('Start Stage %s - %s\n\n%s' % (
        self.name, cros_build_lib.UserDateTimeFormat(), self.__doc__))

  def _Finish(self):
    """Can be overridden.  Called after a stage has been performed."""
    self._PrintLoudly('Finished Stage %s - %s' %
                      (self.name, cros_build_lib.UserDateTimeFormat()))

  def PerformStage(self):
    """Subclassed stages must override this function to perform what they want
    to be done.
    """

  def _HandleExceptionAsSuccess(self, _exception):
    """Use instead of HandleStageException to ignore an exception."""
    return results_lib.Results.SUCCESS, None

  @staticmethod
  def _StringifyException(exception):
    """Convert an exception into a string.

    This can only be called as part of an except block.
    """
    if isinstance(exception, results_lib.StepFailure):
      return str(exception)
    else:
      return traceback.format_exc()

  def _HandleExceptionAsWarning(self, exception, retrying=False):
    """Use instead of HandleStageException to treat an exception as a warning.

    This is used by the ForgivingBuilderStage's to treat any exceptions as
    warnings instead of stage failures.
    """
    description = self._StringifyException(exception)
    cros_build_lib.PrintBuildbotStepWarnings()
    cros_build_lib.Warning(description)
    return results_lib.Results.FORGIVEN, description, retrying

  def _HandleStageException(self, exception):
    """Called when PerformStage throws an exception.  Can be overriden.

    Should return result, description.  Description should be None if result
    is not an exception.
    """
    if self._attempt and self._max_retry and self._attempt <= self._max_retry:
      return self._HandleExceptionAsWarning(exception, retrying=True)
    else:
      # Tell the user about the exception, and record it
      retrying = False
      description = self._StringifyException(exception)
      cros_build_lib.PrintBuildbotStepFailure()
      cros_build_lib.Error(description)
      return exception, description, retrying

  def HandleSkip(self):
    """Run if the stage is skipped."""
    pass

  def Run(self):
    """Have the builder execute the stage."""
    # See if this stage should be skipped.
    if (self.option_name and not getattr(self._run.options, self.option_name) or
        self.config_name and not getattr(self._run.config, self.config_name)):
      self._PrintLoudly('Not running Stage %s' % self.name)
      self.HandleSkip()
      results_lib.Results.Record(self.name, results_lib.Results.SKIPPED,
                                 prefix=self._prefix)
      return

    record = results_lib.Results.PreviouslyCompletedRecord(self.name)
    if record:
      # Success is stored in the results log for a stage that completed
      # successfully in a previous run.
      self._PrintLoudly('Stage %s processed previously' % self.name)
      self.HandleSkip()
      results_lib.Results.Record(self.name, results_lib.Results.SUCCESS,
                                 prefix=self._prefix, time=float(record.time))
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
        result, description, retrying = self._HandleStageException(e)

      raise
    except Exception as e:
      if mox is not None and isinstance(e, mox.Error):
        raise

      # Tell the build bot this step failed for the waterfall.
      result, description, retrying = self._HandleStageException(e)
      if result not in (results_lib.Results.FORGIVEN,
                        results_lib.Results.SUCCESS):
        raise results_lib.StepFailure()
      elif retrying:
        raise results_lib.RetriableStepFailure()
    except BaseException as e:
      result, description, retrying = self._HandleStageException(e)
      raise
    finally:
      elapsed_time = time.time() - start_time
      results_lib.Results.Record(self.name, result, description,
                                 prefix=self._prefix, time=elapsed_time)
      self._Finish()
      sys.stdout.flush()
      sys.stderr.flush()
