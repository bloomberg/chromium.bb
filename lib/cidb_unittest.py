# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for cidb."""

from __future__ import print_function

import exceptions
import mock
import sqlalchemy

from chromite.cbuildbot import constants
from chromite.lib import cidb
from chromite.lib import cros_test_lib
from chromite.lib import factory


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

class CIDBConnectionTest(cros_test_lib.MockTestCase):
  """Tests CIDBConnection."""


  def testInsertCLActions(self):
    connection_mock = mock.Mock(spec=cidb.CIDBConnection,
                                schema_version=10**100)

    cl_actions = [
        mock.Mock(reason='reason: #%d' % i,
                  action='action_%d' % i)
        for i in range(10)
    ]

    metrics_mock = self.PatchObject(cidb, 'metrics')
    cidb.CIDBConnection.InsertCLActions(connection_mock, 0, cl_actions)

    for i in range(10):
      metrics_mock.CounterMetric.assert_any_call(
          'chromeos/cbuildbot/cl_action/action_%d' % i,
          fields={'reason': 'reason: #%d' % i})


class CIDBConnectionFactoryTest(cros_test_lib.MockTestCase):
  """Test that CIDBConnectionFactory behaves as expected."""

  def setUp(self):
    # Ensure that we do not create any live connections in this unit test.
    self.connection_mock = self.PatchObject(cidb, 'CIDBConnection')
    # pylint: disable=W0212
    cidb.CIDBConnectionFactory._ClearCIDBSetup()

  def tearDown(self):
    # pylint: disable=protected-access
    cidb.CIDBConnectionFactory._ClearCIDBSetup()

  def testGetConnectionBeforeSetup(self):
    """Calling GetConnection before Setup should raise exception."""
    self.assertRaises(factory.ObjectFactoryIllegalOperation,
                      cidb.CIDBConnectionFactory.GetCIDBConnectionForBuilder)

  def testSetupProd(self):
    """Test that SetupProd behaves as expected."""
    cidb.CIDBConnectionFactory.SetupProdCidb()
    cidb.CIDBConnectionFactory.GetCIDBConnectionForBuilder()

    # Expected constructor call
    self.connection_mock.assert_called_once_with(constants.CIDB_PROD_BOT_CREDS)
    self.assertTrue(cidb.CIDBConnectionFactory.IsCIDBSetup())
    self.assertRaises(factory.ObjectFactoryIllegalOperation,
                      cidb.CIDBConnectionFactory.SetupProdCidb)
    self.assertRaises(factory.ObjectFactoryIllegalOperation,
                      cidb.CIDBConnectionFactory.SetupDebugCidb)
    self.assertRaises(factory.ObjectFactoryIllegalOperation,
                      cidb.CIDBConnectionFactory.SetupMockCidb)
    self.assertRaises(factory.ObjectFactoryIllegalOperation,
                      cidb.CIDBConnectionFactory.SetupNoCidb)

  def testSetupDebug(self):
    """Test that SetupDebug behaves as expected."""
    cidb.CIDBConnectionFactory.SetupDebugCidb()
    cidb.CIDBConnectionFactory.GetCIDBConnectionForBuilder()

    # Expected constructor call
    self.connection_mock.assert_called_once_with(constants.CIDB_DEBUG_BOT_CREDS)
    self.assertTrue(cidb.CIDBConnectionFactory.IsCIDBSetup())
    self.assertRaises(factory.ObjectFactoryIllegalOperation,
                      cidb.CIDBConnectionFactory.SetupProdCidb)
    self.assertRaises(factory.ObjectFactoryIllegalOperation,
                      cidb.CIDBConnectionFactory.SetupDebugCidb)
    self.assertRaises(factory.ObjectFactoryIllegalOperation,
                      cidb.CIDBConnectionFactory.SetupMockCidb)
    self.assertRaises(factory.ObjectFactoryIllegalOperation,
                      cidb.CIDBConnectionFactory.SetupNoCidb)

  def testInvalidateSetup(self):
    """Test that cidb connection can be invalidated."""
    cidb.CIDBConnectionFactory.SetupProdCidb()
    cidb.CIDBConnectionFactory.InvalidateCIDBSetup()
    self.assertRaises(AssertionError,
                      cidb.CIDBConnectionFactory.GetCIDBConnectionForBuilder)

  def testSetupMock(self):
    """Test that SetupMock behaves as expected."""
    # Set the CIDB to mock mode, but without supplying a mock
    cidb.CIDBConnectionFactory.SetupMockCidb()

    # Calls to non-mock Setup methods should fail.
    self.assertRaises(factory.ObjectFactoryIllegalOperation,
                      cidb.CIDBConnectionFactory.SetupProdCidb)
    self.assertRaises(factory.ObjectFactoryIllegalOperation,
                      cidb.CIDBConnectionFactory.SetupDebugCidb)

    # Now supply a mock.
    a = object()
    cidb.CIDBConnectionFactory.SetupMockCidb(a)
    self.assertTrue(cidb.CIDBConnectionFactory.IsCIDBSetup())
    self.assertEqual(cidb.CIDBConnectionFactory.GetCIDBConnectionForBuilder(),
                     a)

    # Mock object can be changed by future SetupMockCidb call.
    b = object()
    cidb.CIDBConnectionFactory.SetupMockCidb(b)
    self.assertEqual(cidb.CIDBConnectionFactory.GetCIDBConnectionForBuilder(),
                     b)

    # Mock object can be cleared by future ClearMock call.
    cidb.CIDBConnectionFactory.ClearMock()

    # Calls to non-mock Setup methods should still fail.
    self.assertRaises(factory.ObjectFactoryIllegalOperation,
                      cidb.CIDBConnectionFactory.SetupProdCidb)
    self.assertRaises(factory.ObjectFactoryIllegalOperation,
                      cidb.CIDBConnectionFactory.SetupDebugCidb)

  def testSetupNo(self):
    """Test that SetupNoCidb behaves as expected."""
    cidb.CIDBConnectionFactory.SetupMockCidb()
    cidb.CIDBConnectionFactory.SetupNoCidb()
    cidb.CIDBConnectionFactory.SetupNoCidb()
    self.assertTrue(cidb.CIDBConnectionFactory.IsCIDBSetup())
    self.assertEqual(cidb.CIDBConnectionFactory.GetCIDBConnectionForBuilder(),
                     None)
    self.assertRaises(factory.ObjectFactoryIllegalOperation,
                      cidb.CIDBConnectionFactory.SetupProdCidb)
    self.assertRaises(factory.ObjectFactoryIllegalOperation,
                      cidb.CIDBConnectionFactory.SetupDebugCidb)
    self.assertRaises(factory.ObjectFactoryIllegalOperation,
                      cidb.CIDBConnectionFactory.SetupMockCidb)
