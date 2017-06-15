#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A tool for working with state associated with the M60 Chrome on Windows 10
retention experiment.

For example:
experiment_tool_win.py --channel beta --system-level --operation prep
"""

import argparse
import math
import sys
from datetime import datetime, timedelta
import win32api
import win32security
import _winreg


def GetUserSidString():
  """Returns the current user's SID string."""
  token_handle = win32security.OpenProcessToken(win32api.GetCurrentProcess(),
                                                win32security.TOKEN_QUERY)
  user_sid, _ = win32security.GetTokenInformation(token_handle,
                                                  win32security.TokenUser)
  return win32security.ConvertSidToStringSid(user_sid)


def InternalTimeFromPyTime(pytime):
  """Returns a Chromium internal time value representing a Python datetime."""
  # Microseconds since 1601-01-01 00:00:00 UTC
  delta = pytime - datetime(1601, 1, 1)
  return math.trunc(delta.total_seconds()) * 1000000 + delta.microseconds


class ChromeState:
  """An object that provides mutations on Chrome's state relating to the
  user experiment.
  """
  _CHANNEL_CONFIGS = {
    'stable': {
      'guid': '{8A69D345-D564-463c-AFF1-A69D9E530F96}'
    },
    'beta': {
      'guid': '{8237E44A-0054-442C-B6B6-EA0509993955}'
    },
    'dev': {
      'guid': '{401C381F-E0DE-4B85-8BD8-3F3F14FBDA57}'
    },
    'canary': {
      'guid': '{4ea16ac7-fd5a-47c3-875b-dbf4a2008c20}'
    },
   }
  _GOOGLE_UPDATE_PATH = 'Software\\Google\\Update'
  _ACTIVE_SETUP_PATH = 'Software\\Microsoft\\Active Setup\\Installed ' + \
      'Components\\'

  def __init__(self, channel_name, system_level):
    self._config = ChromeState._CHANNEL_CONFIGS[channel_name]
    self._system_level = system_level
    self._registry_root = _winreg.HKEY_LOCAL_MACHINE if self._system_level \
        else _winreg.HKEY_CURRENT_USER

  def SetRetentionStudyValue(self, study):
    """Sets the RetentionStudy value for the install."""
    path = ChromeState._GOOGLE_UPDATE_PATH + '\\ClientState\\' + \
        self._config['guid']
    with _winreg.OpenKey(self._registry_root, path, 0,
                         _winreg.KEY_WOW64_32KEY |
                         _winreg.KEY_SET_VALUE) as key:
      _winreg.SetValueEx(key, 'RetentionStudy', 0, _winreg.REG_DWORD, study)

  def DeleteRetentionStudyValue(self):
    """Deletes the RetentionStudy value for the install."""
    path = ChromeState._GOOGLE_UPDATE_PATH + '\\ClientState\\' + \
        self._config['guid']
    try:
      with _winreg.OpenKey(self._registry_root, path, 0,
                           _winreg.KEY_WOW64_32KEY |
                           _winreg.KEY_SET_VALUE) as key:
        _winreg.DeleteValue(key, 'RetentionStudy')
    except WindowsError as error:
      if error.winerror != 2:
        raise

  def DeleteExperimentLabelsValue(self):
    """Deletes the experiment_labels for the install."""
    medium = 'Medium' if self._system_level else ''
    path = ChromeState._GOOGLE_UPDATE_PATH + '\\ClientState' + medium + '\\' + \
        self._config['guid']
    try:
      with _winreg.OpenKey(self._registry_root, path, 0,
                           _winreg.KEY_WOW64_32KEY |
                           _winreg.KEY_SET_VALUE) as key:
        _winreg.DeleteValue(key, 'experiment_labels')
    except WindowsError as error:
      if error.winerror != 2:
        raise

  def DeleteRentionKey(self):
    """Deletes the Retention key for the current user."""
    medium = 'Medium' if self._system_level else ''
    path = ChromeState._GOOGLE_UPDATE_PATH + '\\ClientState' + medium + '\\' + \
        self._config['guid'] + '\\Retention'
    try:
      if self._system_level:
        _winreg.DeleteKeyEx(self._registry_root,
                            path + '\\' + GetUserSidString(),
                            _winreg.KEY_WOW64_32KEY)
      _winreg.DeleteKeyEx(self._registry_root, path, _winreg.KEY_WOW64_32KEY)
    except WindowsError as error:
      if error.winerror != 2:
        raise

  def SetLastRunTime(self, delta):
    """Sets Chrome's lastrun time for the current user."""
    path = ChromeState._GOOGLE_UPDATE_PATH + '\\ClientState\\' + \
        self._config['guid']
    lastrun = InternalTimeFromPyTime(datetime.utcnow() - delta)
    with _winreg.CreateKeyEx(_winreg.HKEY_CURRENT_USER, path, 0,
                             _winreg.KEY_WOW64_32KEY |
                             _winreg.KEY_SET_VALUE) as key:
      _winreg.SetValueEx(key, 'lastrun', 0, _winreg.REG_SZ, str(lastrun))

  def AdjustActiveSetupCommand(self):
    """Adds --experiment-enterprise-bypass to system-level Chrome's Active Setup
    command."""
    if not self._system_level:
      return
    enable_switch = '--experiment-enable-for-testing'
    bypass_switch = '--experiment-enterprise-bypass'
    for flag in [_winreg.KEY_WOW64_32KEY, _winreg.KEY_WOW64_64KEY]:
      try:
        with _winreg.OpenKey(self._registry_root,
                             ChromeState._ACTIVE_SETUP_PATH +
                             self._config['guid'], 0,
                             _winreg.KEY_SET_VALUE | _winreg.KEY_QUERY_VALUE |
                             flag) as key:
          command, _ = _winreg.QueryValueEx(key, 'StubPath')
          if not bypass_switch in command:
            command += ' ' + bypass_switch
          if not enable_switch in command:
            command += ' ' + enable_switch
          _winreg.SetValueEx(key, 'StubPath', 0, _winreg.REG_SZ, command)
      except WindowsError as error:
        if error.winerror != 2:
          raise

  def ClearUserActiveSetup(self):
    """Clears per-user state associated with Active Setup so that it will run
    again on next login."""
    if not self._system_level:
      return
    paths = [ChromeState._ACTIVE_SETUP_PATH,
             ChromeState._ACTIVE_SETUP_PATH.replace('Software\\',
                                                    'Software\\Wow6432Node\\')]
    for path in paths:
      try:
        _winreg.DeleteKeyEx(_winreg.HKEY_CURRENT_USER,
                            path + self._config['guid'], 0)
      except WindowsError as error:
        if error.winerror != 2:
          raise


def DoClean(chrome_state):
  """Deletes all state associated with the user experiment."""
  chrome_state.DeleteRetentionStudyValue()
  chrome_state.DeleteExperimentLabelsValue()
  chrome_state.DeleteRentionKey()
  return 0


def DoPrep(chrome_state):
  """Prepares an install for participation in the experiment."""
  # Clear old state.
  DoClean(chrome_state)
  # Make Chrome appear to have been last run 30 days ago.
  chrome_state.SetLastRunTime(timedelta(30))
  # Add the enterprise bypass switch to the Active Setup command.
  chrome_state.AdjustActiveSetupCommand()
  # Cause Active Setup to be run for the current user on next logon.
  chrome_state.ClearUserActiveSetup()
  # Put the machine into the first study.
  chrome_state.SetRetentionStudyValue(1)
  return 0


def main(options):
  chrome_state = ChromeState(options.channel, options.system_level)
  if options.operation == 'clean':
    return DoClean(chrome_state)
  if options.operation == 'prep':
    return DoPrep(chrome_state)
  return 1


if __name__ == '__main__':
  parser = argparse.ArgumentParser(
    description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
  parser.add_argument('--operation', required=True, choices=['clean', 'prep'],
                      help='The operation to be performed.')
  parser.add_argument('--channel', default='stable',
                      choices=['stable', 'beta', 'dev', 'canary'],
                      help='The install on which to operate (stable by ' \
                      'default).')
  parser.add_argument('--system-level', action='store_true',
                      help='Specify to operate on a system-level install ' \
                      '(user-level by default).')
  sys.exit(main(parser.parse_args()))
