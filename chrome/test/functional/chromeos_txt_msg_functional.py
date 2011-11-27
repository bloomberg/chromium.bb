#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from email.MIMEText import MIMEText
import logging
import os
import re
import smtplib
import sys
import urllib

import pyauto_functional
import pyauto

sys.path.append(os.path.join(pyauto.PyUITest.DataDir(), 'pyauto_private',
                'chromeos', 'network'))
from gsm_sim_info import SIM, PROVIDER_TXT_SERVER


class ChromeosTxtMsgSanity(pyauto.PyUITest):
    """Tests for ChromeOS text message handling"""

    def _SendText(self, mail_server, sender, phone_number,
                   mobile_provider, msg):
        """Sends a text message to a specific phone

        Args:
            mail_server: An SMTP instance.
            sender: Sender's email address.
            phone_number: The phone number the txt message is directed to.
            mobile_provider: A cellular provider defined in
                             gsm_sim_info.PROVIDER_TXT_SERVER
            msg: The message to be sent.

        """
        recipient = ('%s@%s' % (phone_number,
                     PROVIDER_TXT_SERVER[mobile_provider]))
        self._SendMail(mail_server, sender, recipient, None, msg)

    def _SendMail(self, mail_server, sender, recipients,
                   msg_subject, msg_body):
        """Sends an email using the provided smtp connection

        Args:
            mail_server: An SMTP instace.
            sender: Senders email address.
            recipients: Recipients email address.
            msg_subject: The subject line of the email.
            msg_body: The body of the email.
        """
        msg = MIMEText(msg_body)
        msg['To'] = recipients
        msg['From'] = sender
        if msg_subject:
            msg['Subject'] = msg_subject
        mail_server.sendmail(sender, recipients, msg.as_string())

    def _GetGmailServerInstance(self, email, password):
        """Creates an SMTP connection with the gmail mail server

        Args:
            email: A gmail address.
            password: The password for the gmail address.

        Returns:
            An SMTP connection instance.
        """
        mail_server = smtplib.SMTP('smtp.gmail.com', 587)
        mail_server.starttls()
        mail_server.ehlo()
        mail_server.login(email, password)
        return mail_server

    def _GetIMSI(self):
        """Obtains the IMSI by running modem status

        Returns:
            IMSI of device
        """
        modem_status = os.popen('modem status').read()
        imsi = re.search('IMSI:\s(\d+)', modem_status)
        if not imsi:
            raise Exception('GSM Modem not detected in device')
        return imsi.groups()[0]

    def _GetSIMInfo(self):
        """Returns information necessary to send messages

        Returns:
            A dictionary with the following format
            {
                'mdn' : <phone number>,
                'carrier': <carrier name>
            }
        """
        imsi = self._GetIMSI()
        sim_info = SIM.get(imsi, {})
        if not sim_info:
            raise Exception('Phone number for sim with IMSI=%s is not '
                            'recognized within config file' % imsi)
        return sim_info

    def setUp(self):
        # Connect to cellular service if not already connected.
        pyauto.PyUITest.setUp(self)
        connected_cellular = self.NetworkScan().get('connected_cellular')
        if not connected_cellular:
            self.ConnectToCellularNetwork()
            if not self.NetworkScan().get('connected_cellular'):
                raise Exception('Could not connect to cellular service.')
        else:
            logging.debug('Already connected to cellular service %s' %
                          connected_cellular)

        # Obtain sender, recipient, and SMTP instance.
        self.credentials = self.GetPrivateInfo()['test_account_with_smtp']
        self.sim = self._GetSIMInfo()
        self.mail_server = self._GetGmailServerInstance(
                             self.credentials['username'],
                             self.credentials['password'])

    def tearDown(self):
        self.DisconnectFromCellularNetwork()
        self.mail_server.close()
        for window in range(len(self.GetActiveNotifications())):
            self.CloseNotification(window)
        pyauto.PyUITest.tearDown(self)

    def testTxtMsgNotification(self):
        """Notifications are displayed for text messages"""
        msg = 'This is the text message'
        self._SendText(self.mail_server, self.credentials['username'],
                        self.sim['mdn'], self.sim['carrier'], msg)
        self.WaitForNotificationCount(1)
        notification_result = self.GetActiveNotifications()[0]['content_url']
        self.assertTrue(re.search(urllib.pathname2url(msg),
                        notification_result), 'Invalid message was displayed.  '
                        'Expected "%s" but did not find it"' % msg)

    def testLongTxtMsgNotification(self):
        """Notifications are displayed for long (>160 char) text messages."""
        long_msg = 'This is a really long message with spaces. Testing to '\
                   'make sure that chromeos is able to catch it and '\
                   'create a notifications for this message.'
        self._SendText(self.mail_server, self.credentials['username'],
                        self.sim['mdn'], self.sim['carrier'], long_msg)
        self.WaitForNotificationCount(1)

        # GetActiveNotifications throws an exception if the text message never
        # arrives.
        txt_msg = self.GetActiveNotifications()[0]
        txt_msg = txt_windows[0]['content_url']
        self.assertTrue(re.search(urllib.pathname2url(long_msg),
                        txt_msg), 'Invalid message was displayed. '
                        'Expected "%s" but did not find it"' % long_msg)


if __name__ == '__main__':
    pyauto_functional.Main()
