# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Logging Configuration library.

Allows for easy setup of logging in scripts with color output
and plaintext output.

Example usage:

import logging
from lib import logging_config

logging_config.ConfigureConsoleLogger(color=True)
logging.error('This is red')
"""

import copy
import logging
import logging.handlers
import os
import time

import fixup_path
fixup_path.FixupPath()

from chromite.lib.paygen import urilib


# These are ANSI escape codes for printing in color
_RED = '\x1b[31m'
_YELLOW = '\x1b[1;33m'
_MAGENTA = '\x1b[35m'
_GREEN = '\x1b[32m'
_DEFAULT = '\x1b[0m'


class ConsoleHandler(logging.StreamHandler):
  """StreamHandler with color optionally added.

  Use self.setColor(True) or self.setColor(False) to turn color on or off.
  """
  def __init__(self, *args, **kwargs):
    super(ConsoleHandler, self).__init__(*args, **kwargs)
    self.color = False

  def setColor(self, color):
    """Turn colorizing on or off."""
    self.color = color

  def emit(self, record):
    """Override of the logging.StreamHandler emit function."""
    if self.color:
      # Adjust record.levelname to be colorized, but use a copy of record
      # so the record is not affected for other streamhandlers.
      record = copy.copy(record)
      levelno = record.levelno
      if levelno >= 50:
        # Critical / Fatal
        color = _RED
      elif levelno >= 40:
        # Error
        color = _RED
      elif levelno >= 30:
        # Warning
        color = _YELLOW
      elif levelno >= 20:
        # Info
        color = _GREEN
      elif levelno >= 10:
        # Debug
        color = _MAGENTA
      else: # NOTSET and anything else
        color = _DEFAULT

      record.levelname = '%s%s%s' % (color, record.levelname, _DEFAULT)

    super(ConsoleHandler, self).emit(record)


class ListLogger(object):
  """Contextable logger for a list of objects.

  When used as a context, all lines are guaranteed to be output consecutively
  when the context terminates. Otherwise, will log objects directly when
  instructed.
  """
  def __init__(self, title):
    self._title = title + ':'
    self._title_logged = False
    self._lines = None
    self._index = 0

  def __enter__(self):
    self._lines = []
    return self

  def __exit__(self, exc_type, exc_value, exc_traceback):
    if exc_type is not None:
      return False

    if self._lines:
      for line in self._lines:
        self._LogLine(line)
    else:
      self._LogNoObjects()

  def _LogLine(self, line):
    """Logs a single line, preceded by a title if needed."""
    if not self._title_logged:
      logging.info(self._title)
      self._title_logged = True

    logging.info(line)

  def _LogNoObjects(self):
    """Outputs a "no-object" message."""
    self._LogLine(' (no objects listed)')

  def LogItem(self, obj):
    """Logs the next item of the list.

    When used as a context, actual logging is deferred to context termination,
    which guarantees that other logging calls will not interfere with the
    listing of items.

    """
    self._index += 1
    line = ' %d: %s' % (self._index, obj)
    if self._lines is not None:
      self._lines.append(line)
    else:
      self._LogLine(line)

  def LogAll(self, obj_list):
    """Logs all items in the provided list.

    When not used as a context, this is assumed to be the only call made to
    this logger and so it will print a "no object" message in lieu of the
    missing items to be logged.

    """
    if obj_list:
      for obj in obj_list:
        self.LogItem(obj)
    elif self._lines is None:
      self._LogNoObjects()


# Logging Format: 04/04 18:39:14 INFO[log_test-29]: test message
_FORMAT = '%(asctime)s %(levelname)s[%(module)s-%(lineno)d]: %(message)s'
_DATEFMT = '%m/%d %H:%M:%S'


def _GetRootLogger():
  """Get the root logger object, and set DEBUG level on it."""
  root_logger = logging.getLogger()

  # Configure the root logger to show all messages, and let sub handlers off
  # that root logger filter to a different level if they choose.
  root_logger.setLevel(logging.DEBUG)

  return root_logger


_console_handler = None
def _GetConsoleHandler():
  """Get the console handler object, creating and registering it if needed."""
  # pylint: disable=W0603
  global _console_handler
  # There is one and only one handler for output to the console supported,
  # so re-use the same global object that is created on demand.

  if not _console_handler:
    _console_handler = ConsoleHandler()
    _console_handler.setFormatter(logging.Formatter(_FORMAT, datefmt=_DATEFMT))

    # Register the console handler now.
    _GetRootLogger().addHandler(_console_handler)

  return _console_handler


def ConfigureConsoleLogger(level=logging.INFO, color=True):
  """Adjust the level and color of current console handler.

  Args:
    level: Level of logging to display, i.e. logging.ERROR, logging.DEBUG,
      logging.INFO etc. Default: logging.INFO.
    color: If true color is turned on, otherwise color is turned off.
  """
  console_handler = _GetConsoleHandler()
  console_handler.setLevel(level)
  console_handler.setColor(color)


# TODO(mtennant): Deprecated.  Use ConfigureConsoleLogger.
def ColorLogger(level=logging.INFO):
  """Deprecated.  Use ConfigureConsoleLogger instead."""
  ConfigureConsoleLogger(level, True)


# TODO(mtennant): Deprecated.  Use ConfigureConsoleLogger.
def PlainLogger(level=logging.INFO):
  """Deprecated.  Use ConfigureConsoleLogger instead."""
  ConfigureConsoleLogger(level, False)


def AddFileLogger(log_path='debug.log', max_bytes=1024*1024*4,
                  level=logging.DEBUG, backup_count=5, rollover_now=False):
  """Add a rotating file handler for logging to a file.

  Args:
    log_path: The path to the log file.
    max_bytes: Maximum bytes to let the log file grow to before rotation
        kicks in.  Defaults to 4 megs.
    level: Level of logging to display, i.e. logging.ERROR, logging.DEBUG,
        logging.INFO etc. Default: logging.DEBUG.
    backup_count: Number of previous log files to keep around.
    rollover_now: If True, do rollover operation (copy log to log.1, log.1 to
      log.2, etc.) at start of run rather than waiting for max_bytes threshold.
      This effectively makes each log file specific to one run.
  """
  handler = logging.handlers.RotatingFileHandler(log_path, maxBytes=max_bytes,
                                                 backupCount=backup_count)
  handler.setFormatter(logging.Formatter(_FORMAT, datefmt=_DATEFMT))
  handler.setLevel(level)
  _GetRootLogger().addHandler(handler)

  if rollover_now:
    handler.doRollover()


# TODO(mtennant): Deprecated.  Use AddFileLogger.
def FileLogger(*args, **kwargs):
  """Deprecated.  Use AddFileLogger instead."""
  return AddFileLogger(*args, **kwargs)


def ArchiveLog(log_path, archive_dir, strftime_format='%Y%m%d-%H%M%S-UTC',
               archive_basename=None):
  """Copy a log file to an archive directory, adding UTC timestamp to filename.

  Args:
    log_path: Path to log file.  Can be local or on supported
      remote system (e.g. Google Storage).
    archive_dir: Path to directory to archive log in.  Can be any path type
      accepted by urilib.Copy (e.g. local path or Google Storage path).
    strftime_format: String representing strftime format as accepted by
      time.strftime().  If archive_name is not specified, then this
      will be used to add a timestamp suffix to the archived log file name
      (suffix will be inserted before any ending file extension suffix).
    archive_basename: String for name of archived log file.  The string is pass
      through time.strftime() before it is used, so any custom timestamp can
      be included anywhere in the archive_name.

  Returns:
    Full path of archived log file.
  """
  # Construct basename from log_path and strftime_format, if none specified.
  if not archive_basename:
    basename = os.path.basename(log_path)
    basename_no_ext, ext = os.path.splitext(basename)
    archive_basename = '%s-%s%s' % (basename_no_ext, strftime_format, ext)

  # Insert timestamp (with UTC time) that will be used.
  archive_basename = time.strftime(archive_basename, time.gmtime())

  # Copy log file contents to archive destination.
  archive_path = os.path.join(archive_dir, archive_basename)
  urilib.Copy(log_path, archive_path)
  return archive_path
