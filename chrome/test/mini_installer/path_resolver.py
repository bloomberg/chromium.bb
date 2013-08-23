# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import string
import win32api
import win32com.client
from win32com.shell import shell, shellcon


def ResolvePath(path):
  """Resolves variables in a file path, and returns the resolved path.

  This method resolves only variables defined below. It does not resolve
  environment variables. Any dollar signs that are not part of variables must be
  escaped with $$, otherwise a KeyError or a ValueError will be raised.

  Args:
    path: An absolute or relative path.

  Returns:
    A new path created by replacing
        * $PROGRAM_FILES with the path to the Program Files folder,
        * $LOCAL_APPDATA with the path to the Local Application Data folder,
        * $MINI_INSTALLER with the path to the mini_installer,
        * $MINI_INSTALLER_FILE_VERSION with the file version of the
            mini_installer,
        * $CHROME_SHORT_NAME with 'Chrome' (or 'Chromium'),
        * $CHROME_LONG_NAME with 'Google Chrome' (or 'Chromium'),
        * $CHROME_DIR with the directory of Chrome (or Chromium) from the base
            installation directory, and
        * $CHROME_UPDATE_REGISTRY_SUBKEY with the registry key, excluding the
            root key, of Chrome for Google Update.
  """
  program_files_path = shell.SHGetFolderPath(0, shellcon.CSIDL_PROGRAM_FILES,
                                             None, 0)
  local_appdata_path = shell.SHGetFolderPath(0, shellcon.CSIDL_LOCAL_APPDATA,
                                             None, 0)
  # TODO(sukolsak): Copy the mini_installer.exe from the build output into here.
  mini_installer_path = os.path.abspath('mini_installer.exe')
  mini_installer_file_version = win32com.client.Dispatch(
      'Scripting.FileSystemObject').GetFileVersion(mini_installer_path)
  # TODO(sukolsak): Check whether this is Chrome or Chromium.
  is_chrome = True
  if is_chrome:
    chrome_short_name = 'Chrome'
    chrome_long_name = 'Google Chrome'
    chrome_dir = 'Google\\Chrome'
    chrome_update_registry_subkey = ('Software\\Google\\Update\\Clients\\'
                                     '{8A69D345-D564-463c-AFF1-A69D9E530F96}')
  else:
    chrome_short_name = 'Chromium'
    chrome_long_name = 'Chromium'
    chrome_dir = 'Chromium'
    chrome_update_registry_subkey = 'Software\\Chromium'

  variable_mapping = {
      'PROGRAM_FILES': program_files_path,
      'LOCAL_APPDATA': local_appdata_path,
      'MINI_INSTALLER': mini_installer_path,
      'MINI_INSTALLER_FILE_VERSION': mini_installer_file_version,
      'CHROME_SHORT_NAME': chrome_short_name,
      'CHROME_LONG_NAME': chrome_long_name,
      'CHROME_DIR': chrome_dir,
      'CHROME_UPDATE_REGISTRY_SUBKEY': chrome_update_registry_subkey,
  }
  return string.Template(path).substitute(variable_mapping)
