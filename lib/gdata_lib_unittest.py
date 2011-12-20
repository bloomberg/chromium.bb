#!/usr/bin/python2.6
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for the gdata_lib module."""

import getpass
import re
import unittest

import gdata.spreadsheet.service
import mox

import cros_test_lib as test_lib
import gdata_lib

# pylint: disable=W0201,E1101,R0904

class GdataLibTest(test_lib.TestCase):

  def setUp(self):
    pass

  def testPrepColNameForSS(self):
    tests = {
      'foo': 'foo',
      'Foo': 'foo',
      'FOO': 'foo',
      'foo bar': 'foobar',
      'Foo Bar': 'foobar',
      'F O O B A R': 'foobar',
      'Foo/Bar': 'foobar',
      'Foo Bar/Dude': 'foobardude',
      'foo/bar': 'foobar',
      }

    for col in tests:
      expected = tests[col]
      self.assertEquals(expected, gdata_lib.PrepColNameForSS(col))
      self.assertEquals(expected, gdata_lib.PrepColNameForSS(expected))

  def testPrepValForSS(self):
    tests = {
      None: None,
      '': '',
      'foo': 'foo',
      'foo123': 'foo123',
      '123': "'123",
      '1.2': "'1.2",
      }

    for val in tests:
      expected = tests[val]
      self.assertEquals(expected, gdata_lib.PrepValForSS(val))

  def testPrepRowForSS(self):
    vals = {
      None: None,
      '': '',
      'foo': 'foo',
      'foo123': 'foo123',
      '123': "'123",
      '1.2': "'1.2",
      }

    # Create before and after rows (rowIn, rowOut).
    rowIn = {}
    rowOut = {}
    col = 'a' # Column names not important.
    for valIn in vals:
      valOut = vals[valIn]
      rowIn[col] = valIn
      rowOut[col] = valOut

      col = chr(ord(col) + 1) # Change column name.

    self.assertEquals(rowOut, gdata_lib.PrepRowForSS(rowIn))

  def testScrubValFromSS(self):
    tests = {
      None: None,
      'foo': 'foo',
      '123': '123',
      "'123": '123',
      }

    for val in tests:
      expected = tests[val]
      self.assertEquals(expected, gdata_lib.ScrubValFromSS(val))

class CredsTest(test_lib.MoxTestCase):

  USER = 'somedude@chromium.org'
  PASSWORD = 'worldsbestpassword'
  TMPFILE_NAME = 'creds.txt'

  def setUp(self):
    mox.MoxTestBase.setUp(self)

  @test_lib.tempfile_decorator
  def testStoreLoadCreds(self):
    # Replay script
    mocked_creds = self.mox.CreateMock(gdata_lib.Creds)
    self.mox.ReplayAll()

    # Verify
    with self.OutputCapturer():
      gdata_lib.Creds.__init__(mocked_creds, user=self.USER,
                               password=self.PASSWORD)
      self.assertEquals(self.USER, mocked_creds.user)
      self.assertEquals(self.PASSWORD, mocked_creds.password)

      gdata_lib.Creds.StoreCreds(mocked_creds, self.tempfile)
      self.assertEquals(self.USER, mocked_creds.user)
      self.assertEquals(self.PASSWORD, mocked_creds.password)

      # Clear user/password before loading from just-created file.
      mocked_creds.user = None
      mocked_creds.password = None
      self.assertEquals(None, mocked_creds.user)
      self.assertEquals(None, mocked_creds.password)

      gdata_lib.Creds.LoadCreds(mocked_creds, self.tempfile)
      self.assertEquals(self.USER, mocked_creds.user)
      self.assertEquals(self.PASSWORD, mocked_creds.password)

    self.mox.VerifyAll()

  @test_lib.tempfile_decorator
  def testCredsInitFromFile(self):
    open(self.tempfile, 'w').close()

    # Replay script
    mocked_creds = self.mox.CreateMock(gdata_lib.Creds)
    mocked_creds.LoadCreds(self.tempfile)
    self.mox.ReplayAll()

    # Verify
    gdata_lib.Creds.__init__(mocked_creds, cred_file=self.tempfile)
    self.mox.VerifyAll()

  def testCredsInitFromUserPassword(self):
    # Replay script
    mocked_creds = self.mox.CreateMock(gdata_lib.Creds)
    self.mox.ReplayAll()

    # Verify
    gdata_lib.Creds.__init__(mocked_creds, user=self.USER,
                             password=self.PASSWORD)
    self.mox.VerifyAll()
    self.assertEquals(self.USER, mocked_creds.user)
    self.assertEquals(self.PASSWORD, mocked_creds.password)

  def testCredsInitFromUser(self):
    # Add test-specific mocks/stubs
    self.mox.StubOutWithMock(getpass, 'getpass')

    # Replay script
    mocked_creds = self.mox.CreateMock(gdata_lib.Creds)
    getpass.getpass(mox.IgnoreArg()).AndReturn(self.PASSWORD)
    self.mox.ReplayAll()

    # Verify
    gdata_lib.Creds.__init__(mocked_creds, user=self.USER)
    self.mox.VerifyAll()
    self.assertEquals(self.USER, mocked_creds.user)
    self.assertEquals(self.PASSWORD, mocked_creds.password)

class RetrySpreadsheetsServiceTest(test_lib.MoxTestCase):

  def setUp(self):
    mox.MoxTestBase.setUp(self)

  def _TestRequest(self, statuses, expected_status_index, success):
    mocked_ss = self.mox.CreateMock(gdata_lib.RetrySpreadsheetsService)
    self.mox.StubOutWithMock(gdata.spreadsheet.service.SpreadsheetsService,
                             'request')

    args = ('GET', 'http://foo.bar')

    # Replay script
    for ix, status in enumerate(statuses):
      # Add index of status to track which status the request function is
      # returning.  It is expected to return the last return status if
      # successful (retries or not), but first return status if failed.
      retval = test_lib.EasyAttr(status=status, index=ix)
      gdata.spreadsheet.service.SpreadsheetsService.request(mocked_ss, *args
                                                            ).AndReturn(retval)
    self.mox.ReplayAll()

    # Verity
    with self.OutputCapturer():
      retval = gdata_lib.RetrySpreadsheetsService.request(mocked_ss, *args)
      self.mox.VerifyAll()
      self.assertEquals(statuses[expected_status_index], retval.status)
      self.assertEquals(expected_status_index, retval.index)

    if not success:
      # Retries did not help, request still failed.
      regexp = re.compile(r'^Giving up on HTTP request')
      self.AssertOutputContainsWarning(regexp=regexp)
    elif expected_status_index > 0:
      # Warning expected if retries were needed.
      self.AssertOutputContainsWarning()
    else:
      # First try worked, expect no warnings.
      self.AssertOutputContainsWarning(invert=True)

  def testRequest(self):
    self._TestRequest([200], 0, True)

  def testRequest403(self):
    self._TestRequest([403, 200], 1, True)

  def testRequest403x2(self):
    self._TestRequest([403, 403, 200], 2, True)

  def testRequest403x3(self):
    self._TestRequest([403, 403, 403, 200], 3, True)

  def testRequest403x4(self):
    self._TestRequest([403, 403, 403, 403, 200], 4, True)

  def testRequest403x5(self):
    # This one should exhaust the retries.
    self._TestRequest([403, 403, 403, 403, 403], 0, False)

class MyError1(RuntimeError):
  pass

class MyError2(RuntimeError):
  pass

class MyError3(RuntimeError):
  pass

class Foo(object):
  Error1Msg = 'You told me to raise Error1'
  Error2Msg = 'You told me to raise Error2'
  Error3Msg = 'You told me to raise Error3'

  def __init__(self, first, last):
    self.first = first
    self.last = last
    self.error2_count = 0
    self.error2_max = 1

  def GetFullName(self):
    return '%s %s' % (self.first, self.last)

  def GetFirst(self):
    return self.first

  def GetLast(self):
    return self.last

  def RaiseError1(self, returnval=None, doit=True):
    if doit:
      raise MyError1(self.Error1Msg)

    return returnval

  def RaiseError2(self, returnval=None, doit=True):
    """Raise error only first |self.error2_max| times run for this object."""
    if doit:
      self.error2_count += 1
      if self.error2_count <= self.error2_max:
        raise MyError2(self.Error2Msg)

    return returnval

  def RaiseError3(self, returnval=None, doit=True):
    if doit:
      raise MyError3(self.Error1Msg)

    return returnval


if __name__ == '__main__':
  unittest.main()
