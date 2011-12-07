# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

import pyauto_functional  # must be imported before pyauto
import pyauto
import pyauto_errors


class ChromeosEnterpriseEnroll(pyauto.PyUITest):
  """Test enrollment of chromebook device to enterprise domains.

  Preconditions:
  1) TPM is not locked. That is, the device was not 'owned' by successful
     enrollment to a domain, or by user sign in, since its last recovery.
  2) Files exist at: /root/.forget_usernames, /home/chronos/.oobe_completed,
     and /tmp/machine-info
  3) File does not exist at: /home/.shadow/install_attributes.pb
  """

  _CHROME_FLAGS_FOR_ENTERPRISE = [
    ('--device-management-url='
     'https://cros-auto.sandbox.google.com/devicemanagement/data/api'),
    '--gaia-host=gaiastaging.corp.google.com',
  ]

  def setUp(self):
    pyauto.PyUITest.setUp(self)
    self.assertFalse(self.IsEnterpriseDevice(), msg='Device is already '
                     'enrolled. Tests cannot continue.')
    # TODO(scunningham): Add precondition steps to workaround bug 22282.

  def ExtraChromeFlags(self):
    """Launches Chrome with custom flags for enterprise test domains.

    Overrides the default list of flags normally passed to Chrome by
    ExtraChromeFlags() in pyauto.py.
    """
    return self._CHROME_FLAGS_FOR_ENTERPRISE

  def testEnrollToCrOsUnmanagedDomain(self):
    """Try enroll to domain that does not support CrOS Management."""
    credentials = self.GetPrivateInfo()['test_enterprise_disabled_domain']
    enrollment_result = self.EnrollEnterpriseDevice(credentials['username'],
                                                    credentials['password'])
    self.assertTrue(enrollment_result == 'Enrollment failed.',
                    msg='Enrollment succeded to a domain that does not have '
                    'Chrome OS management enabled.')
    # TODO(scunningham): Add more precise error checking after bug 22280 is
    # fixed.

  def testEnrollWithInvalidPassword(self):
    """Try to enroll with an invalid account password."""
    credentials = self.GetPrivateInfo()['test_enterprise_enabled_domain']
    enrollment_result = self.EnrollEnterpriseDevice(credentials['username'],
                                                    '__bogus_password__')
    self.assertTrue(enrollment_result == 'Enrollment failed.',
                    msg='Enrollment succeeded to a domain that has Chrome OS '
                    'management enabled, using an invalid password.')
    # TODO(scunningham): Add more precise error checking after bug 22280 is
    # fixed.

  def testEnrollWithInvalidUsername(self):
    """Try to enroll with an invalid account username."""
    credentials = self.GetPrivateInfo()['test_enterprise_enabled_domain']
    enrollment_result = self.EnrollEnterpriseDevice('__bogus_username__',
                                                    credentials['password'])
    self.assertTrue(enrollment_result == 'Enrollment failed.',
                    msg='Enrollment succeeded to a domain that has Chrome OS '
                    'management enabled, using an invalid user account.')
    # TODO(scunningham): Add more precise error checking after bug 22280 is
    # fixed.


if __name__ == '__main__':
  pyauto_functional.Main()
