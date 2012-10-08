# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Provides an interface for installing Chrome.

At present the only platform it supports is Windows.
"""

import _winreg
import ctypes
from ctypes import wintypes, windll
import httplib
import logging
import os
import shutil
import socket
import subprocess
import tempfile
import urllib


class InstallationType(object):
  """Defines the Chrome installation types."""
  SYSTEM = 1
  USER = 2


def IsVersionNewer(cur_version, new_version):
  """Determines if new Chrome version is higher than the installed one.

  Args:
    cur_version: Current version of Chrome.
    new_version: New version that will be installed.

  Returns:
    True, if new version is higher, otherwise False.
  """
  if cur_version == new_version:
    return False
  cur = cur_version.split('.')
  new = new_version.split('.')
  if len(cur) != 4 or len(new) != 4:
    raise RuntimeError('One or both of the versions are invalid.')
  for x in range(len(cur)):
    if int(cur[x]) > int(new[x]):
      return False
  return True


def Install(installer_path, install_type, build, options):
  """Installs the specified Chrome version.

  Args:
    installer_path: Path to the Chrome installer.
    install_type: Type of installation (i.e., system or user).
    build: Chrome build number.
    options: Additional installation options.

  Returns:
    An instance of ChromeInstallation.
  """
  current = ChromeInstallation.GetCurrent()
  if current:
    # Make sure new build can be installed over existing Chrome build.
    if (current.GetType() == InstallationType.SYSTEM and
        install_type == InstallationType.USER):
      raise RuntimeError('System level Chrome exists, aborting user level '
                         'installation.')
    # Confirm the new Chrome build is higher than the installed build.
    if not IsVersionNewer(current.GetVersion(), build):
      raise RuntimeError('Installation failed because a newer version is '
                         'already installed.')
  options.append('--install')
  options.append('--do-not-launch-chrome')
  logging.info('Installing Chrome build %s...' % build)
  args = [installer_path]
  args.extend(options)
  if subprocess.Popen(args).wait() != 0:
    raise RuntimeError('Chrome installation for build %s failed.' % build)
  logging.info('Installation complete.')
  return ChromeInstallation.GetCurrent()


class ChromeRegistryValues(object):
  """Defines the Chrome registry key values."""
  PRODUCT_VERSION = 'pv'
  UNINSTALL_STRING = 'UninstallString'
  UNINSTALL_ARGUMENTS = 'UninstallArguments'


class ChromeRegistryKeys(object):
  """An interface for accessing and manipulating Chrome registry keys."""

  _HKEY_LOCAL = r'SOFTWARE\Wow6432Node\Google\Update'
  _HKEY_USER = _HKEY_LOCAL.replace(r'\Wow6432Node', '')
  _chrome_version = r'Clients\{8A69D345-D564-463C-AFF1-A69D9E530F96}'
  _chrome_args = r'ClientState\{8A69D345-D564-463C-AFF1-A69D9E530F96}'

  def _GetKeyName(self, install_type, value):
    """Gets the registry key name for the specified value.

    Args:
      install_type: Type of installation, must be InstallationType type.
      value: ChromeRegistryValues type for which the key name is required.

    Returns:
      A string representing the full key name of the specified key value.
    """
    key_name = None
    if install_type == InstallationType.USER:
      key_name = self._HKEY_USER
    elif install_type == InstallationType.SYSTEM:
      key_name = self._HKEY_LOCAL
    if value == ChromeRegistryValues.PRODUCT_VERSION:
      return r'%s\%s' % (key_name, self._chrome_version)
    elif value == ChromeRegistryValues.UNINSTALL_ARGUMENTS:
      return r'%s\%s' % (key_name, self._chrome_args)
    elif value == ChromeRegistryValues.UNINSTALL_STRING:
      return r'%s\%s' % (key_name, self._chrome_args)
    raise RuntimeError('Invalid registry value.')

  def _GetRegistryType(self, install_type):
    """Determines the registry key to use based on installation type.

    Args:
      install_type: Type of installation, must be InstallationType type.

    Returns:
      A long representing HKLM or HKCU, depending on installation type.
    """
    if install_type == InstallationType.SYSTEM:
      return _winreg.HKEY_LOCAL_MACHINE
    elif install_type == InstallationType.USER:
      return _winreg.HKEY_CURRENT_USER
    raise RuntimeError('Invalid installation type.')

  def DoesKeyExist(self, install_type, subkey):
    """Determines if a particular key exists in the registry.

    Args:
      install_type: Type of installation, must be InstallationType type.
      subkey: Subkey to look up. It must be a ChromeRegistryValues type.

    Returns:
      True if the key exists, otherwise False.
    """
    key = self._GetRegistryType(install_type)
    key_name = self._GetKeyName(install_type, subkey)
    try:
      hkey = _winreg.OpenKey(key, key_name)
    except _winreg.error:
      return False
    if not hkey.handle:
      return False
    hkey.Close()
    return True

  def GetKeyValue(self, install_type, subkey):
    """Gets value of the specified subkey from the registry.

    Args:
      install_type: Type of installation, must be InstallationType type.
      subkey: ChromeRegistryValue type representing the value to be returned.

    Returns:
      A string representing the subkey value.
    """
    key = self._GetRegistryType(install_type)
    key_name = self._GetKeyName(install_type, subkey)
    hkey = _winreg.OpenKey(key, key_name)
    reg_value = str(_winreg.QueryValueEx(hkey, subkey)[0])
    hkey.Close()
    return reg_value

  def DeleteRegistryEntries(self, install_type):
    """Deletes chrome registry settings.

    Args:
      install_type: Type of installation, must be InstallationType type.
    """
    reg_type = self._GetRegistryType(install_type)
    key_name = self._GetKeyName(install_type,
                                ChromeRegistryValues.UNINSTALL_ARGUMENTS)
    root = key_name[:key_name.rfind('\\')]
    child = key_name[key_name.rfind('\\') + 1:]
    key = _winreg.OpenKey(reg_type, root, 0, _winreg.KEY_ALL_ACCESS)
    _winreg.DeleteKey(key, child)
    key.Close()


class ChromeInstallation(object):
  """Provides pertinent information about the installed Chrome version.

  The type of Chrome version must be passed as an argument to the constructor,
  (i.e. - user or system level).
  """

  _CSIDL_COMMON_APPDATA = 0x1C
  _CSIDL_PROGRAM_FILESX86 = 0x2A

  def __init__(self, install_type):
    assert(install_type == InstallationType.SYSTEM or
           install_type == InstallationType.USER)
    self._type = install_type
    self._regedit = ChromeRegistryKeys()

  def GetType(self):
    """Returns the current installation type."""
    return self._type

  def GetVersion(self):
    """Returns the installed version of Chrome."""
    return self._regedit.GetKeyValue(self._type,
                                     ChromeRegistryValues.PRODUCT_VERSION)

  def _GetWinLocalFolder(self, ftype=_CSIDL_COMMON_APPDATA):
    """Returns full path of the 'Local' folder on Windows.

    Args:
      ftype: Location to look up, which could vary based on installation type.

    Returns:
      A String representing the folder path if successful, otherwise an empty
      string.
    """
    SHGetFolderPathW = windll.shell32.SHGetFolderPathW
    SHGetFolderPathW.argtypes = [wintypes.HWND,
                                 ctypes.c_int,
                                 wintypes.HANDLE,
                                 wintypes.DWORD,
                                 wintypes.LPCWSTR]
    path_buf = wintypes.create_unicode_buffer(wintypes.MAX_PATH)
    result = SHGetFolderPathW(0, ftype, 0, 0, path_buf)
    return str(path_buf.value)

  def _GetUninstallString(self):
    """Returns the Chrome uninstall string from the registry."""
    return self._regedit.GetKeyValue(self._type,
                                     ChromeRegistryValues.UNINSTALL_STRING)

  def _GetUninstallArguments(self):
    """Returns the Chrome uninstall arguments from the registry."""
    return self._regedit.GetKeyValue(self._type,
                                     ChromeRegistryValues.UNINSTALL_ARGUMENTS)

  def GetExePath(self):
    """Returns Chrome binary location based on installation type.

    Currently this method only returns the location of the Chrome binary.
    It does not support Chromium.
    """
    if self._type == InstallationType.USER:
      folder_id = self._CSIDL_COMMON_APPDATA
    elif self._type == InstallationType.SYSTEM:
      folder_id = self._CSIDL_PROGRAM_FILESX86
    chrome_path = os.path.join(self._GetWinLocalFolder(folder_id), 'Google',
                               'Chrome', 'Application', 'chrome.exe')
    return (chrome_path if os.path.exists(chrome_path) else '')

  @staticmethod
  def GetCurrent():
    """Determines Chrome installation type.

    Returns:
      ChromeInstallation object if Chrome is present, otherwise None.
    """
    registry = ChromeRegistryKeys()
    if registry.DoesKeyExist(InstallationType.SYSTEM,
                             ChromeRegistryValues.PRODUCT_VERSION):
      return ChromeInstallation(InstallationType.SYSTEM)
    elif registry.DoesKeyExist(InstallationType.USER,
                               ChromeRegistryValues.PRODUCT_VERSION):
      return ChromeInstallation(InstallationType.USER)
    return None

  def Uninstall(self):
    """Uninstalls Chrome."""
    chrome_path = self.GetExePath()
    reg_opts = self._GetUninstallArguments()
    uninstall_str = self._GetUninstallString()
    options = '%s --force-uninstall' % (reg_opts)
    if self._type == InstallationType.SYSTEM:
      options += ' --system-level'
    if not os.path.exists(chrome_path):
      raise RuntimeError('Could not find chrome, aborting uninstall.')
    logging.info('Launching Chrome installer...')
    cmd = '"%s" %s' % (uninstall_str, options)
    subprocess.call(cmd)
    if not os.path.exists(chrome_path):
      logging.info('Chrome was uninstalled successfully...')
      logging.info('Deleting registry entries...')
      self._regedit.DeleteRegistryEntries(self._type)
      logging.info('Uninstall complete.')
    else:
      raise RuntimeError('Uninstall failed.')
