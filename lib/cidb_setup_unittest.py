#!/usr/bin/python
# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for cidb.py Setup methods.

These unit tests require that cros_test_lib is NOT impoted (importing
cros_test_lib
are in a dedicated module, separate from any other
unit tests which may be added in future to cidb_unittest. That is because
these tests require us NOT to import cros_test_lib
"""


import mox
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))

from chromite.cbuildbot import constants
from chromite.lib import cidb

class CIDBConnectionFactoryTest(unittest.TestCase):
  """Test that CIDBConnectionFactory behaves as expected."""

  def setUp(self):
    self.mox = mox.Mox()
    # Ensure that we do not create any live connections in this unit test.
    self.mox.StubOutWithMock(cidb, 'CIDBConnection')
    # pylint: disable-msg=W0212
    cidb.CIDBConnectionFactory._ClearCIDBSetup()

  def tearDown(self):
    self.mox.UnsetStubs()
    self.mox.VerifyAll()

  def testGetConnectionBeforeSetup(self):
    """Calling GetConnection before Setup should raise exception."""
    self.assertRaises(AssertionError,
                      cidb.CIDBConnectionFactory.GetCIDBConnectionForBuilder)

  def testSetupProd(self):
    """Test that SetupProd behaves as expected."""
    # Expected constructor call
    cidb.CIDBConnection(constants.CIDB_PROD_BOT_CREDS)
    self.mox.ReplayAll()

    cidb.CIDBConnectionFactory.SetupProdCidb()
    cidb.CIDBConnectionFactory.GetCIDBConnectionForBuilder()
    self.assertTrue(cidb.CIDBConnectionFactory.IsCIDBSetup())
    self.assertRaises(AssertionError, cidb.CIDBConnectionFactory.SetupProdCidb)
    self.assertRaises(AssertionError, cidb.CIDBConnectionFactory.SetupDebugCidb)
    self.assertRaises(AssertionError, cidb.CIDBConnectionFactory.SetupMockCidb)
    self.assertRaises(AssertionError, cidb.CIDBConnectionFactory.SetupNoCidb)

  def testSetupDebug(self):
    """Test that SetupDebug behaves as expected."""
    # Expected constructor call
    cidb.CIDBConnection(constants.CIDB_DEBUG_BOT_CREDS)
    self.mox.ReplayAll()

    cidb.CIDBConnectionFactory.SetupDebugCidb()
    cidb.CIDBConnectionFactory.GetCIDBConnectionForBuilder()
    self.assertTrue(cidb.CIDBConnectionFactory.IsCIDBSetup())
    self.assertRaises(AssertionError, cidb.CIDBConnectionFactory.SetupProdCidb)
    self.assertRaises(AssertionError, cidb.CIDBConnectionFactory.SetupDebugCidb)
    self.assertRaises(AssertionError, cidb.CIDBConnectionFactory.SetupMockCidb)
    self.assertRaises(AssertionError, cidb.CIDBConnectionFactory.SetupNoCidb)

  def testInvalidateSetup(self):
    """Test that cidb connection can be invalidated."""
    cidb.CIDBConnectionFactory.SetupProdCidb()
    cidb.CIDBConnectionFactory.InvalidateCIDBSetup()
    self.assertRaises(AssertionError,
                      cidb.CIDBConnectionFactory.GetCIDBConnectionForBuilder)

  def testSetupMock(self):
    """Test that SetupDebug behaves as expected."""
    # Set the CIDB to mock mode, but without supplying a mock
    cidb.CIDBConnectionFactory.SetupMockCidb()
    self.assertFalse(cidb.CIDBConnectionFactory.IsCIDBSetup())
    self.assertRaises(AssertionError,
                      cidb.CIDBConnectionFactory.GetCIDBConnectionForBuilder)

    # Calls to non-mock Setup methods should fail.
    self.assertRaises(AssertionError, cidb.CIDBConnectionFactory.SetupProdCidb)
    self.assertRaises(AssertionError, cidb.CIDBConnectionFactory.SetupDebugCidb)

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

    # Calls to non-mock Setup methods should still fail.
    self.assertRaises(AssertionError, cidb.CIDBConnectionFactory.SetupProdCidb)
    self.assertRaises(AssertionError, cidb.CIDBConnectionFactory.SetupDebugCidb)

  def testSetupNo(self):
    """Test that SetupNoCidb behaves as expected."""
    cidb.CIDBConnectionFactory.SetupMockCidb()
    cidb.CIDBConnectionFactory.SetupNoCidb()
    cidb.CIDBConnectionFactory.SetupNoCidb()
    self.assertTrue(cidb.CIDBConnectionFactory.IsCIDBSetup())
    self.assertEqual(cidb.CIDBConnectionFactory.GetCIDBConnectionForBuilder(),
                     None)
    self.assertRaises(AssertionError, cidb.CIDBConnectionFactory.SetupProdCidb)
    self.assertRaises(AssertionError, cidb.CIDBConnectionFactory.SetupDebugCidb)
    self.assertRaises(AssertionError, cidb.CIDBConnectionFactory.SetupMockCidb)


if __name__ == '__main__':
  unittest.main()
