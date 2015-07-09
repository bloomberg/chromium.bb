# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for cidb."""

from __future__ import print_function

import exceptions
import sqlalchemy

from chromite.lib import cidb
from chromite.lib import cros_test_lib


class RetryableOperationalError(exceptions.EnvironmentError):
  """An operational error with retryable error code."""

  def __init__(self):
    super(RetryableOperationalError, self).__init__(1053, 'retryable')


class FatalOperationalError(exceptions.EnvironmentError):
  """An operational error with fatal error code."""

  def __init__(self):
    super(FatalOperationalError, self).__init__(9999, 'fatal')


class UnknownError(Exception):
  """An error that's not an OperationalError."""


class HelperFunctionsTest(cros_test_lib.TestCase):
  """Test (private) helper functions in the module."""

  def _WrapError(self, error):
    return sqlalchemy.exc.OperationalError(
        statement=None, params=None, orig=error)

  # pylint: disable=protected-access
  def testIsRetryableExceptionMatch(self):
    self.assertTrue(cidb._IsRetryableException(RetryableOperationalError()))
    self.assertFalse(cidb._IsRetryableException(FatalOperationalError()))
    self.assertFalse(cidb._IsRetryableException(UnknownError()))

    self.assertTrue(cidb._IsRetryableException(self._WrapError(
        RetryableOperationalError())))
    self.assertFalse(cidb._IsRetryableException(self._WrapError(
        FatalOperationalError())))
    self.assertFalse(cidb._IsRetryableException(self._WrapError(
        UnknownError())))
