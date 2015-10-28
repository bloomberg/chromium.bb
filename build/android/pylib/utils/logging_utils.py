# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import logging
import os
import sys

from pylib import constants
sys.path.insert(0, os.path.join(constants.DIR_SOURCE_ROOT,
                                'third_party', 'colorama', 'src'))
import colorama

class ColorStreamHandler(logging.StreamHandler):
  """Handler that can be used to colorize logging output.

  Example using a specific logger:

    logger = logging.getLogger('my_logger')
    logger.addHandler(ColorStreamHandler())
    logger.info('message')

  Example using the root logger:

    ColorStreamHandler.MakeDefault()
    logging.info('message')

  """
  # pylint does not see members added dynamically in the constructor.
  # pylint: disable=no-member
  color_map = {
    logging.DEBUG: colorama.Fore.CYAN,
    logging.WARNING: colorama.Fore.YELLOW,
    logging.ERROR: colorama.Fore.RED,
    logging.CRITICAL: colorama.Back.RED + colorama.Style.BRIGHT,
  }

  @property
  def is_tty(self):
    isatty = getattr(self.stream, 'isatty', None)
    return isatty and isatty()

  #override
  def format(self, record):
    message = logging.StreamHandler.format(self, record)
    if self.is_tty:
      return self.Colorize(message, record.levelno)

  def Colorize(self, message, log_level):
    try:
      return self.color_map[log_level] + message + colorama.Style.RESET_ALL
    except KeyError:
      return message

  @staticmethod
  def MakeDefault():
     """
     Replaces the default logging handlers with a coloring handler. To use
     a colorizing handler at the same time as others, either register them
     after this call, or add the ColorStreamHandler on the logger using
     Logger.addHandler()
     """
     # If the existing handlers aren't removed, messages are duplicated
     logging.getLogger().handlers = []
     logging.getLogger().addHandler(ColorStreamHandler())


@contextlib.contextmanager
def SuppressLogging(level=logging.ERROR):
  """Momentarilly suppress logging events from all loggers.

  TODO(jbudorick): This is not thread safe. Log events from other threads might
  also inadvertently dissapear.

  Example:

    with logging_utils.SuppressLogging():
      # all but CRITICAL logging messages are suppressed
      logging.info('just doing some thing') # not shown
      logging.critical('something really bad happened') # still shown

  Args:
    level: logging events with this or lower levels are suppressed.
  """
  logging.disable(level)
  yield
  logging.disable(logging.NOTSET)
