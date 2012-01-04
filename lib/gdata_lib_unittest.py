#!/usr/bin/python2.6
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for the gdata_lib module."""

import getpass
import re
import unittest

import atom.service
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
    # This is the replay script for the test.
    mocked_creds = self.mox.CreateMock(gdata_lib.Creds)
    self.mox.ReplayAll()

    # This is the test verification.
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

    # This is the replay script for the test.
    mocked_creds = self.mox.CreateMock(gdata_lib.Creds)
    mocked_creds.LoadCreds(self.tempfile)
    self.mox.ReplayAll()

    # This is the test verification.
    gdata_lib.Creds.__init__(mocked_creds, cred_file=self.tempfile)
    self.mox.VerifyAll()

  def testCredsInitFromUserPassword(self):
    # This is the replay script for the test.
    mocked_creds = self.mox.CreateMock(gdata_lib.Creds)
    self.mox.ReplayAll()

    # This is the test verification.
    gdata_lib.Creds.__init__(mocked_creds, user=self.USER,
                             password=self.PASSWORD)
    self.mox.VerifyAll()
    self.assertEquals(self.USER, mocked_creds.user)
    self.assertEquals(self.PASSWORD, mocked_creds.password)

  def testCredsInitFromUser(self):
    # Add test-specific mocks/stubs
    self.mox.StubOutWithMock(getpass, 'getpass')

    # This is the replay script for the test.
    mocked_creds = self.mox.CreateMock(gdata_lib.Creds)
    getpass.getpass(mox.IgnoreArg()).AndReturn(self.PASSWORD)
    self.mox.ReplayAll()

    # This is the test verification.
    gdata_lib.Creds.__init__(mocked_creds, user=self.USER)
    self.mox.VerifyAll()
    self.assertEquals(self.USER, mocked_creds.user)
    self.assertEquals(self.PASSWORD, mocked_creds.password)

class RetrySpreadsheetsServiceTest(test_lib.MoxTestCase):

  def setUp(self):
    mox.MoxTestBase.setUp(self)

  def testRequest(self):
    """Test that calling request method invokes _RetryRequest wrapper."""
    # pylint: disable=W0212

    self.mox.StubOutWithMock(gdata_lib.RetrySpreadsheetsService,
                             '_RetryRequest')

    # Use a real RetrySpreadsheetsService object rather than a mocked
    # one, because the .request method only exists if __init__ is run.
    # Also split up __new__ and __init__ in order to grab the original
    # rss.request method (inherited from base class at that point).
    rss = gdata_lib.RetrySpreadsheetsService.__new__(
        gdata_lib.RetrySpreadsheetsService)
    orig_request = rss.request
    rss.__init__()

    args = ('GET', 'http://foo.bar')

    # This is the replay script for the test.
    gdata_lib.RetrySpreadsheetsService._RetryRequest(orig_request, *args
                                                     ).AndReturn('wrapped')
    self.mox.ReplayAll()

    # This is the test verification.
    retval = rss.request(*args)
    self.mox.VerifyAll()
    self.assertEquals('wrapped', retval)

  def _TestHttpClientRetryRequest(self, statuses):
    """Test retry logic in http_client request during ProgrammaticLogin.

    |statuses| is list of http codes to simulate, where 200 means success.
    """
    expect_success = statuses[-1] == 200

    self.mox.StubOutWithMock(atom.http.ProxiedHttpClient, 'request')
    rss = gdata_lib.RetrySpreadsheetsService()

    args = ('POST', 'https://www.google.com/accounts/ClientLogin')
    def _read():
      return 'Some response text'

    # This is the replay script for the test.
    # Simulate the return codes in statuses.
    for status in statuses:
      retstatus = test_lib.EasyAttr(status=status, read=_read)
      atom.http.ProxiedHttpClient.request(*args,
                                          data=mox.IgnoreArg(),
                                          headers=mox.IgnoreArg()
                                          ).AndReturn(retstatus)
    self.mox.ReplayAll()

    # This is the test verification.
    with self.OutputCapturer():
      if expect_success:
        rss.ProgrammaticLogin()
      else:
        self.assertRaises(gdata.service.Error, rss.ProgrammaticLogin)
      self.mox.VerifyAll()

    if not expect_success:
      # Retries did not help, request still failed.
      regexp = re.compile(r'^Giving up on HTTP request')
      self.AssertOutputContainsWarning(regexp=regexp)
    elif len(statuses) > 1:
      # Warning expected if retries were needed.
      self.AssertOutputContainsWarning()
    else:
      # First try worked, expect no warnings.
      self.AssertOutputContainsWarning(invert=True)

  def testHttpClientRetryRequest(self):
    self._TestHttpClientRetryRequest([200])

  def testHttpClientRetryRequest403(self):
    self._TestHttpClientRetryRequest([403, 200])

  def testHttpClientRetryRequest403x2(self):
    self._TestHttpClientRetryRequest([403, 403, 200])

  def testHttpClientRetryRequest403x3(self):
    self._TestHttpClientRetryRequest([403, 403, 403, 200])

  def testHttpClientRetryRequest403x4(self):
    self._TestHttpClientRetryRequest([403, 403, 403, 403, 200])

  def testHttpClientRetryRequest403x5(self):
    # This one should exhaust the retries.
    self._TestHttpClientRetryRequest([403, 403, 403, 403, 403])

  def _TestRetryRequest(self, statuses):
    """Test retry logic for request method.

    |statuses| is list of http codes to simulate, where 200 means success.
    """
    expect_success = statuses[-1] == 200
    expected_status_index = len(statuses) - 1 if expect_success else 0

    mocked_ss = self.mox.CreateMock(gdata_lib.RetrySpreadsheetsService)
    args = ('GET', 'http://foo.bar')

    # This is the replay script for the test.
    for ix, status in enumerate(statuses):
      # Add index of status to track which status the request function is
      # returning.  It is expected to return the last return status if
      # successful (retries or not), but first return status if failed.
      retval = test_lib.EasyAttr(status=status, index=ix)
      mocked_ss.request(*args).AndReturn(retval)

    self.mox.ReplayAll()

    # This is the test verification.
    with self.OutputCapturer():
      # pylint: disable=W0212
      rval = gdata_lib.RetrySpreadsheetsService._RetryRequest(mocked_ss,
                                                              mocked_ss.request,
                                                              *args)
      self.mox.VerifyAll()
      self.assertEquals(statuses[expected_status_index], rval.status)
      self.assertEquals(expected_status_index, rval.index)

    if not expect_success:
      # Retries did not help, request still failed.
      regexp = re.compile(r'^Giving up on HTTP request')
      self.AssertOutputContainsWarning(regexp=regexp)
    elif expected_status_index > 0:
      # Warning expected if retries were needed.
      self.AssertOutputContainsWarning()
    else:
      # First try worked, expect no warnings.
      self.AssertOutputContainsWarning(invert=True)

  def testRetryRequest(self):
    self._TestRetryRequest([200])

  def testRetryRequest403(self):
    self._TestRetryRequest([403, 200])

  def testRetryRequest403x2(self):
    self._TestRetryRequest([403, 403, 200])

  def testRetryRequest403x3(self):
    self._TestRetryRequest([403, 403, 403, 200])

  def testRetryRequest403x4(self):
    self._TestRetryRequest([403, 403, 403, 403, 200])

  def testRetryRequest403x5(self):
    # This one should exhaust the retries.
    self._TestRetryRequest([403, 403, 403, 403, 403])


if __name__ == '__main__':
  unittest.main()
