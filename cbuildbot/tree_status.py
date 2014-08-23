# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Manage tree status."""

import httplib
import json
import logging
import os
import re
import socket
import urllib
import urllib2

from chromite.cbuildbot import constants
from chromite.lib import osutils
from chromite.lib import timeout_util


CROS_TREE_STATUS_URL = 'https://chromiumos-status.appspot.com'
CROS_TREE_STATUS_JSON_URL = '%s/current?format=json' % CROS_TREE_STATUS_URL
CROS_TREE_STATUS_UPDATE_URL = '%s/status' % CROS_TREE_STATUS_URL

_USER_NAME = 'buildbot@chromium.org'
_PASSWORD_PATH = '/home/chrome-bot/.status_password'


class PasswordFileDoesNotExist(Exception):
  """Raised when password file does not exist."""


class InvalidTreeStatus(Exception):
  """Raised when user wants to set an invalid tree status."""


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
    status_url = CROS_TREE_STATUS_JSON_URL

  acceptable_states = set([constants.TREE_OPEN])
  verb = 'open'
  if throttled_ok:
    acceptable_states.add(constants.TREE_THROTTLED)
    verb = 'not be closed'

  timeout = max(timeout, 1)

  def _LogMessage(minutes_left):
    logging.info('Waiting for the tree to %s (%d minutes left)...', verb,
                 minutes_left)

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
    status_url = CROS_TREE_STATUS_JSON_URL

  try:
    WaitForTreeStatus(status_url=status_url, period=period, timeout=timeout,
                      throttled_ok=throttled_ok)
  except timeout_util.TimeoutError:
    return False
  return True


def _GetPassword():
  """Returns the password for updating tree status."""
  if not os.path.exists(_PASSWORD_PATH):
    raise PasswordFileDoesNotExist(
        'Unable to retrieve password. %s does not exist',
        _PASSWORD_PATH)

  return osutils.ReadFile(_PASSWORD_PATH).strip()


def _UpdateTreeStatus(status_url, message):
  """Updates the tree status to |message|.

  Args:
    status_url: The tree status URL.
    message: The tree status text to post .
  """
  password = _GetPassword()
  params = urllib.urlencode({
      'message': message,
      'username': _USER_NAME,
      'password': password,
  })
  headers = {'Content-Type': 'application/x-www-form-urlencoded'}
  req = urllib2.Request(status_url, data=params, headers=headers)
  try:
    urllib2.urlopen(req)
  except (urllib2.URLError, httplib.HTTPException, socket.error) as e:
    logging.error('Unable to update tree status: %s', e)
    raise e
  else:
    logging.info('Updated tree status to %s', message)


def UpdateTreeStatus(status, message, status_url=None):
  """Updates the tree status to |status| with additional |message|.

  Args:
    status: A status in constants.VALID_TREE_STATUSES.
    message: A string to display as part of the tree status.
    status_url: The tree status URL.
  """
  if status_url is None:
    status_url = CROS_TREE_STATUS_UPDATE_URL

  if status not in constants.VALID_TREE_STATUSES:
    raise InvalidTreeStatus('%s is not a valid tree status.' % status)

  status_text = 'Tree is %(status)s (cbuildbot: %(message)s)' % {
      'status': status,
      'message': message}

  _UpdateTreeStatus(status_url, status_text)


def _OpenSheriffURL(sheriff_url):
  """Returns the content of |sheriff_url| or None if failed to open it."""
  try:
    response = urllib.urlopen(sheriff_url)
    if response.getcode() == 200:
      return response.read()
  except IOError as e:
    logging.error('Could not reach %s: %r', sheriff_url, e)


def GetSheriffEmailAddresses(sheriff_type):
  """Get the email addresses of the sheriffs or deputy.

  Args:
    sheriff_type: Type of the sheriff to look for. See the keys in
    constants.SHERIFF_TYPE_TO_URL.
      - 'tree': tree sheriffs
      - 'build': build deputy
      - 'lab' : lab sheriff
      - 'chrome': chrome gardener

  Returns:
    A list of email addresses.
  """
  if sheriff_type not in constants.SHERIFF_TYPE_TO_URL:
    raise ValueError('Unknown sheriff type: %s' % sheriff_type)

  urls = constants.SHERIFF_TYPE_TO_URL.get(sheriff_type)
  sheriffs = []
  for url in urls:
    # The URL displays a line: document.write('taco, burrito')
    raw_line = _OpenSheriffURL(url)
    if raw_line is not None:
      match = re.search(r'\'(.*)\'', raw_line)
      if match and match.group(1) != 'None (channel is sheriff)':
        sheriffs.extend(x.strip() for x in match.group(1).split(','))

  return ['%s%s' % (x, constants.GOOGLE_EMAIL) for x in sheriffs]


def GetHealthAlertRecipients(builder_run):
  """Returns a list of email addresses of the health alert recipients."""
  recipients = []
  for entry in builder_run.config.health_alert_recipients:
    if '@' in entry:
      # If the entry is an email address, add it to the list.
      recipients.append(entry)
    else:
      # Perform address lookup for a non-email entry.
      recipients.extend(GetSheriffEmailAddresses(entry))

  return recipients
