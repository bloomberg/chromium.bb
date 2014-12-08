# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for the alerts.py module."""

from __future__ import print_function

from email.mime.text import MIMEText
import smtplib
import socket

from chromite.lib import alerts
from chromite.lib import cros_test_lib


class SendEmailHelperTest(cros_test_lib.MockTestCase):
  """Tests for _SendEmailHelper()."""

  # pylint: disable=protected-access

  def setUp(self):
    self.smtp_mock = self.PatchObject(smtplib, 'SMTP')

  def testBasic(self):
    """Basic sanity check."""
    ret = alerts._SendEmailHelper(('localhost', 1), 'me@localhost',
                                  'root@localhost', MIMEText('mail'))
    self.assertTrue(ret)
    self.assertEqual(self.smtp_mock.call_count, 1)

  def testRetryException(self):
    """Verify we try sending multiple times & don't abort socket.error."""
    self.smtp_mock.side_effect = socket.error('test fail')
    ret = alerts._SendEmailHelper(('localhost', 1), 'me@localhost',
                                  'root@localhost', MIMEText('mail'))
    self.assertFalse(ret)
    self.assertEqual(self.smtp_mock.call_count, 4)


class SendEmailTest(cros_test_lib.MockTestCase):
  """Tests for SendEmailTest()."""

  def setUp(self):
    self.send_mock = self.PatchObject(alerts, '_SendEmailHelper')

  def testBasic(self):
    """Basic sanity check."""
    alerts.SendEmail('mail', 'root@localhost')
    self.assertEqual(self.send_mock.call_count, 1)


class SendEmailLogTest(cros_test_lib.MockTestCase):
  """Tests for SendEmailLog()."""

  def setUp(self):
    self.send_mock = self.PatchObject(alerts, 'SendEmail')

  def testBasic(self):
    """Basic sanity check."""
    alerts.SendEmailLog('mail', 'root@localhost')
    self.assertEqual(self.send_mock.call_count, 1)


def main(_argv):
  # No need to make unittests sleep.
  alerts.SMTP_RETRY_DELAY = 0

  cros_test_lib.main(module=__name__)
