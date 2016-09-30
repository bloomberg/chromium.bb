# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module that contains unittests for auth module."""

from __future__ import print_function

import mock
import time

from chromite.lib import auth
from chromite.lib import cros_test_lib
from chromite.lib import cros_build_lib

class AuthTest(cros_test_lib.MockTestCase):
  """Test cases for methods in auth."""

  def setUp(self):
    self.PatchObject(time, 'sleep')
    self.PatchObject(auth, 'GetAuthUtil', return_value='auth_util')

  def testLogin(self):
    """Test Login."""
    failure_result = cros_build_lib.CommandResult(
        error='error', output=None, returncode=1)
    self.PatchObject(
        cros_build_lib, 'RunCommand', return_value=failure_result)
    self.assertFalse(auth.Login())

    success_result = cros_build_lib.CommandResult(
        output=None, returncode=0)
    self.PatchObject(
        cros_build_lib, 'RunCommand', return_value=success_result)
    self.assertTrue(auth.Login())

  def testToken(self):
    """Test Token."""
    failure_result = cros_build_lib.CommandResult(
        error='error', output=None, returncode=1)
    self.PatchObject(
        cros_build_lib, 'RunCommand', return_value=failure_result)
    self.assertIsNone(auth.Token())

    success_result = cros_build_lib.CommandResult(
        output='token', returncode=0)
    self.PatchObject(
        cros_build_lib, 'RunCommand', return_value=success_result)
    self.assertEqual(auth.Token(), 'token')

  def testGetAccessToken(self):
    """Test GetAccessToken."""
    mock_login = self.PatchObject(auth, 'Login', return_value=True)
    mock_token = self.PatchObject(auth, 'Token', return_value=None)

    auth.GetAccessToken()
    self.assertEqual(mock_login.call_count, auth.RETRY_GET_ACCESS_TOKEN+1)
    self.assertEqual(mock_token.call_count, auth.RETRY_GET_ACCESS_TOKEN+1)

class AuthorizedHttp(cros_test_lib.MockTestCase):
  """Test cases for AuthorizedHttp."""

  def setUp(self):
    self.mock_http = mock.Mock()
    self.mock_resp = mock.Mock()
    self.mock_token = self.PatchObject(auth, 'Token', return_value='token')
    self.mock_login = self.PatchObject(auth, 'Login', return_value=True)
    self.mock_get_token = self.PatchObject(
        auth, 'GetAccessToken', return_value='token')

  def testAuthorize(self):
    self.mock_resp.status = 200
    self.mock_http.request.return_value = self.mock_resp, 'content'

    auth_http = auth.AuthorizedHttp(
        auth.GetAccessToken, http=self.mock_http)
    auth_http.request('url', 'GET', body={}, headers={})
    auth_http.request('url', 'PUT', body={}, headers={})
    auth_http.request('url', 'POST', body={}, headers={})

    self.assertEqual(1, self.mock_get_token.call_count)
    self.assertEqual(3, self.mock_http.request.call_count)

  def testAuthorize2(self):
    self.mock_resp.status = 401
    self.mock_http.request.return_value = self.mock_resp, 'content'

    auth_http = auth.AuthorizedHttp(
        auth.GetAccessToken, http=self.mock_http)
    auth_http.request('url', 'GET', body={}, headers={})
    auth_http.request('url', 'PUT', body={}, headers={})
    auth_http.request('url', 'POST', body={}, headers={})

    self.assertEqual(4, self.mock_get_token.call_count)
    self.assertEqual(3, self.mock_login.call_count)
    self.assertEqual(6, self.mock_http.request.call_count)

  def testAuthorize3(self):
    self.mock_resp.status = 500
    self.mock_http.request.return_value = self.mock_resp, 'content'

    auth_http = auth.AuthorizedHttp(
        auth.GetAccessToken, http=self.mock_http)
    auth_http.request('url', 'GET', body={}, headers={})
    auth_http.request('url', 'PUT', body={}, headers={})
    auth_http.request('url', 'POST', body={}, headers={})

    self.assertEqual(1, self.mock_get_token.call_count)
    self.assertEqual(0, self.mock_login.call_count)
