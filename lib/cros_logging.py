# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Logging module to be used by all scripts.

cros_logging is a wrapper around logging with additional support for NOTICE
level. This is to be used instead of the default logging module. The new
logging level can only be used from here.
"""

from __future__ import print_function

import sys
# pylint: disable=unused-wildcard-import, wildcard-import
from logging import *
# pylint: enable=unused-wildcard-import, wildcard-import

# Have to import shutdown explicitly from logging because it is not included
# in logging's __all__.
# pylint: disable=unused-import
from logging import shutdown
# pylint: enable=unused-import


# Notice Level.
NOTICE = 25
addLevelName(NOTICE, 'NOTICE')


# Notice implementation.
def notice(message, *args, **kwargs):
  """Log 'msg % args' with severity 'NOTICE'."""
  log(NOTICE, message, *args, **kwargs)


# Only buildbot aware entry-points need to spew buildbot specific logs. Require
# user action for the special log lines.
_buildbot_markers_enabled = False
def EnableBuildbotMarkers():
  # pylint: disable=global-statement
  global _buildbot_markers_enabled
  _buildbot_markers_enabled = True


def _PrintForBuildbot(handle, buildbot_tag, *args):
  """Log a line for buildbot.

  This function dumps a line to log recognizable by buildbot if
  EnableBuildbotMarkers has been called. Otherwise, it dumps the same line in a
  human friendly way that buildbot ignores.

  Args:
    handle: The pipe to dump the log to. If None, log to sys.stderr.
    buildbot_tag: A tag specifying the type of buildbot log.
    *args: The rest of the str arguments to be dumped to the log.
  """
  if _buildbot_markers_enabled:
    args_separator = '@'
    args_prefix = '@'
    end_marker = '@@@'
  else:
    args_separator = '; '
    args_prefix = ': '
    end_marker = ''

  # Cast each argument, because we end up getting all sorts of objects from
  # callers.
  suffix = args_separator.join([str(x) for x in args])
  if suffix:
    suffix = args_prefix + suffix
  line = '\n' + end_marker + buildbot_tag + suffix + end_marker + '\n'

  if handle is None:
    handle = sys.stderr
  handle.write(line)


def PrintBuildbotLink(text, url, handle=None):
  """Prints out a link to buildbot."""
  _PrintForBuildbot(handle, 'STEP_LINK', text, url)


def PrintBuildbotStepText(text, handle=None):
  """Prints out stage text to buildbot."""
  _PrintForBuildbot(handle, 'STEP_TEXT', text)


def PrintBuildbotStepWarnings(handle=None):
  """Marks a stage as having warnings."""
  _PrintForBuildbot(handle, 'STEP_WARNINGS')


def PrintBuildbotStepFailure(handle=None):
  """Marks a stage as having failures."""
  _PrintForBuildbot(handle, 'STEP_FAILURE')


def PrintBuildbotStepName(name, handle=None):
  """Marks a step name for buildbot to display."""
  _PrintForBuildbot(handle, 'BUILD_STEP', name)
