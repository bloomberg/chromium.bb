# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing the base class for the stages that a builder runs."""

import copy
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
from chromite.buildbot import validation_pool
from chromite.lib import cros_build_lib


class BuilderStage(object):
  """Parent class for stages to be performed by a builder."""
  name_stage_re = re.compile(r'(\w+)Stage')

  # TODO(sosa): Remove these once we have a SEND/RECIEVE IPC mechanism
  # implemented.
  overlays = None
  push_overlays = None

  # Class variable that stores the branch to build and test
  _target_manifest_branch = None

  # Class should set this if they have a corresponding no<stage> option that
  # skips their stage.
  option_name = None

  # Class should set this if they have a corresponding setting in
  # self._build_config that skips their stage.
  config_name = None

  @staticmethod
  def SetManifestBranch(branch):
    BuilderStage._target_manifest_branch = branch

  @classmethod
  def StageNamePrefix(cls):
    return cls.name_stage_re.match(cls.__name__).group(1)

  def __init__(self, options, build_config, suffix=None):
    self._options = options
    self._bot_id = build_config['name']
    if not self._options.archive_base and self._options.remote_trybot:
      self._bot_id = 'trybot-' + self._bot_id

    self._build_config = copy.deepcopy(build_config)
    self.name = self.StageNamePrefix()
    if suffix:
      self.name += suffix
    self._boards = self._build_config['boards']
    self._build_root = os.path.abspath(self._options.buildroot)
    self._prebuilt_type = None
    if self._options.prebuilts and self._build_config['prebuilts']:
      self._prebuilt_type = self._build_config['build_type']

    # Determine correct chrome_rev.
    self._chrome_rev = self._build_config['chrome_rev']
    if self._options.chrome_rev:
      self._chrome_rev = self._options.chrome_rev

  def ConstructDashboardURL(self, stage=None):
    """Return the dashboard URL

    This is the direct link to buildbot logs as seen in build.chromium.org

    Args:
      stage: Link to a specific |stage|, otherwise the general buildbot log
    Returns:
      The fully formed URL
    """
    return validation_pool.ValidationPool.ConstructDashboardURL(
        self._build_config['overlays'], self._options.remote_trybot,
        self._build_config['name'], self._options.buildnumber, stage=stage)

  def _ExtractOverlays(self):
    """Extracts list of overlays into class."""
    overlays = portage_utilities.FindOverlays(
        self._build_config['overlays'], buildroot=self._build_root)
    push_overlays = portage_utilities.FindOverlays(
        self._build_config['push_overlays'], buildroot=self._build_root)

    # Sanity checks.
    # We cannot push to overlays that we don't rev.
    assert set(push_overlays).issubset(set(overlays))
    # Either has to be a master or not have any push overlays.
    assert self._build_config['master'] or not push_overlays

    return overlays, push_overlays

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

    envvar: The environment variable to get. E.g. 'PORTAGE_BINHOST'.

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
    """Gets the important builds corresponding to this master.

    Returns:
      A list of the slaves for this builder.
    """
    if configs is None:
      configs = cbuildbot_config.config
    builders = []
    assert build_config['manifest_version']
    assert build_config['master']
    for config in configs.itervalues():
      if (config['important'] and
          config['manifest_version'] and
          config['build_type'] == build_config['build_type'] and
          config['chrome_rev'] == build_config['chrome_rev'] and
          config['branch'] == build_config['branch']):
        builders.append(config)
    return builders

  def _Begin(self):
    """Can be overridden.  Called before a stage is performed."""

    # Tell the buildbot we are starting a new step for the waterfall
    self._Print('\n@@@BUILD_STEP %s@@@\n' % self.name)

    self._PrintLoudly('Start Stage %s - %s\n\n%s' % (
        self.name, time.strftime('%H:%M:%S'), self.__doc__))

  def _Finish(self):
    """Can be overridden.  Called after a stage has been performed."""
    self._PrintLoudly('Finished Stage %s - %s' %
                      (self.name, time.strftime('%H:%M:%S')))

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

  def _HandleExceptionAsWarning(self, exception):
    """Use instead of HandleStageException to treat an exception as a warning.

    This is used by the ForgivingBuilderStage's to treat any exceptions as
    warnings instead of stage failures.
    """
    cros_build_lib.PrintBuildbotStepWarnings()
    cros_build_lib.Warning(self._StringifyException(exception))
    return results_lib.Results.FORGIVEN, None

  def _HandleStageException(self, exception):
    """Called when PerformStage throws an exception.  Can be overriden.

    Should return result, description.  Description should be None if result
    is not an exception.
    """
    # Tell the user about the exception, and record it
    description = self._StringifyException(exception)
    cros_build_lib.PrintBuildbotStepFailure()
    cros_build_lib.Error(description)
    return exception, description

  def HandleSkip(self):
    """Run if the stage is skipped."""
    pass

  def Run(self):
    """Have the builder execute the stage."""
    if (self.option_name and not getattr(self._options, self.option_name) or
        self.config_name and not self._build_config[self.config_name]):
      self._PrintLoudly('Not running Stage %s' % self.name)
      self.HandleSkip()
      results_lib.Results.Record(self.name, results_lib.Results.SKIPPED)
      return

    record = results_lib.Results.PreviouslyCompletedRecord(self.name)
    if record:
      # Success is stored in the results log for a stage that completed
      # successfully in a previous run.
      self._PrintLoudly('Stage %s processed previously' % self.name)
      self.HandleSkip()
      results_lib.Results.Record(self.name, results_lib.Results.SUCCESS, None,
                                 float(record[2]))
      return

    start_time = time.time()

    # Set default values
    result = results_lib.Results.SUCCESS
    description = None

    sys.stdout.flush()
    sys.stderr.flush()
    self._Begin()
    try:
      self.PerformStage()
    except SystemExit as e:
      if e.code != 0:
        result, description = self._HandleStageException(e)
      raise
    except Exception as e:
      if mox is not None and isinstance(e, mox.Error):
        raise
      # Tell the build bot this step failed for the waterfall
      result, description = self._HandleStageException(e)
      if result not in (results_lib.Results.FORGIVEN,
                        results_lib.Results.SUCCESS):
        raise results_lib.StepFailure()
    except BaseException as e:
      result, description = self._HandleStageException(e)
      raise
    finally:
      elapsed_time = time.time() - start_time
      results_lib.Results.Record(self.name, result, description,
                                 time=elapsed_time)
      self._Finish()
      sys.stdout.flush()
      sys.stderr.flush()
