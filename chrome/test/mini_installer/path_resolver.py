# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import string
import win32api
import win32com.client
from win32com.shell import shell, shellcon


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


class PathResolver:
  """Resolves variables in file paths and registry keys."""

  def __init__(self, mini_installer_path):
    """Constructor.

    The constructor initializes a variable dictionary that maps variables to
    their values. These are the only acceptable variables:
        * $PROGRAM_FILE: the unquoted path to the Program Files folder.
        * $LOCAL_APPDATA: the unquoted path to the Local Application Data
            folder.
        * $MINI_INSTALLER: the unquoted path to the mini_installer.
        * $MINI_INSTALLER_FILE_VERSION: the file version of the mini_installer.
        * $CHROME_SHORT_NAME: 'Chrome' (or 'Chromium').
        * $CHROME_LONG_NAME: 'Google Chrome' (or 'Chromium').
        * $CHROME_DIR: the directory of Chrome (or Chromium) from the base
            installation directory.
        * $CHROME_UPDATE_REGISTRY_SUBKEY: the registry key, excluding the root
            key, of Chrome for Google Update.

    Args:
      mini_installer_path: The path to mini_installer.exe.
    """
    mini_installer_abspath = os.path.abspath(mini_installer_path)
    mini_installer_file_version = _GetFileVersion(mini_installer_abspath)
    mini_installer_product_name = _GetProductName(mini_installer_abspath)
    program_files_path = shell.SHGetFolderPath(0, shellcon.CSIDL_PROGRAM_FILES,
                                               None, 0)
    local_appdata_path = shell.SHGetFolderPath(0, shellcon.CSIDL_LOCAL_APPDATA,
                                               None, 0)
    if mini_installer_product_name == 'Google Chrome':
      chrome_short_name = 'Chrome'
      chrome_long_name = 'Google Chrome'
      chrome_dir = 'Google\\Chrome'
      chrome_update_registry_subkey = ('Software\\Google\\Update\\Clients\\'
                                       '{8A69D345-D564-463c-AFF1-A69D9E530F96}')
    elif mini_installer_product_name == 'Chromium':
      chrome_short_name = 'Chromium'
      chrome_long_name = 'Chromium'
      chrome_dir = 'Chromium'
      chrome_update_registry_subkey = 'Software\\Chromium'
    else:
      raise KeyError("Unknown mini_installer product name '%s'" %
                     mini_installer_product_name)

    self._variable_mapping = {
        'PROGRAM_FILES': program_files_path,
        'LOCAL_APPDATA': local_appdata_path,
        'MINI_INSTALLER': mini_installer_abspath,
        'MINI_INSTALLER_FILE_VERSION': mini_installer_file_version,
        'CHROME_SHORT_NAME': chrome_short_name,
        'CHROME_LONG_NAME': chrome_long_name,
        'CHROME_DIR': chrome_dir,
        'CHROME_UPDATE_REGISTRY_SUBKEY': chrome_update_registry_subkey,
    }

  def ResolvePath(self, path):
    """Resolves variables in a file path, and returns the resolved path.

    This method resolves only variables defined in the constructor. It does not
    resolve environment variables. Any dollar signs that are not part of
    variables must be escaped with $$, otherwise a KeyError or a ValueError will
    be raised.

    Args:
      path: An absolute or relative path.

    Returns:
      A new path created by replacing variables with their values.
    """
    return string.Template(path).substitute(self._variable_mapping)
