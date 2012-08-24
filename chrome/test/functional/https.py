#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

import pyauto_functional  # Must be imported before pyauto
import pyauto
import pyauto_paths

class HTTPSTest(pyauto.PyUITest):
  """TestCase for SSL."""

  def Debug(self):
    """Test method for experimentation.

    This method will not run automatically.
    Use: python chrome/test/functional/ssl.py ssl.SSLTest.Debug
    """
    while True:
      raw_input('Hit <enter> to dump info.. ')
      self.pprint(self.GetNavigationInfo())

  def testSSLPageBasic(self):
    """Verify the navigation state in an https page."""
    self.NavigateToURL('https://www.google.com')
    self.assertTrue(self.WaitUntil(
        lambda: self.GetNavigationInfo()['ssl']['security_style'],
                expect_retval='SECURITY_STYLE_AUTHENTICATED'))
    ssl = self.GetNavigationInfo()['ssl']
    self.assertFalse(ssl['displayed_insecure_content'])
    self.assertFalse(ssl['ran_insecure_content'])


class SSLTest(pyauto.PyUITest):
  """TestCase for SSL."""

  def setUp(self):
    self._https_server_ok = self.StartHttpsServer(
        pyauto.SSLOptions.CERT_OK,
        os.path.relpath(os.path.join(self.DataDir(), 'ssl'),
                        pyauto_paths.GetSourceDir()))
    self._https_server_expired = self.StartHttpsServer(
        pyauto.SSLOptions.CERT_EXPIRED,
        os.path.relpath(os.path.join(self.DataDir(), 'ssl'),
                        pyauto_paths.GetSourceDir()))
    self._https_server_mismatched = self.StartHttpsServer(
        pyauto.SSLOptions.CERT_MISMATCHED_NAME,
        os.path.relpath(os.path.join(self.DataDir(), 'ssl'),
                        pyauto_paths.GetSourceDir()))
    pyauto.PyUITest.setUp(self)

  def tearDown(self):
    pyauto.PyUITest.tearDown(self)
    self.StopHttpsServer(self._https_server_mismatched)
    self.StopHttpsServer(self._https_server_expired)
    self.StopHttpsServer(self._https_server_ok)

  def testSSLProceed(self):
    """Verify able to proceed from an interstitial page."""
    for server in (self._https_server_expired,
                   self._https_server_mismatched):
      url = server.GetURL('google.html').spec()
      self.NavigateToURL(url)
      # Equivalent to clicking 'proceed anyway' button.
      self.ActionOnSSLBlockingPage(proceed=True)
      self.assertTrue(
          'google' in self.GetActiveTabTitle().lower(),
          msg="Did not correctly proceed from the interstitial page.")

  def testSSLGoBack(self):
    """Verify able to go back from the interstitial page to the previous site.

    Visits a page with https error and then goes back using Browser::GoBack.
    """
    for server in (self._https_server_expired,
                   self._https_server_mismatched):
      self.NavigateToURL(
          self.GetHttpURLForDataPath('ssl', 'google.html'))
      first_page_title = self.GetActiveTabTitle()
      self.NavigateToURL(
          server.GetURL('google.html').spec())
      # Equivalent to clicking 'back to safety' button.
      self.ActionOnSSLBlockingPage(proceed=False)
      self.assertEqual(self.GetActiveTabTitle(), first_page_title,
                       msg="Did not go back to previous page correctly.")

  def testSSLCertOK(self):
    """Verify Certificate OK does not display interstitial page.

    This test also asserts that the page type is normal.
    """
    url = self._https_server_ok.GetURL('google.html').spec()
    self.NavigateToURL(url)
    result_dict = self.GetNavigationInfo()
    self.assertTrue(result_dict, msg='Could not determine the type of the page')
    self.assertEqual(
        result_dict['page_type'], 'NORMAL_PAGE',
        msg='Cert OK did not display page type normal (page_type=%s).' %
            result_dict['page_type'])

  def testSSLCertIsExpiredAndCertNameMismatches(self):
    """Verify Certificate Expiration and Certificate Mismatched name."""
    for server, cert_status_flag, msg in zip(
        (self._https_server_expired, self._https_server_mismatched),
        (pyauto.CERT_STATUS_DATE_INVALID,
         pyauto.CERT_STATUS_COMMON_NAME_INVALID),
        ('Cert has not expired', 'Cert name does not mismatch')):
      self.NavigateToURL(server.GetURL('google.html').spec())
      result_dict = self.GetSecurityState()
      self.assertTrue(result_dict, msg='Could not get security state info')
      self.assertTrue(
          result_dict['ssl_cert_status'] & pyauto.uint32_ptr.frompointer(
              cert_status_flag).value(),
          msg=msg)

  def testSSLCertAuthorityOK(self):
    """Verify Certificate OK is valid."""
    self.NavigateToURL(
        self._https_server_mismatched.GetURL('google.html').spec())
    result_dict = self.GetSecurityState()
    self.assertTrue(result_dict, msg='Could not get security state info')
    self.assertFalse(
        result_dict['ssl_cert_status'] & pyauto.uint32_ptr.frompointer(
            pyauto.CERT_STATUS_AUTHORITY_INVALID).value(),
        msg='Cert OK is invalid')


if __name__ == '__main__':
  pyauto_functional.Main()
