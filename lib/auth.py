# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Functions for authenticating httplib2 requests with OAuth2 tokens."""

from __future__ import print_function

import functools
import os
import sys

import chromite.lib.cros_logging as log
from chromite.lib import cipd
from chromite.lib import cros_build_lib
from chromite.lib import retry_util
from chromite.lib import path_util


REFRESH_STATUS_CODES = [401]


def _GetAuthUtil(instance_id='latest'):
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


def _Login():
  """Logs a user into chrome-infra-auth using authutil.

  Runs 'authutil login' to get a OAuth2 refresh token.

  Returns:
    Whether the login process was successful.
  """
  log.info('Logging into chrome-infra-auth...')
  result = retry_util.RunCommandWithRetries(
      3,
      args=[[_GetAuthUtil(), 'login']],
      kwargs={'error_code_ok': True})
  if not result.returncode:
    return True
  print(result.error, file=sys.stderr)
  log.error("Error logging in to chrome-infra-auth.")
  return False


def GetAccessToken(service_account_json=None):
  """Returns an OAuth2 access token using authutil.

  Attempts to get a token using 'authutil token'. If that fails, retries after
  running 'authutil login'.

  Args:
    service_account_json: A optional path to a service account to use

  Returns:
    The access token string.
  """
  flags = []
  if service_account_json:
    flags += ['-service_account_json', service_account_json]
  result = cros_build_lib.RunCommand(
      [_GetAuthUtil(), 'token'] + flags,
      capture_output=True,
      error_code_ok=True)

  if result.returncode:
    if not _Login():
      return
    return GetAccessToken()
  else:
    return result.output.strip()


def Authorize(get_access_token, http):
  """Monkey patches authentication logic of httplib2.Http instance.

  The modified http.request method will add authentication headers to each
  request and will refresh access_tokens when a 401 is received on a
  request.

  Args:
    get_access_token: A function which returns an access token.
    http: An instance of httplib2.Http.

  Returns:
    A modified instance of http that was passed in.
  """
  # Adapted from oauth2client.OAuth2Credentials.authorize.
  # We can't use oauthclient2 because the import will fail on slaves due to
  # missing PyOpenSSL (crbug.com/498467).
  request_orig = http.request

  @functools.wraps(request_orig)
  def new_request(*args, **kwargs):
    headers = kwargs.get('headers', {}).copy()
    headers['Authorization'] = 'Bearer %s' % get_access_token()
    kwargs['headers'] = headers

    resp, content = request_orig(*args, **kwargs)
    if resp.status in REFRESH_STATUS_CODES:
      log.info('Refreshing due to a %s', resp.status)
      # TODO(phobbs): delete the "access_token" key from the token file used.
      headers['Authorization'] = 'Bearer %s' % get_access_token().token
      resp, content = request_orig(*args, **kwargs)

    return resp, content

  http.request = new_request
  return http
