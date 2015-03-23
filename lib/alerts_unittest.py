# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for the alerts.py module."""

from __future__ import print_function

import mock
import os
import smtplib
import socket

from chromite.lib import alerts
from chromite.lib import cros_test_lib


class SmtpServerTest(cros_test_lib.MockTestCase):
  """Tests for Smtp server."""

  # pylint: disable=protected-access

  def setUp(self):
    self.smtp_mock = self.PatchObject(smtplib, 'SMTP')

  def testBasic(self):
    """Basic sanity check."""
    msg = alerts.CreateEmail('fake subject', 'fake@localhost', 'fake message')
    server = alerts.SmtpServer(('localhost', 1))
    ret = server.Send(msg)
    self.assertTrue(ret)
    self.assertEqual(self.smtp_mock.call_count, 1)

  def testRetryException(self):
    """Verify we try sending multiple times & don't abort socket.error."""
    self.smtp_mock.side_effect = socket.error('test fail')
    msg = alerts.CreateEmail('fake subject', 'fake@localhost', 'fake message')
    server = alerts.SmtpServer(('localhost', 1))
    ret = server.Send(msg)
    self.assertFalse(ret)
    self.assertEqual(self.smtp_mock.call_count, 4)


class GmailServerTest(cros_test_lib.MockTestCase):
  """Tests for Gmail server."""

  def testBasic(self):
    """Test send email normally."""
    self.PatchObject(os.path, 'isfile', return_value=True)
    self.PatchObject(alerts, 'apiclient_build')
    fake_creds = mock.MagicMock()
    fake_creds.invalid = False
    fake_storage = mock.MagicMock()
    fake_storage.get = mock.MagicMock(return_value=fake_creds)
    self.PatchObject(alerts, 'oauth_client_fileio')
    self.PatchObject(alerts.oauth_client_fileio, 'Storage',
                     return_value=fake_storage)
    msg = alerts.CreateEmail('fake subject', 'fake@localhost', 'fake msg')
    server = alerts.GmailServer()
    ret = server.Send(msg)
    self.assertTrue(ret)

  def testCredsFileNotExist(self):
    """Test credentials do not exists."""
    self.PatchObject(os.path, 'isfile', return_value=False)
    msg = alerts.CreateEmail('fake subject', 'fake@localhost', 'fake msg')
    server = alerts.GmailServer()
    ret = server.Send(msg)
    self.assertFalse(ret)

  def testCredsInvalid(self):
    """Test invalid credentials."""
    self.PatchObject(os.path, 'isfile', return_value=True)
    self.PatchObject(alerts, 'apiclient_build')
    self.PatchObject(alerts, 'oauth_client_fileio')
    self.PatchObject(alerts.oauth_client_fileio, 'Storage')
    msg = alerts.CreateEmail('fake subject', 'fake@localhost', 'fake msg')
    server = alerts.GmailServer()
    ret = server.Send(msg)
    self.assertFalse(ret)


class SendEmailTest(cros_test_lib.MockTestCase):
  """Tests for SendEmail."""

  def testSmtp(self):
    """Smtp sanity check."""
    send_mock = self.PatchObject(alerts.SmtpServer, 'Send')
    alerts.SendEmail('mail', 'root@localhost')
    self.assertEqual(send_mock.call_count, 1)

  def testGmail(self):
    """Gmail sanity check."""
    send_mock = self.PatchObject(alerts.GmailServer, 'Send')
    alerts.SendEmail('mail', 'root@localhost', server=alerts.GmailServer())
    self.assertEqual(send_mock.call_count, 1)


class SendEmailLogTest(cros_test_lib.MockTestCase):
  """Tests for SendEmailLog()."""

  def testSmtp(self):
    """Smtp sanity check."""
    send_mock = self.PatchObject(alerts.SmtpServer, 'Send')
    alerts.SendEmailLog('mail', 'root@localhost')
    self.assertEqual(send_mock.call_count, 1)

  def testGmail(self):
    """Gmail sanity check."""
    send_mock = self.PatchObject(alerts.GmailServer, 'Send')
    alerts.SendEmailLog('mail', 'root@localhost', server=alerts.GmailServer())
    self.assertEqual(send_mock.call_count, 1)


def main(_argv):
  # No need to make unittests sleep.
  alerts.SmtpServer.SMTP_RETRY_DELAY = 0

  cros_test_lib.main(module=__name__)
