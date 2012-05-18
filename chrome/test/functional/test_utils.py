# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import ctypes
import email
import logging
import os
import platform
import shutil
import smtplib
import subprocess
import sys
import types

import pyauto_functional
import pyauto
import pyauto_utils


"""Commonly used functions for PyAuto tests."""

def CrashBrowser(test):
  """Crashes the browser by navigating to special URL."""
  test.NavigateToURL('chrome://inducebrowsercrashforrealz')


def CopyFileFromDataDirToDownloadDir(test, file_path):
  """Copy a file from data directory to downloads directory.

  Args:
    test: derived from pyauto.PyUITest - base class for UI test cases.
    path: path of the file relative to the data directory
  """
  data_file = os.path.join(test.DataDir(), file_path)
  download_dir = test.GetDownloadDirectory().value()
  shutil.copy(data_file, download_dir)

def DownloadFileFromDownloadsDataDir(test, file_name):
  """Download a file from downloads data directory, in first tab, first window.

  Args:
    test: derived from pyauto.PyUITest - base class for UI test cases.
    file_name: name of file to download.
  """
  file_url = test.GetFileURLForDataPath(os.path.join('downloads', file_name))
  downloaded_pkg = os.path.join(test.GetDownloadDirectory().value(),
                                file_name)
  # Check if file already exists. If so then delete it.
  if os.path.exists(downloaded_pkg):
    RemoveDownloadedTestFile(test, file_name)
  pre_download_ids = [x['id'] for x in test.GetDownloadsInfo().Downloads()]
  test.DownloadAndWaitForStart(file_url)
  test.WaitForAllDownloadsToComplete(pre_download_ids)


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


def GoogleAccountsLogin(test, username, password, tab_index=0, windex=0):
  """Log into Google Accounts.

  Attempts to login to Google by entering the username/password into the google
  login page and click submit button.

  Args:
    test: derived from pyauto.PyUITest - base class for UI test cases.
    username: users login input.
    password: users login password input.
    tab_index: The tab index, default is 0.
    windex: The window index, default is 0.
  """
  test.NavigateToURL('https://accounts.google.com/', windex, tab_index)
  email_id = 'document.getElementById("Email").value = "%s"; ' \
             'window.domAutomationController.send("done")' % username
  password = 'document.getElementById("Passwd").value = "%s"; ' \
             'window.domAutomationController.send("done")' % password
  test.ExecuteJavascript(email_id, tab_index, windex)
  test.ExecuteJavascript(password, tab_index, windex)
  test.assertTrue(test.SubmitForm('gaia_loginform', tab_index, windex))


def VerifyGoogleAccountCredsFilled(test, username, password, tab_index=0,
                                   windex=0):
  """Verify stored/saved user and password values to the values in the field.

  Args:
    test: derived from pyauto.PyUITest - base class for UI test cases.
    username: user log in input.
    password: user log in password input.
    tab_index: The tab index, default is 0.
    windex: The window index, default is 0.
  """
  email_value = test.GetDOMValue('document.getElementById("Email").value',
                                 tab_index, windex)
  passwd_value = test.GetDOMValue('document.getElementById("Passwd").value',
                                  tab_index, windex)
  test.assertEqual(email_value, username)
  # Not using assertEqual because if it fails it would end up dumping the
  # password (which is supposed to be private)
  test.assertTrue(passwd_value == password)


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


def GetFreeSpace(path):
  """Returns the free space (in bytes) on the drive containing |path|."""
  if sys.platform == 'win32':
    free_bytes = ctypes.c_ulonglong(0)
    ctypes.windll.kernel32.GetDiskFreeSpaceExW(
        ctypes.c_wchar_p(os.path.dirname(path)), None, None,
        ctypes.pointer(free_bytes))
    return free_bytes.value
  fs_stat = os.statvfs(path)
  return fs_stat.f_bsize * fs_stat.f_bavail


def StripUnmatchedKeys(item_to_strip, reference_item):
  """Returns a copy of 'item_to_strip' where unmatched key-value pairs in
  every dictionary are removed.

  This will examine each dictionary in 'item_to_strip' recursively, and will
  remove keys that are not found in the corresponding dictionary in
  'reference_item'. This is useful for testing equality of a subset of data.

  Items may contain dictionaries, lists, or primitives, but only corresponding
  dictionaries will be stripped. A corresponding entry is one which is found
  in the same index in the corresponding parent array or at the same key in the
  corresponding parent dictionary.

  Arg:
    item_to_strip: item to copy and remove all unmatched key-value pairs
    reference_item: item that serves as a reference for which keys-value pairs
                    to strip from 'item_to_strip'

  Returns:
    a copy of 'item_to_strip' where all key-value pairs that do not have a
    matching key in 'reference_item' are removed

  Example:
    item_to_strip = {'tabs': 3,
                     'time': 5908}
    reference_item = {'tabs': 2}
    StripUnmatchedKeys(item_to_strip, reference_item) will return {'tabs': 3}
  """
  def StripList(list1, list2):
    return_list = copy.deepcopy(list2)
    for i in range(min(len(list1), len(list2))):
      return_list[i] = StripUnmatchedKeys(list1[i], list2[i])
    return return_list

  def StripDict(dict1, dict2):
    return_dict = {}
    for key in dict1:
      if key in dict2:
        return_dict[key] = StripUnmatchedKeys(dict1[key], dict2[key])
    return return_dict

  item_to_strip_type = type(item_to_strip)
  if item_to_strip_type is type(reference_item):
    if item_to_strip_type is types.ListType:
      return StripList(item_to_strip, reference_item)
    elif item_to_strip_type is types.DictType:
      return StripDict(item_to_strip, reference_item)
  return copy.deepcopy(item_to_strip)


def StringContentCheck(test, content_string, have_list, nothave_list):
  """Check for the presence or absence of strings within content.

  Confirm all strings in |have_list| are found in |content_string|.
  Confirm all strings in |nothave_list| are not found in |content_string|.

  Args:
    content_string: string containing the content to check.
    have_list: list of strings expected to be found within the content.
    nothave_list: list of strings expected to not be found within the content.
  """
  for s in have_list:
    test.assertTrue(s in content_string,
                    msg='"%s" missing from content.' % s)
  for s in nothave_list:
    test.assertTrue(s not in content_string,
                    msg='"%s" unexpectedly contained in content.' % s)


def CallFunctionWithNewTimeout(self, new_timeout, function):
  """Sets the timeout to |new_timeout| and calls |function|.

  This method resets the timeout before returning.
  """
  timeout_changer = pyauto.PyUITest.ActionTimeoutChanger(
      self, new_timeout)
  logging.info('Automation execution timeout has been changed to %d. '
               'If the timeout is large the test might appear to hang.'
               % new_timeout)
  function()
  del timeout_changer


def GetOmniboxMatchesFor(self, text, windex=0, attr_dict=None):
    """Fetch omnibox matches with the given attributes for the given query.

    Args:
      text: the query text to use
      windex: the window index to work on. Defaults to 0 (first window)
      attr_dict: the dictionary of properties to be satisfied

    Returns:
      a list of match items
    """
    self.SetOmniboxText(text, windex=windex)
    self.WaitUntilOmniboxQueryDone(windex=windex)
    if not attr_dict:
      matches = self.GetOmniboxInfo(windex=windex).Matches()
    else:
      matches = self.GetOmniboxInfo(windex=windex).MatchesWithAttributes(
          attr_dict=attr_dict)
    return matches


def GetMemoryUsageOfProcess(pid):
  """Queries the system for the current memory usage of a specified process.

  This function only works in Linux and ChromeOS.

  Args:
    pid: The integer process identifier for the process to use.

  Returns:
    The memory usage of the process in MB, given as a float.  If the process
    doesn't exist on the machine, then the value 0 is returned.
  """
  assert pyauto.PyUITest.IsLinux() or pyauto.PyUITest.IsChromeOS()
  process = subprocess.Popen('ps h -o rss -p %s' % pid, shell=True,
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  stdout = process.communicate()[0]
  if stdout:
    return float(stdout.strip()) / 1024
  else:
    return 0


def GetCredsKey():
  """Get the credential key associated with a bot on the waterfall.

  The key is associated with the proper credentials in the text data file stored
  in the private directory. The key determines a bot's OS and machine name. Each
  key credential is associated with its own user/password value. This allows
  sync integration tests to run in parallel on all bots.

  Returns:
    A String of the credentials key for the specified bot. Otherwise None.
  """
  if pyauto.PyUITest.IsWin():
    system_name = 'win'
  elif pyauto.PyUITest.IsLinux():
    system_name = 'linux'
  elif pyauto.PyUITest.IsMac():
    system_name = 'mac'
  else:
    return None
  node = platform.uname()[1].split('.')[0]
  creds_key = 'test_google_acct_%s_%s' % (system_name, node)
  return creds_key


def SignInToSyncAndVerifyState(test, account_key):
  """Sign into sync and verify that it was successful.

  Args:
    test: derived from pyauto.PyUITest - base class for UI test cases.
    account_key: the credentials key in the private account dictionary file.
  """
  creds = test.GetPrivateInfo()[account_key]
  username = creds['username']
  password = creds['password']
  test.assertTrue(test.GetSyncInfo()['last synced'] == 'Never')
  test.assertTrue(test.SignInToSync(username, password))
  test.assertTrue(test.GetSyncInfo()['last synced'] == 'Just now')


def LoginToDevice(test, test_account='test_google_account'):
  """Login to the Chromeos device using the given test account.

  If no test account is specified, we use test_google_account as the default.
  You can choose test accounts from -
  chrome/test/data/pyauto_private/private_tests_info.txt 

  Args:
    test_account: The account used to login to the Chromeos device.
  """
  if not test.GetLoginInfo()['is_logged_in']:
    credentials = test.GetPrivateInfo()[test_account]
    test.Login(credentials['username'], credentials['password'])
    login_info = test.GetLoginInfo()
    test.assertTrue(login_info['is_logged_in'], msg='Login failed.')
  else:
    test.fail(msg='Another user is already logged in. Please logout first.')

def GetInfobarIndexByType(test, infobar_type, windex=0, tab_index=0):
  """Returns the index of the infobar of the given type.

  Args:
    test: Derived from pyauto.PyUITest - base class for UI test cases.
    infobar_type: The infobar type to look for.
    windex: Window index.
    tab_index: Tab index.

  Returns:
    Index of infobar for infobar type, or None if not found.
  """
  infobar_list = (
      test.GetBrowserInfo()['windows'][windex]['tabs'][tab_index] \
          ['infobars'])
  for infobar in infobar_list:
    if infobar_type == infobar['type']:
      return infobar_list.index(infobar)
  return None

def WaitForInfobarTypeAndGetIndex(test, infobar_type, windex=0, tab_index=0):
  """Wait for infobar type to appear and returns its index.

  If the infobar never appears, an exception will be raised.

  Args:
    test: Derived from pyauto.PyUITest - base class for UI test cases.
    infobar_type: The infobar type to look for.
    windex: Window index. Defaults to 0 (first window).
    tab_index: Tab index. Defaults to 0 (first tab).

  Returns:
    Index of infobar for infobar type.
  """
  test.assertTrue(
      test.WaitUntil(lambda: GetInfobarIndexByType(
          test, infobar_type, windex, tab_index) is not None),
      msg='Infobar type for %s did not appear.' % infobar_type)
  # Return the infobar index.
  return GetInfobarIndexByType(test, infobar_type, windex, tab_index)

def AssertInfobarTypeDoesNotAppear(test, infobar_type, windex=0, tab_index=0):
  """Check that the infobar type does not appear.

  This function waits 20s to assert that the infobar does not appear.

  Args:
    test: Derived from pyauto.PyUITest - base class for UI test cases.
    infobar_type: The infobar type to look for.
    windex: Window index. Defaults to 0 (first window).
    tab_index: Tab index. Defaults to 0 (first tab).
  """
  test.assertFalse(
      test.WaitUntil(lambda: GetInfobarIndexByType(
          test, infobar_type, windex, tab_index) is not None, timeout=20),
      msg=('Infobar type for %s appeared when it should be hidden.'
           % infobar_type))
