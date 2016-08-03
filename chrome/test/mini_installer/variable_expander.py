# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64
import hashlib
import os
import string
import win32api
import win32com.client
from win32com.shell import shell, shellcon
import win32security


def _GetFileVersion(file_path):
  """Returns the file version of the given file."""
  return win32com.client.Dispatch('Scripting.FileSystemObject').GetFileVersion(
      file_path)


def _GetProductName(file_path):
  """Returns the product name of the given file.

  Args:
    file_path: The absolute or relative path to the file.

  Returns:
    A string representing the product name of the file, or None if the product
    name was not found.
  """
  language_and_codepage_pairs = win32api.GetFileVersionInfo(
      file_path, '\\VarFileInfo\\Translation')
  if not language_and_codepage_pairs:
    return None
  product_name_entry = ('\\StringFileInfo\\%04x%04x\\ProductName' %
                        language_and_codepage_pairs[0])
  return win32api.GetFileVersionInfo(file_path, product_name_entry)


def _GetUserSpecificRegistrySuffix():
  """Returns '.' plus the unpadded Base32 encoding of the MD5 of the user's SID.

  The result must match the output from the method
  UserSpecificRegistrySuffix::GetSuffix() in
  chrome/installer/util/shell_util.cc. It will always be 27 characters long.
  """
  token_handle = win32security.OpenProcessToken(win32api.GetCurrentProcess(),
                                                win32security.TOKEN_QUERY)
  user_sid, _ = win32security.GetTokenInformation(token_handle,
                                                  win32security.TokenUser)
  user_sid_string = win32security.ConvertSidToStringSid(user_sid)
  md5_digest = hashlib.md5(user_sid_string).digest()
  return '.' + base64.b32encode(md5_digest).rstrip('=')


class VariableExpander:
  """Expands variables in strings."""

  def __init__(self, mini_installer_path, next_version_mini_installer_path):
    """Constructor.

    The constructor initializes a variable dictionary that maps variables to
    their values. These are the only acceptable variables:
        * $CHROME_DIR: the directory of Chrome (or Chromium) from the base
            installation directory.
        * $CHROME_HTML_PROG_ID: 'ChromeHTML' (or 'ChromiumHTM').
        * $CHROME_LONG_NAME: 'Google Chrome' (or 'Chromium').
        * $CHROME_LONG_NAME_SXS: 'Google Chrome SxS' if $SUPPORTS_SXS.
        * $CHROME_SHORT_NAME: 'Chrome' (or 'Chromium').
        * $CHROME_SHORT_NAME_SXS: 'ChromeCanary' if $SUPPORTS_SXS.
        * $CHROME_UPDATE_REGISTRY_SUBKEY: the registry key, excluding the root
            key, of Chrome for Google Update.
        * $CHROME_UPDATE_REGISTRY_SUBKEY_SXS: the registry key, excluding the
            root key, of Chrome SxS for Google Update.
        * $LAUNCHER_UPDATE_REGISTRY_SUBKEY: the registry key, excluding the root
            key, of the app launcher for Google Update if $SUPPORTS_SXS.
        * $LOCAL_APPDATA: the unquoted path to the Local Application Data
            folder.
        * $MINI_INSTALLER: the unquoted path to the mini_installer.
        * $MINI_INSTALLER_FILE_VERSION: the file version of $MINI_INSTALLER.
        * $NEXT_VERSION_MINI_INSTALLER: the unquoted path to a mini_installer
            whose version is higher than $MINI_INSTALLER.
        * $NEXT_VERSION_MINI_INSTALLER_FILE_VERSION: the file version of
            $NEXT_VERSION_MINI_INSTALLER.
        * $PROGRAM_FILES: the unquoted path to the Program Files folder.
        * $SUPPORTS_SXS: a boolean indicating whether or not SxS installs
            are supported by the mini_installer under test.
        * $USER_SPECIFIC_REGISTRY_SUFFIX: the output from the function
            _GetUserSpecificRegistrySuffix().
        * $VERSION_[XP/SERVER_2003/VISTA/WIN7/WIN8/WIN8_1/WIN10]: a 2-tuple
            representing the version of the corresponding OS.
        * $WINDOWS_VERSION: a 2-tuple representing the current Windows version.

    Args:
      mini_installer_path: The path to a mini_installer.
      next_version_mini_installer_path: The path to a mini_installer whose
          version is higher than |mini_installer_path|.
    """
    mini_installer_abspath = os.path.abspath(mini_installer_path)
    next_version_mini_installer_abspath = os.path.abspath(
        next_version_mini_installer_path)
    windows_major_ver, windows_minor_ver, _, _, _ = win32api.GetVersionEx()
    self._variable_mapping = {
        'LOCAL_APPDATA': shell.SHGetFolderPath(0, shellcon.CSIDL_LOCAL_APPDATA,
                                               None, 0),
        'MINI_INSTALLER': mini_installer_abspath,
        'MINI_INSTALLER_FILE_VERSION': _GetFileVersion(mini_installer_abspath),
        'NEXT_VERSION_MINI_INSTALLER': next_version_mini_installer_abspath,
        'NEXT_VERSION_MINI_INSTALLER_FILE_VERSION': _GetFileVersion(
            next_version_mini_installer_abspath),
        'PROGRAM_FILES': shell.SHGetFolderPath(0, shellcon.CSIDL_PROGRAM_FILES,
                                               None, 0),
        'USER_SPECIFIC_REGISTRY_SUFFIX': _GetUserSpecificRegistrySuffix(),
        'VERSION_SERVER_2003': '(5, 2)',
        'VERSION_VISTA': '(6, 0)',
        'VERSION_WIN10': '(10, 0)',
        'VERSION_WIN7': '(6, 1)',
        'VERSION_WIN8': '(6, 2)',
        'VERSION_WIN8_1': '(6, 3)',
        'VERSION_XP': '(5, 1)',
        'WINDOWS_VERSION': '(%s, %s)' % (windows_major_ver, windows_minor_ver)
    }

    mini_installer_product_name = _GetProductName(mini_installer_abspath)
    if mini_installer_product_name == 'Google Chrome Installer':
      self._variable_mapping.update({
          'BINARIES_UPDATE_REGISTRY_SUBKEY': (
            'Software\\Google\\Update\\Clients\\'
            '{4DC8B4CA-1BDA-483e-B5FA-D3C12E15B62D}'),
          'CHROME_DIR': 'Google\\Chrome',
          'CHROME_HTML_PROG_ID': 'ChromeHTML',
          'CHROME_LONG_NAME': 'Google Chrome',
          'CHROME_SHORT_NAME': 'Chrome',
          'CHROME_UPDATE_REGISTRY_SUBKEY': (
            'Software\\Google\\Update\\Clients\\'
            '{8A69D345-D564-463c-AFF1-A69D9E530F96}'),
          'CHROME_CLIENT_STATE_KEY': (
            'Software\\Google\\Update\\ClientState\\'
            '{8A69D345-D564-463c-AFF1-A69D9E530F96}'),
          'SUPPORTS_SXS': True,
          'CHROME_DIR_SXS': 'Google\\Chrome SxS',
          'CHROME_LONG_NAME_SXS': 'Google Chrome SxS',
          'CHROME_SHORT_NAME_SXS': 'ChromeCanary',
          'CHROME_UPDATE_REGISTRY_SUBKEY_SXS': (
            'Software\\Google\\Update\\Clients\\'
            '{4ea16ac7-fd5a-47c3-875b-dbf4a2008c20}'),
          'LAUNCHER_UPDATE_REGISTRY_SUBKEY': (
            'Software\\Google\\Update\\Clients\\'
            '{FDA71E6F-AC4C-4a00-8B70-9958A68906BF}')
      })
    elif mini_installer_product_name == 'Chromium Installer':
      self._variable_mapping.update({
          'BINARIES_UPDATE_REGISTRY_SUBKEY': 'Software\\Chromium Binaries',
          'CHROME_DIR': 'Chromium',
          'CHROME_HTML_PROG_ID': 'ChromiumHTM',
          'CHROME_LONG_NAME': 'Chromium',
          'CHROME_SHORT_NAME': 'Chromium',
          'CHROME_UPDATE_REGISTRY_SUBKEY': 'Software\\Chromium',
          'CHROME_CLIENT_STATE_KEY': 'Software\\Chromium',
          'SUPPORTS_SXS': False
      })
    else:
      raise KeyError("Unknown mini_installer product name '%s'" %
                     mini_installer_product_name)


  def Expand(self, str):
    """Expands variables in the given string.

    This method resolves only variables defined in the constructor. It does not
    resolve environment variables. Any dollar signs that are not part of
    variables must be escaped with $$, otherwise a KeyError or a ValueError will
    be raised.

    Args:
      str: A string.

    Returns:
      A new string created by replacing variables with their values.
    """
    return string.Template(str).substitute(self._variable_mapping)
