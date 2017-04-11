# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Functions for authenticating httplib2 requests with OAuth2 tokens."""

from __future__ import print_function

import os

from chromite.lib import cipd
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import retry_util
from chromite.lib import path_util

# from third_party
import httplib2

REFRESH_STATUS_CODES = [401]

# Retry times on get_access_token
RETRY_GET_ACCESS_TOKEN = 3

class AccessTokenError(Exception):
  """Error accessing the token."""

def GetAuthUtil(instance_id='latest'):
  """Returns a path to the authutil binary.

  This will download and install the authutil package if it is not already
  deployed.

  Args:
    instance_id: The instance-id of the package to install. Defaults to 'latest'

  Returns:
    the path to the authutil binary.
  """
  cache_dir = os.path.join(path_util.GetCacheDir(), 'cipd/packages')
  path = cipd.InstallPackage(
      cipd.GetCIPDFromCache(),
      'infra/tools/authutil/linux-amd64',
      instance_id,
      destination=cache_dir)

  return os.path.join(path, 'authutil')

def Login(service_account_json=None):
  """Logs a user into chrome-infra-auth using authutil.

  Runs 'authutil login' to get a OAuth2 refresh token.

  Args:
    service_account_json: A optional path to a service account.

  Returns:
    Whether the login process was successful.
  """
  logging.info('Logging into chrome-infra-auth with service_account %s',
               service_account_json)

  cmd = [GetAuthUtil(), 'login']
  if service_account_json:
    cmd += ['-service-account-json=%s' % service_account_json]

  result = cros_build_lib.RunCommand(
      cmd,
      mute_output=False,
      error_code_ok=True)

  if result.returncode:
    logging.error('Error logging in to chrome-infra-auth: %s' %
                  result.error)

  return result.returncode == 0

def Token(service_account_json=None):
  """Get the token using authutil.

  Runs 'authutil token' to get the OAuth2 token.

  Args:
    service_account_json: A optional path to a service account.

  Returns:
    The token string if the command succeeded; else, None.
  """
  cmd = [GetAuthUtil(), 'token']
  if service_account_json:
    cmd += ['-service-account-json=%s' % service_account_json]

  result = cros_build_lib.RunCommand(
      cmd,
      capture_output=True,
      error_code_ok=True)

  if result.returncode:
    logging.warning('Error getting tokens with service_account %s: %s',
                    service_account_json, result.error)
    return
  else:
    return result.output.strip()

def _TokenAndLoginIfNeed(service_account_json=None):
  """Run Token and Login opertions.

  Run Token operation first. If no token found, run Login operation
  to refresh the token. Throw an AccessTokenError after running the
  Login operation, so that GetAccessToken can retry on
  _TokenAndLoginIfNeed.

  Args:
    service_account_json: A optional path to a service account.

  Returns:
    The token string if the command succeeded; else, None.

  Raises:
    AccessTokenError if the Token operation failed.
  """
  token = Token(service_account_json=service_account_json)

  if token is None:
    Login(service_account_json=service_account_json)
    raise AccessTokenError('Failed at getting the access token, may retry.')
  else:
    return token

def GetAccessToken(service_account_json=None):
  """Returns an OAuth2 access token using authutil.

  Retry the _TokenAndLoginIfNeed function when the error threw is an
  AccessTokenError.

  Args:
    service_account_json: A optional path to a service account.

  Returns:
    The access token string.
  """
  retry = lambda e: isinstance(e, AccessTokenError)

  try:
    result = retry_util.GenericRetry(
        retry, RETRY_GET_ACCESS_TOKEN,
        _TokenAndLoginIfNeed,
        service_account_json, sleep=3)
    return result
  except AccessTokenError as e:
    logging.error('Failed at getting the access token: %s ', e)
    # Do not raise the AccessTokenError here.
    # Let the response returned by the request handler
    # tell the status and errors.
    return

class AuthorizedHttp(object):
  """Authorized http instance"""

  def __init__(self, get_access_token, http=None, service_account_json=None):
    self.get_access_token = get_access_token
    self.http = http if http is not None else httplib2.Http()
    self.service_account_json = service_account_json
    self.token = get_access_token(service_account_json=service_account_json)

  # Adapted from oauth2client.OAuth2Credentials.authorize.
  # We can't use oauthclient2 because the import will fail on slaves due to
  # missing PyOpenSSL (crbug.com/498467).
  def request(self, *args, **kwargs):
    headers = kwargs.get('headers', {}).copy()
    headers['Authorization'] = 'Bearer %s' % self.token
    kwargs['headers'] = headers

    resp, content = self.http.request(*args, **kwargs)
    if resp.status in REFRESH_STATUS_CODES:
      logging.info('Refreshing due to a %s', resp.status)
      # Login and renew the token
      Login(service_account_json=self.service_account_json)
      self.token = self.get_access_token(
          service_account_json=self.service_account_json)
      # TODO(phobbs): delete the "access_token" key from the token file used.
      headers['Authorization'] = 'Bearer %s' % self.token
      resp, content = self.http.request(*args, **kwargs)

    return resp, content
