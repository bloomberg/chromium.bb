# -*- coding: utf-8 -*-
# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A logging strategy for cros-update.

1. Logging format is set as the same as autoserv.
2. Globally, set log level of output as 'DEBUG'.
3. For control output, set LEVEL as 'INFO', to avoid unnecessary logs in
   cherrypy logs.
4. Add file handler to record all logs above level 'DEBUG' into file.
"""

from __future__ import print_function

import sys

from chromite.lib import cros_logging as logging


class loggingConfig(object):
  """Configuration for auto-update logging."""

  LOGGING_FORMAT = ('%(asctime)s.%(msecs)03d %(levelname)-5.5s|%(module)18.18s:'
                    '%(lineno)4.4d| %(message)s')

  def __init__(self):
    self.logger = logging.getLogger()
    self.GLOBAL_LEVEL = logging.DEBUG
    self.CONSOLE_LEVEL = logging.INFO
    self.FILE_LEVEL = logging.DEBUG
    self.ENABLE_CONSOLE_LOGGING = False

  def SetControlHandler(self, stream):
    """Set console handler for logging.

    Args:
      stream: The input stream, could be stdout/stderr.
    """
    handler = logging.StreamHandler(stream)
    handler.setLevel(self.CONSOLE_LEVEL)
    file_formatter = logging.Formatter(fmt=self.LOGGING_FORMAT,
                                       datefmt='%Y/%m/%d %H:%M:%S')
    handler.setFormatter(file_formatter)
    self.logger.addHandler(handler)
    return handler

  def SetFileHandler(self, file_name):
    """Set file handler for logging.

    Args:
      file_name: The file to save logs into.
    """
    handler = logging.FileHandler(file_name)
    handler.setLevel(self.FILE_LEVEL)
    # file format is set as same as console format.
    file_formatter = logging.Formatter(fmt=self.LOGGING_FORMAT,
                                       datefmt='%Y/%m/%d %H:%M:%S')
    handler.setFormatter(file_formatter)
    self.logger.addHandler(handler)

  def ConfigureLogging(self):
    self.logger.setLevel(self.GLOBAL_LEVEL)
    if self.ENABLE_CONSOLE_LOGGING:
      self.SetControlHandler(sys.stdout)
