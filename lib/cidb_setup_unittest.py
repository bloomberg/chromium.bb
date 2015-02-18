# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for cidb.py Setup methods."""

from __future__ import print_function

from chromite.cbuildbot import constants
from chromite.lib import cidb
from chromite.lib import cros_test_lib
from chromite.lib import factory


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
