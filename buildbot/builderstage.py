# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing the base class for the stages that a builder runs."""

import os
import re
import sys
import time
import traceback

from chromite.buildbot import cbuildbot_config
from chromite.buildbot import cbuildbot_results as results_lib
from chromite.buildbot import portage_utilities
from chromite.lib import cros_build_lib as cros_lib

class NonBacktraceBuildException(Exception):
  pass

class BuilderStage(object):
  """Parent class for stages to be performed by a builder."""
  name_stage_re = re.compile('(\w+)Stage')

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

  def __init__(self, options, build_config, suffix=None):
    self._bot_id = build_config['name']
    self._options = options
    self._build_config = build_config
    self.name = self.name_stage_re.match(self.__class__.__name__).group(1)
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

  def _ExtractOverlays(self):
    """Extracts list of overlays into class."""
    overlays = portage_utilities.FindOverlays(
        self._build_root, self._build_config['overlays'])
    push_overlays = portage_utilities.FindOverlays(
        self._build_root, self._build_config['push_overlays'])

    # Sanity checks.
    # We cannot push to overlays that we don't rev.
    assert set(push_overlays).issubset(set(overlays))
    # Either has to be a master or not have any push overlays.
    assert self._build_config['master'] or not push_overlays

    return overlays, push_overlays

  def _PrintLoudly(self, msg):
    """Prints a msg with loudly."""

    border_line = '*' * 60
    edge = '*' * 2

    print border_line

    msg_lines = msg.split('\n')

    # If the last line is whitespace only drop it.
    if not msg_lines[-1].rstrip():
      del msg_lines[-1]

    for msg_line in msg_lines:
      print '%s %s' % (edge, msg_line)

    print border_line

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
    binhost = cros_lib.RunCommand(
        [portageq, 'envvar', envvar], cwd=cwd, redirect_stdout=True,
        enter_chroot=True, error_ok=True)
    return binhost.output.rstrip('\n')

  # pylint: disable=W0102
  def _GetSlavesForMaster(self, configs=cbuildbot_config.config):
    """Gets the important builds corresponding to this master builder.

    Given that we are a master builder, find all corresponding slaves that
    are important to me.  These are those builders that share the same
    build_type and manifest_version url.
    """
    builders = []
    build_type = self._build_config['build_type']
    branch_config = self._build_config['branch']
    overlay_config = self._build_config['overlays']
    assert not self._build_config['unified_manifest_version']
    assert self._build_config['manifest_version']
    assert self._build_config['master']
    for build_name, config in configs.iteritems():
      if (config['important'] and config['manifest_version'] and
          not config['unified_manifest_version'] and
          config['build_type'] == build_type and
          config['chrome_rev'] == self._chrome_rev and
          config['overlays'] == overlay_config and
          config['branch'] == branch_config):
        builders.append(build_name)

    return builders

  # pylint: disable=W0102
  def _GetSlavesForUnifiedMaster(self, configs=cbuildbot_config.config):
    """Gets the important builds corresponding to this unified master.

    A unified master has both private and public slaves that read from two
    separate manifest_versions repositories.

    Returns:
      A tuple consisting of the public slaves and private slaves for this
      unified builder.
    """
    public_builders = []
    private_builders = []
    build_type = self._build_config['build_type']
    branch_config = self._build_config['branch']
    assert self._build_config['unified_manifest_version']
    assert self._build_config['manifest_version']
    assert self._build_config['master']
    for build_name, config in configs.iteritems():
      if (config['important'] and config['unified_manifest_version'] and
          config['manifest_version'] and
          config['build_type'] == build_type and
          config['chrome_rev'] == self._chrome_rev and
          config['branch'] == branch_config):
        if config['internal']:
          private_builders.append(build_name)
        else:
          public_builders.append(build_name)

    return public_builders, private_builders

  def _Begin(self):
    """Can be overridden.  Called before a stage is performed."""

    # Tell the buildbot we are starting a new step for the waterfall
    print '\n@@@BUILD_STEP %s@@@\n' % self.name

    self._PrintLoudly('Start Stage %s - %s\n\n%s' % (
        self.name, time.strftime('%H:%M:%S'), self.__doc__))

  def _Finish(self):
    """Can be overridden.  Called after a stage has been performed."""
    self._PrintLoudly('Finished Stage %s - %s' %
                      (self.name, time.strftime('%H:%M:%S')))

  def _PerformStage(self):
    """Subclassed stages must override this function to perform what they want
    to be done.
    """
    pass

  def _HandleExceptionAsWarning(self, exception):
    print '\n@@@STEP_WARNINGS@@@'
    description = traceback.format_exc()
    print >> sys.stderr, description
    return results_lib.Results.FORGIVEN, None

  def _HandleStageException(self, exception):
    """Called when _PerformStages throws an exception.  Can be overriden.

    Should return result, description.  Description should be None if result
    is not an exception.
    """
    # Tell the user about the exception, and record it
    print '\n@@@STEP_FAILURE@@@'
    description = None
    if isinstance(exception, NonBacktraceBuildException):
      description = str(exception)
    else:
      description = traceback.format_exc()
    print >> sys.stderr, description
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
      return

    record = results_lib.Results.PreviouslyCompletedRecord(self.name)
    if record:
      self._PrintLoudly('Skipping Stage %s' % self.name)
      self.HandleSkip()
      results_lib.Results.Record(self.name, results_lib.Results.SUCCESS, None,
                                 float(record[2]))
      return

    start_time = time.time()

    # Set default values
    result = results_lib.Results.SUCCESS
    description = None

    self._Begin()
    try:
      self._PerformStage()
    except SystemExit as e:
      if e.code != 0:
        result, description = self._HandleStageException(e)
      raise
    except Exception as e:
      # Tell the build bot this step failed for the waterfall
      result, description = self._HandleStageException(e)
      if result not in (results_lib.Results.FORGIVEN,
                        results_lib.Results.SUCCESS):
        raise NonBacktraceBuildException()
    finally:
      elapsed_time = time.time() - start_time
      results_lib.Results.Record(self.name, result, description,
                                 time=elapsed_time)
      self._Finish()
