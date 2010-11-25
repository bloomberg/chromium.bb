#!/usr/bin/python

# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import email
import os
import smtplib

import pyauto_functional
import pyauto_utils


"""Commonly used functions for PyAuto tests."""

def DownloadFileFromDownloadsDataDir(test, file_name):
  """Download a file from downloads data directory, in first tab first window.

  Args:
    test: derived from pyauto.PyUITest - base class for UI test cases
    file_name: name of file to download
  """
  file_url = test.GetFileURLForDataPath(os.path.join('downloads', file_name))
  downloaded_pkg = os.path.join(test.GetDownloadDirectory().value(),
                                file_name)
  # Check if file already exists. If so then delete it.
  if os.path.exists(downloaded_pkg):
    RemoveDownloadedTestFile(test, file_name)
  test.DownloadAndWaitForStart(file_url)
  test.WaitForAllDownloadsToComplete()


def RemoveDownloadedTestFile(test, file_name):
  """Delete a file from the downloads directory.

  Arg:
    test: derived from pyauto.PyUITest - base class for UI test cases
    file_name: name of file to remove
  """
  downloaded_pkg = os.path.join(test.GetDownloadDirectory().value(),
                                file_name)
  pyauto_utils.RemovePath(downloaded_pkg)
  pyauto_utils.RemovePath(downloaded_pkg + '.crdownload')


def GoogleAccountsLogin(test, url, username, password):
  """Log into Google Accounts.

  Attempts to login to Google by entering the username/password into the google
  login page and click submit button.

  Args:
    test: derived from pyauto.PyUITest - base class for UI test cases.
    username: users login input.
    password: users login password input.
  """
  test.NavigateToURL('https://www.google.com/accounts/')
  email_id = 'document.getElementById("Email").value = \"%s\"; ' \
             'window.domAutomationController.send("done")' % username
  password = 'document.getElementById("Passwd").value = \"%s\"; ' \
             'window.domAutomationController.send("done")' % password
  test.ExecuteJavascript(email_id);
  test.ExecuteJavascript(password);
  test.ExecuteJavascript('document.getElementById("gaia_loginform").submit();'
                         'window.domAutomationController.send("done")')


def VerifyGoogleAccountCredsFilled(test, username, password):
  """Verify stored/saved user and password values to the values in the field.

  Args:
    test: derived from pyauto.PyUITest - base class for UI test cases.
    username: user log in input.
    password: user log in password input.
  """
  email_value = test.GetDOMValue('document.getElementById("Email").value')
  passwd_value = test.GetDOMValue('document.getElementById("Passwd").value')
  test.assertEqual(email_value, username)
  test.assertEqual(passwd_value, password)


def ClearPasswords(test):
  """Clear saved passwords."""
  test.ClearBrowsingData(['PASSWORDS'], 'EVERYTHING')


def Shell2(cmd_string, bg=False):
  """Run a shell command.

  Args:
    cmd_string: command to run
    bg: should the process be run in background? Default: False

  Returns:
    Output, return code
    """
  if not cmd_string: return ('', 0)
  if bg:
    cmd_string += ' 1>/dev/null 2>&1 &'
  proc = os.popen(cmd_string)
  if bg: return ('Background process: %s' % cmd_string, 0)
  out = proc.read()
  retcode = proc.close()
  if not retcode:  # Success
    retcode = 0
  return (out, retcode)


def SendMail(send_from, send_to, subject, text, smtp, file_to_send=None):
  """Send mail to all the group to notify about the crash and uploaded data.

  Args:
    send_from: from mail id.
    send_to: to mail id.
    subject: mail subject.
    text: mail body.
    smtp: The smtp to use.
    file_to_send: attachments for the mail.
  """
  msg = email.MIMEMultipart.MIMEMultipart()
  msg['From'] = send_from
  msg['To'] = send_to
  msg['Date'] = email.Utils.formatdate(localtime=True)
  msg['Subject'] = subject

  # To send multiple files in one message, introduce for loop here for files.
  msg.attach(email.MIMEText.MIMEText(text))
  part = email.MIMEBase.MIMEBase('application', 'octet-stream')
  if file_to_send is not None:
    part.set_payload(open(file_to_send,'rb').read())
    email.Encoders.encode_base64(part)
    part.add_header('Content-Disposition',
                    'attachment; filename="%s"'
                    % os.path.basename(file_to_send))
    msg.attach(part)
  smtp_obj = smtplib.SMTP(smtp)
  smtp_obj.sendmail(send_from, send_to, msg.as_string())
  smtp_obj.close()

