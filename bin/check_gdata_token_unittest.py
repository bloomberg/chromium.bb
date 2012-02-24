#!/usr/bin/python

# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for cros_portage_upgrade.py."""

import filecmp
import os
import re
import shutil
import sys
import unittest

import gdata.service
import gdata.spreadsheet.service as gdata_ss_service
import mox

import chromite.bin.check_gdata_token as cgt
import chromite.lib.cros_build_lib as build_lib
import chromite.lib.cros_test_lib as test_lib
import chromite.lib.gdata_lib as gdata_lib

# pylint: disable=W0212,R0904,E1120,E1101


class MainTest(test_lib.MoxTestCase):
  """Test argument handling at the main method level."""

  def setUp(self):
    """Setup for all tests in this class."""
    mox.MoxTestBase.setUp(self)

  def testHelp(self):
    """Test that --help is functioning"""
    argv = [ '--help' ]

    with self.OutputCapturer() as output:
      # Running with --help should exit with code==0.
      self.AssertFuncSystemExitZero(cgt.main, argv)

    # Verify that a message beginning with "Usage: " was printed.
    stdout = output.GetStdout()
    self.assertTrue(stdout.startswith('Usage: '))

  def testMainOutsideChroot(self):
    """Test flow outside chroot"""
    argv = []
    mocked_outsidechroot = self.mox.CreateMock(cgt.OutsideChroot)

    # Create replay script.
    self.mox.StubOutWithMock(build_lib, 'IsInsideChroot')
    self.mox.StubOutWithMock(cgt.OutsideChroot, '__new__')

    build_lib.IsInsideChroot().AndReturn(False)
    cgt.OutsideChroot.__new__(cgt.OutsideChroot, argv,
                              ).AndReturn(mocked_outsidechroot)
    mocked_outsidechroot.Run()
    self.mox.ReplayAll()

    # Run test verification.
    with self.OutputCapturer():
      cgt.main(argv)
    self.mox.VerifyAll()

  def testMainInsideChroot(self):
    """Test flow inside chroot"""
    argv = []
    mocked_insidechroot = self.mox.CreateMock(cgt.InsideChroot)

    # Create replay script.
    self.mox.StubOutWithMock(build_lib, 'IsInsideChroot')
    self.mox.StubOutWithMock(cgt.InsideChroot, '__new__')

    build_lib.IsInsideChroot().AndReturn(True)
    cgt.InsideChroot.__new__(cgt.InsideChroot
                             ).AndReturn(mocked_insidechroot)
    mocked_insidechroot.Run()
    self.mox.ReplayAll()

    # Run test verification.
    with self.OutputCapturer():
      cgt.main(argv)
    self.mox.VerifyAll()


class OutsideChrootTest(test_lib.MoxTestCase):
  """Test flow when run outside chroot."""

  def setUp(self):
    """Setup for all tests in this class."""
    mox.MoxTestBase.setUp(self)

  def _MockOutsideChroot(self, *args):
    """Prepare mocked OutsideChroot object with |args|."""
    mocked_outsidechroot = self.mox.CreateMock(cgt.OutsideChroot)

    mocked_outsidechroot.args = list(args) if args else []

    return mocked_outsidechroot

  def testOutsideChrootRestartFail(self):
    mocked_outsidechroot = self._MockOutsideChroot()

    self.mox.StubOutWithMock(build_lib, 'RunCommand')
    cmd = ['check_gdata_token']
    run_result = test_lib.EasyAttr(returncode=1)

    # Create replay script.
    build_lib.RunCommand(cmd, enter_chroot=True,
                         print_cmd=False,
                         error_code_ok=True).AndReturn(run_result)
    self.mox.ReplayAll()

    # Run test verification.
    with self.OutputCapturer():
      # Test should exit with failure.
      self.AssertFuncSystemExitNonZero(cgt.OutsideChroot.Run,
                                       mocked_outsidechroot)

    self.mox.VerifyAll()

    self.AssertOutputContainsError()

  def testOutsideChrootNoTokenFile(self):
    mocked_outsidechroot = self._MockOutsideChroot('foo')

    self.mox.StubOutWithMock(cgt, '_ChrootPathToExternalPath')
    self.mox.StubOutWithMock(os.path, 'exists')
    self.mox.StubOutWithMock(build_lib, 'RunCommand')
    cmd = ['check_gdata_token', 'foo']
    run_result = test_lib.EasyAttr(returncode=0)

    # Create replay script.
    build_lib.RunCommand(cmd, enter_chroot=True,
                         print_cmd=False,
                         error_code_ok=True).AndReturn(run_result)
    cgt._ChrootPathToExternalPath(cgt.TOKEN_FILE).AndReturn('chr-tok')
    os.path.exists('chr-tok').AndReturn(False)
    self.mox.ReplayAll()

    # Run test verification.
    with self.OutputCapturer():
      # Test should exit with failure.
      self.AssertFuncSystemExitNonZero(cgt.OutsideChroot.Run,
                                       mocked_outsidechroot)

    self.mox.VerifyAll()

    self.AssertOutputContainsError()

  def testOutsideChrootNewTokenFile(self):
    mocked_outsidechroot = self._MockOutsideChroot('foo')

    self.mox.StubOutWithMock(cgt, '_ChrootPathToExternalPath')
    self.mox.StubOutWithMock(os.path, 'exists')
    self.mox.StubOutWithMock(shutil, 'copy2')
    self.mox.StubOutWithMock(build_lib, 'RunCommand')
    cmd = ['check_gdata_token', 'foo']
    run_result = test_lib.EasyAttr(returncode=0)

    # Create replay script.
    build_lib.RunCommand(cmd, enter_chroot=True,
                         print_cmd=False,
                         error_code_ok=True).AndReturn(run_result)
    cgt._ChrootPathToExternalPath(cgt.TOKEN_FILE).AndReturn('chr-tok')
    os.path.exists('chr-tok').AndReturn(True)
    os.path.exists(cgt.TOKEN_FILE).AndReturn(False)
    shutil.copy2('chr-tok', cgt.TOKEN_FILE)
    self.mox.ReplayAll()

    # Run test verification.
    with self.OutputCapturer():
      cgt.OutsideChroot.Run(mocked_outsidechroot)
    self.mox.VerifyAll()

  def testOutsideChrootDifferentTokenFile(self):
    mocked_outsidechroot = self._MockOutsideChroot('foo')

    self.mox.StubOutWithMock(cgt, '_ChrootPathToExternalPath')
    self.mox.StubOutWithMock(os.path, 'exists')
    self.mox.StubOutWithMock(shutil, 'copy2')
    self.mox.StubOutWithMock(filecmp, 'cmp')
    self.mox.StubOutWithMock(build_lib, 'RunCommand')
    cmd = ['check_gdata_token', 'foo']
    run_result = test_lib.EasyAttr(returncode=0)

    # Create replay script.
    build_lib.RunCommand(cmd, enter_chroot=True,
                         print_cmd=False,
                         error_code_ok=True).AndReturn(run_result)
    cgt._ChrootPathToExternalPath(cgt.TOKEN_FILE).AndReturn('chr-tok')
    os.path.exists('chr-tok').AndReturn(True)
    os.path.exists(cgt.TOKEN_FILE).AndReturn(True)
    filecmp.cmp(cgt.TOKEN_FILE, 'chr-tok').AndReturn(False)
    shutil.copy2('chr-tok', cgt.TOKEN_FILE)
    self.mox.ReplayAll()

    # Run test verification.
    with self.OutputCapturer():
      cgt.OutsideChroot.Run(mocked_outsidechroot)
    self.mox.VerifyAll()

  def testOutsideChrootNoChangeInTokenFile(self):
    mocked_outsidechroot = self._MockOutsideChroot('foo')

    self.mox.StubOutWithMock(cgt, '_ChrootPathToExternalPath')
    self.mox.StubOutWithMock(os.path, 'exists')
    self.mox.StubOutWithMock(filecmp, 'cmp')
    self.mox.StubOutWithMock(build_lib, 'RunCommand')
    cmd = ['check_gdata_token', 'foo']
    run_result = test_lib.EasyAttr(returncode=0)

    # Create replay script.
    build_lib.RunCommand(cmd, enter_chroot=True,
                         print_cmd=False,
                         error_code_ok=True).AndReturn(run_result)
    cgt._ChrootPathToExternalPath(cgt.TOKEN_FILE).AndReturn('chr-tok')
    os.path.exists('chr-tok').AndReturn(True)
    os.path.exists(cgt.TOKEN_FILE).AndReturn(True)
    filecmp.cmp(cgt.TOKEN_FILE, 'chr-tok').AndReturn(True)
    self.mox.ReplayAll()

    # Run test verification.
    with self.OutputCapturer():
      cgt.OutsideChroot.Run(mocked_outsidechroot)
    self.mox.VerifyAll()

class InsideChrootTest(test_lib.MoxTestCase):
  """Test flow when run inside chroot."""

  def setUp(self):
    """Setup for all tests in this class."""
    mox.MoxTestBase.setUp(self)

  def _MockInsideChroot(self):
    """Prepare mocked OutsideChroot object."""
    mic = self.mox.CreateMock(cgt.InsideChroot)

    mic.creds = self.mox.CreateMock(gdata_lib.Creds)
    mic.gd_client = self.mox.CreateMock(gdata_ss_service.SpreadsheetsService)

    return mic

  def testInsideChrootValidateOK(self):
    mocked_insidechroot = self._MockInsideChroot()

    # Create replay script.
    mocked_insidechroot._ValidateToken().AndReturn(True)
    self.mox.ReplayAll()

    # Run test verification.
    with self.OutputCapturer():
      cgt.InsideChroot.Run(mocked_insidechroot)
    self.mox.VerifyAll()

  def testInsideChrootValidateFailGenerateOK(self):
    mocked_insidechroot = self._MockInsideChroot()

    # Create replay script.
    mocked_insidechroot._ValidateToken().AndReturn(False)
    mocked_insidechroot._GenerateToken().AndReturn(True)
    self.mox.ReplayAll()

    # Run test verification.
    with self.OutputCapturer():
      cgt.InsideChroot.Run(mocked_insidechroot)
    self.mox.VerifyAll()

  def testInsideChrootValidateFailGenerateFail(self):
    mocked_insidechroot = self._MockInsideChroot()

    # Create replay script.
    mocked_insidechroot._ValidateToken().AndReturn(False)
    mocked_insidechroot._GenerateToken().AndReturn(False)
    self.mox.ReplayAll()

    # Run test verification.
    with self.OutputCapturer():
      # Test should exit with failure.
      self.AssertFuncSystemExitNonZero(cgt.InsideChroot.Run,
                                       mocked_insidechroot)
    self.mox.VerifyAll()

    self.AssertOutputContainsError()

  def testGenerateTokenOK(self):
    mocked_insidechroot = self._MockInsideChroot()

    # Create replay script.
    mocked_creds = mocked_insidechroot.creds
    mocked_gdclient = mocked_insidechroot.gd_client
    mocked_creds.email = 'joe@chromium.org'
    mocked_creds.password = 'shhh'
    auth_token = 'SomeToken'

    mocked_creds.LoadCreds(cgt.CRED_FILE)
    mocked_gdclient.ProgrammaticLogin()
    mocked_gdclient.GetClientLoginToken().AndReturn(auth_token)
    mocked_creds.SetAuthToken(auth_token)
    mocked_creds.StoreAuthToken(cgt.TOKEN_FILE)
    self.mox.ReplayAll()

    # Run test verification.
    with self.OutputCapturer():
      result = cgt.InsideChroot._GenerateToken(mocked_insidechroot)
      self.assertTrue(result, '_GenerateToken should have passed')
    self.mox.VerifyAll()

  def testGenerateTokenFail(self):
    mocked_insidechroot = self._MockInsideChroot()

    # Create replay script.
    mocked_creds = mocked_insidechroot.creds
    mocked_gdclient = mocked_insidechroot.gd_client
    mocked_creds.email = 'joe@chromium.org'
    mocked_creds.password = 'shhh'

    mocked_creds.LoadCreds(cgt.CRED_FILE)
    mocked_gdclient.ProgrammaticLogin(
      ).AndRaise(gdata.service.BadAuthentication())
    self.mox.ReplayAll()

    # Run test verification.
    with self.OutputCapturer():
      result = cgt.InsideChroot._GenerateToken(mocked_insidechroot)
      self.assertFalse(result, '_GenerateToken should have failed')
    self.mox.VerifyAll()

    self.AssertOutputContainsError()

  def testValidateTokenMissing(self):
    mocked_insidechroot = self._MockInsideChroot()

    # Create replay script.
    self.mox.StubOutWithMock(os.path, 'exists')

    os.path.exists(cgt.TOKEN_FILE).AndReturn(False)
    self.mox.ReplayAll()

    # Run test verification.
    with self.OutputCapturer():
      result = cgt.InsideChroot._ValidateToken(mocked_insidechroot)
      self.assertFalse(result, '_ValidateToken should have failed')
    self.mox.VerifyAll()

  def testValidateTokenOK(self):
    mocked_insidechroot = self._MockInsideChroot()

    # Create replay script.
    auth_token = 'SomeToken'
    mocked_insidechroot.creds.auth_token = auth_token

    self.mox.StubOutWithMock(os.path, 'exists')

    os.path.exists(cgt.TOKEN_FILE).AndReturn(True)
    mocked_insidechroot.creds.LoadAuthToken(cgt.TOKEN_FILE)
    mocked_insidechroot.gd_client.SetClientLoginToken(auth_token)
    mocked_insidechroot.gd_client.GetSpreadsheetsFeed()
    self.mox.ReplayAll()

    # Run test verification.
    with self.OutputCapturer():
      result = cgt.InsideChroot._ValidateToken(mocked_insidechroot)
      self.assertTrue(result, '_ValidateToken should have passed')
    self.mox.VerifyAll()

  def testValidateTokenFail(self):
    mocked_insidechroot = self._MockInsideChroot()

    # Create replay script.
    auth_token = 'SomeToken'
    mocked_insidechroot.creds.auth_token = auth_token

    self.mox.StubOutWithMock(os.path, 'exists')

    os.path.exists(cgt.TOKEN_FILE).AndReturn(True)
    mocked_insidechroot.creds.LoadAuthToken(cgt.TOKEN_FILE)
    mocked_insidechroot.gd_client.SetClientLoginToken(auth_token)
    expired_error = gdata.service.RequestError({'reason': 'Token expired'})
    mocked_insidechroot.gd_client.GetSpreadsheetsFeed().AndRaise(expired_error)
    self.mox.ReplayAll()

    # Run test verification.
    with self.OutputCapturer():
      result = cgt.InsideChroot._ValidateToken(mocked_insidechroot)
      self.assertFalse(result, '_ValidateToken should have failed')
    self.mox.VerifyAll()


if __name__ == '__main__':
  unittest.main()
