# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Manage tree status."""

import json
import logging
import time
import urllib

from chromite.cbuildbot import constants
from chromite.lib import timeout_util


STATUS_URL = 'https://chromiumos-status.appspot.com/current?format=json'


def _GetStatus(status_url):
  """Polls |status_url| and returns the retrieved tree status.

  This function gets a JSON response from |status_url|, and returns the
  value associated with the 'general_state' key, if one exists and the
  http request was successful.

  Returns:
    The tree status, as a string, if it was successfully retrieved. Otherwise
    None.
  """
  try:
    # Check for successful response code.
    response = urllib.urlopen(status_url)
    if response.getcode() == 200:
      data = json.load(response)
      if data.has_key('general_state'):
        return data['general_state']
  # We remain robust against IOError's.
  except IOError as e:
    logging.error('Could not reach %s: %r', status_url, e)


def WaitForTreeStatus(status_url=None, period=1, timeout=1, throttled_ok=False):
  """Wait for tree status to be open (or throttled, if |throttled_ok|).

  Args:
    status_url: The status url to check i.e.
      'https://status.appspot.com/current?format=json'
    period: How often to poll for status updates.
    timeout: How long to wait until a tree status is discovered.
    throttled_ok: is TREE_THROTTLED an acceptable status?

  Returns:
    The most recent tree status, either constants.TREE_OPEN or
    constants.TREE_THROTTLED (if |throttled_ok|)

  Raises:
    timeout_util.TimeoutError if timeout expired before tree reached
    acceptable status.
  """
  if not status_url:
    status_url = STATUS_URL

  acceptable_states = set([constants.TREE_OPEN])
  verb = 'open'
  if throttled_ok:
    acceptable_states.add(constants.TREE_THROTTLED)
    verb = 'not be closed'

  timeout = max(timeout, 1)

  end_time = time.time() + timeout

  def _LogMessage():
    time_left = end_time - time.time()
    logging.info('Waiting for the tree to %s (%d minutes left)...', verb,
                 time_left / 60)

  def _get_status():
    return _GetStatus(status_url)

  return timeout_util.WaitForReturnValue(
      acceptable_states, _get_status, timeout=timeout,
      period=period, side_effect_func=_LogMessage)


def IsTreeOpen(status_url=None, period=1, timeout=1, throttled_ok=False):
  """Wait for tree status to be open (or throttled, if |throttled_ok|).

  Args:
    status_url: The status url to check i.e.
      'https://status.appspot.com/current?format=json'
    period: How often to poll for status updates.
    timeout: How long to wait until a tree status is discovered.
    throttled_ok: Does TREE_THROTTLED count as open?

  Returns:
    True if the tree is open (or throttled, if |throttled_ok|). False if
    timeout expired before tree reached acceptable status.
  """
  if not status_url:
    status_url = STATUS_URL

  try:
    WaitForTreeStatus(status_url=status_url, period=period, timeout=timeout,
                      throttled_ok=throttled_ok)
  except timeout_util.TimeoutError:
    return False
  return True


def GetTreeStatus(status_url, polling_period=0, timeout=0):
  """Returns the current tree status as fetched from |status_url|.

  This function returns the tree status as a string, either
  constants.TREE_OPEN, constants.TREE_THROTTLED, or constants.TREE_CLOSED.

  Args:
    status_url: The status url to check i.e.
      'https://status.appspot.com/current?format=json'
    polling_period: Time to wait in seconds between polling attempts.
    timeout: Maximum time in seconds to wait for status.

  Returns:
    constants.TREE_OPEN, constants.TREE_THROTTLED, or constants.TREE_CLOSED

  Raises:
    time_out.TimeoutError if the timeout expired before the status could be
    successfully fetched.
  """
  acceptable_states = set([constants.TREE_OPEN, constants.TREE_THROTTLED,
                           constants.TREE_CLOSED])
  def _get_status():
    return _GetStatus(status_url)

  return timeout_util.WaitForReturnValue(acceptable_states, _get_status,
                                         timeout, polling_period)
