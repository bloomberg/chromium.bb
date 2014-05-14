# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing utils to get and check lab status."""

import json
import logging
import re
import time
import urllib

from chromite.cbuildbot import constants
from chromite.cbuildbot import cbuildbot_failures as failures_lib


logger = logging.getLogger('chromite')


class LabIsDownException(failures_lib.TestLabFailure):
  """Raised when the Lab is Down."""


class BoardIsDisabledException(failures_lib.TestLabFailure):
  """Raised when a certain board is disabled in the lab."""


def GetLabStatus(max_attempts=5):
  """Grabs the current lab status and message.

  Adopted from autotest/files/client/common_lib/site_utils.py

  Args:
    max_attempts: max attempts to hit the lab status url.

  Returns:
    a dict with keys 'lab_is_up' and 'message'. lab_is_up points
    to a boolean and message points to a string.
  """
  status_url = constants.LAB_STATUS_URL
  result = {'lab_is_up': True, 'message': ''}
  retry_waittime = 1
  for _ in range(max_attempts):
    try:
      response = urllib.urlopen(status_url)
    except IOError as e:
      logger.log(logging.WARNING,
               'Error occurred when grabbing the lab status: %s.', e)
      time.sleep(retry_waittime)
      continue
    # Check for successful response code.
    code = response.getcode()
    if code == 200:
      data = json.load(response)
      result['lab_is_up'] = data['general_state'] in ('open', 'throttled')
      result['message'] = data['message']
      return result
    else:
      logger.log(logging.WARNING,
                 'Get HTTP code %d when grabbing the lab status from %s',
                 code, status_url)
      time.sleep(retry_waittime)
  # We go ahead and say the lab is open if we can't get the status.
  logger.log(logging.WARNING, 'Could not get a status from %s', status_url)
  return result


def CheckLabStatus(board=None):
  """Check if the lab is up and if we can schedule suites to run.

  Also checks if the lab is disabled for that particular board, and if so
  will raise an error to prevent new suites from being scheduled for that
  board. Adopted from autotest/files/client/common_lib/site_utils.py

  Args:
    board: board name that we want to check the status of.

  Raises:
    LabIsDownException if the lab is not up.
    BoardIsDisabledException if the desired board is currently disabled.
  """
  # First check if the lab is up.
  lab_status = GetLabStatus()
  if not lab_status['lab_is_up']:
    raise LabIsDownException('Chromium OS Lab is currently not up: '
                                   '%s.' % lab_status['message'])

  # Check if the board we wish to use is disabled.
  # Lab messages should be in the format of:
  # Lab is 'status' [boards not to be ran] (comment). Example:
  # Lab is Open [stumpy, kiev, x86-alex] (power_resume rtc causing duts to go
  # down)
  boards_are_disabled = re.search('\[(.*)\]', lab_status['message'])
  if board and boards_are_disabled:
    if board in boards_are_disabled.group(1):
      raise BoardIsDisabledException('Chromium OS Lab is '
          'currently not allowing suites to be scheduled on board '
          '%s: %s' % (board, lab_status['message']))
  return
