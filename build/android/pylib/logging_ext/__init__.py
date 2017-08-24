# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrapper around logging to set some custom logging functionality."""

import contextlib
import logging
import os
import time
import sys

from pylib.constants import host_paths
from pylib.utils import unicode_characters

_COLORAMA_PATH = os.path.join(
    host_paths.DIR_SOURCE_ROOT, 'third_party', 'colorama', 'src')

with host_paths.SysPath(_COLORAMA_PATH, position=0):
  import colorama

TEST_FAIL = logging.INFO + 3
TEST_PASS = logging.INFO + 2
TEST_TIME = logging.INFO + 1


def _set_log_level(verbose_count):
  if verbose_count == -1: # Quiet
    log_level = logging.WARNING
  elif verbose_count == 0: # Default
    log_level = logging.INFO + 1
  elif verbose_count == 1:
    log_level = logging.INFO
  elif verbose_count >= 2:
    log_level = logging.DEBUG
  logging.getLogger().setLevel(log_level)


class CustomFormatter(logging.Formatter):
  """Custom log formatter."""
  # pylint does not see members added dynamically in the constructor.
  # pylint: disable=no-member
  COLOR_MAP = {
    logging.DEBUG: colorama.Fore.CYAN + colorama.Style.DIM,
    logging.WARNING: colorama.Fore.YELLOW,
    logging.ERROR: colorama.Back.RED,
    logging.CRITICAL: colorama.Back.RED,
    TEST_PASS: colorama.Fore.GREEN + colorama.Style.BRIGHT,
    TEST_FAIL: colorama.Fore.RED + colorama.Style.BRIGHT,
    TEST_TIME: colorama.Fore.MAGENTA + colorama.Style.BRIGHT,
  }

  # override
  def __init__(self, color, fmt='%(threadName)-4s  %(message)s'):
    logging.Formatter.__init__(self, fmt=fmt)
    self._creation_time = time.time()
    self._should_colorize = color

  # override
  def format(self, record):
    msg = logging.Formatter.format(self, record)
    if 'MainThread' in msg[:19]:
      msg = msg.replace('MainThread', 'Main', 1)
    timediff = time.time() - self._creation_time

    level_tag = record.levelname[0]
    if self._should_colorize:
      level_tag = '%s%-1s%s' % (
          self.COLOR_MAP.get(record.levelno, ''),
          level_tag, colorama.Style.RESET_ALL)

    return '%s %8.3fs %s' % (level_tag, timediff, msg)


logging.addLevelName(TEST_TIME, unicode_characters.CLOCK)
def test_time(message, *args, **kwargs):
  logger = logging.getLogger()
  if logger.isEnabledFor(TEST_TIME):
    # pylint: disable=protected-access
    logger._log(TEST_TIME, message, args, **kwargs)


logging.addLevelName(TEST_FAIL, unicode_characters.CROSS)
def test_fail(message, *args, **kwargs):
  logger = logging.getLogger()
  if logger.isEnabledFor(TEST_FAIL):
    # pylint: disable=protected-access
    logger._log(TEST_FAIL, message, args, **kwargs)


logging.addLevelName(TEST_PASS, unicode_characters.CHECK)
def test_pass(message, *args, **kwargs):
  logger = logging.getLogger()
  if logger.isEnabledFor(TEST_PASS):
    # pylint: disable=protected-access
    logger._log(TEST_PASS, message, args, **kwargs)


def Initialize(log_level):
  logging.getLogger().handlers = []
  handler = logging.StreamHandler(stream=sys.stderr)
  logging.getLogger().addHandler(handler)
  handler.setFormatter(CustomFormatter(color=sys.stderr.isatty()))
  _set_log_level(log_level)

