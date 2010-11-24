#!/usr/bin/python

# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

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
