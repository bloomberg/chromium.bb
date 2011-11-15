# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing the base class for the stages that a builder runs."""

import os
import re
import sys
import time
import traceback

from chromite.buildbot import cbuildbot_results as results_lib
from chromite.lib import cros_build_lib as cros_lib

PUBLIC_OVERLAY = '%(buildroot)s/src/third_party/chromiumos-overlay'
OVERLAY_LIST_CMD = '%(buildroot)s/src/platform/dev/host/cros_overlay_list'

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
  _tracking_branch = None

  # Class should set this if they have a corresponding no<stage> option that
  # skips their stage.
  option_name = None

  @staticmethod
  def SetTrackingBranch(tracking_branch):
    BuilderStage._tracking_branch = tracking_branch

  def __init__(self, bot_id, options, build_config):
    self._bot_id = bot_id
    self._options = options
    self._build_config = build_config
    self._prebuilt_type = None
    self.name = self.name_stage_re.match(self.__class__.__name__).group(1)
    self._ExtractVariables()

    # Determine correct chrome_rev.
    self._chrome_rev = self._build_config['chrome_rev']
    if self._options.chrome_rev: self._chrome_rev = self._options.chrome_rev

  def _ExtractVariables(self):
    """Extracts common variables from build config and options into class."""
    self._build_root = os.path.abspath(self._options.buildroot)
    if self._options.prebuilts and self._build_config['prebuilts']:
      self._prebuilt_type = self._build_config['build_type']

  def _ExtractOverlays(self):
    """Extracts list of overlays into class."""
    overlays = self._ResolveOverlays(self._build_config['overlays'])
    push_overlays = self._ResolveOverlays(self._build_config['push_overlays'])

    # Sanity checks.
    # We cannot push to overlays that we don't rev.
    assert set(push_overlays).issubset(set(overlays))
    # Either has to be a master or not have any push overlays.
    assert self._build_config['master'] or not push_overlays

    return overlays, push_overlays

  def _ListifyBoard(self, board):
    """Return list of boards from either str or list |board|."""
    boards = None
    if isinstance(board, str):
      boards = [board]
    else:
      boards = board

    assert isinstance(boards, list), 'Board was neither an array or a string.'
    return boards

  def _ResolveOverlays(self, overlays):
    """Return the list of overlays to use for a given buildbot.

    Args:
      overlays: A string describing which overlays you want.
                'private': Just the private overlay.
                'public': Just the public overlay.
                'both': Both the public and private overlays.
    """
    cmd = OVERLAY_LIST_CMD % {'buildroot': self._build_root}
    # Check in case we haven't checked out the source yet.
    if not os.path.exists(cmd):
      return []

    public_overlays = cros_lib.RunCommand([cmd, '--all_boards', '--noprivate'],
                                          redirect_stdout=True,
                                          print_cmd=False).output.split()
    private_overlays = cros_lib.RunCommand([cmd, '--all_boards', '--nopublic'],
                                           redirect_stdout=True,
                                           print_cmd=False).output.split()

    # TODO(davidjames): cros_overlay_list should include chromiumos-overlay in
    #                   its list of public overlays. But it doesn't yet...
    public_overlays.append(PUBLIC_OVERLAY % {'buildroot': self._build_root})

    if overlays == 'private':
      paths = private_overlays
    elif overlays == 'public':
      paths = public_overlays
    elif overlays == 'both':
      paths = public_overlays + private_overlays
    else:
      cros_lib.Info('No overlays found.')
      paths = []

    return paths

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
    binhost = cros_lib.OldRunCommand(
        [portageq, 'envvar', envvar], cwd=cwd, redirect_stdout=True,
        enter_chroot=True, error_ok=True)
    return binhost.rstrip('\n')

  def _GetImportantBuildersForMaster(self, config):
    """Gets the important builds corresponding to this master builder.

    Given that we are a master builder, find all corresponding slaves that
    are important to me.  These are those builders that share the same
    build_type and manifest_version url.
    """
    builders = []
    build_type = self._build_config['build_type']
    overlay_config = self._build_config['overlays']
    use_manifest_version = self._build_config['manifest_version']
    for build_name, config in config.iteritems():
      if (config['important'] and config['build_type'] == build_type and
          config['chrome_rev'] == self._chrome_rev and
          config['overlays'] == overlay_config and
          config['manifest_version'] == use_manifest_version):
        builders.append(build_name)

    return builders

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

  def GetImageDirSymlink(self, pointer='latest-cbuildbot'):
    """Get the location of the current image."""
    buildroot, board = self._options.buildroot, self._build_config['board']
    return os.path.join(buildroot, 'src', 'build', 'images', board, pointer)

  def HandleSkip(self):
    """Run if the stage is skipped."""
    pass

  def Run(self):
    """Have the builder execute the stage."""
    if self.option_name and not getattr(self._options, self.option_name):
      self._PrintLoudly('Not running Stage %s' % self.name)
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
    except Exception as e:
      # Tell the build bot this step failed for the waterfall
      result, description = self._HandleStageException(e)
      raise NonBacktraceBuildException()
    finally:
      elapsed_time = time.time() - start_time
      results_lib.Results.Record(self.name, result, description,
                                 time=elapsed_time)
      self._Finish()
