#!/usr/bin/python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Install the NSIS compiler and its SDK."""

import os
import shutil
import subprocess
import zipfile


# The name of the archive that contains the AccessControl extensions.
ACCESS_CONTROL_ZIP = 'AccessControl.zip'
# The AccessControl plugin.  The installer check for this before installing.
ACCESS_CONTROL_DLL = 'AccessControl.dll'
# The name of the MkLnk extension DLL.  This is checked into the SDK repo.
MKLINK_DLL = os.path.join('MkLink', 'Release Unicode', 'MkLink.dll')
# The NSIS compiler.  The installer checks for this before installing.
NSIS_COMPILER = 'makensis.exe'
# The default directory name for the NSIS installation.
NSIS_DIR = 'NSIS'
# The name of the NSIS installer.  This file is checked into the SDK repo.
NSIS_INSTALLER = 'nsis-2.46-Unicode-setup.exe'

def MakeDirsIgnoreExist(dir_path, mode=0755):
  '''Recursively make a directory path.

  Recursively makes all the paths in |dir_path|.  If |dir_path| already exists,
  do nothing.

  Args:
    dir_path: A directory path.  All necessary comonents of this path are
        created.
    mode: The mode to use if creating |dir_path|.  Default is 0755.
  '''
  if not os.path.exists(dir_path):
    os.makedirs(dir_path, mode=mode)


def InstallNsis(installer_exe, target_dir, force=False):
  '''Install NSIS into |target_dir|.

  Args:
    installer_exe: The full path to the NSIS self-extracting installer.

    target_dir: The target directory for NSIS.  The installer is run in this
        directory and produces a directory named NSIS that contains the NSIS
        compiler, etc. Must be defined.

    force: Whether or not to force an installation.
  '''
  if not os.path.exists(installer_exe):
    raise IOError('%s not found' % installer_exe)

  if not os.path.isabs(installer_exe):
    raise ValueError('%s must be an absolute path' % installer_exe)
  if not os.path.isabs(target_dir):
    raise ValueError('%s must be an absolute path' % target_dir)

  if force or not os.path.exists(os.path.join(target_dir, NSIS_COMPILER)):
    MakeDirsIgnoreExist(target_dir)
    subprocess.check_call([installer_exe,
                           '/S',
                           '/D=%s' % target_dir],
                          cwd=os.path.dirname(installer_exe),
                          shell=True)


def InstallAccessControlExtensions(cwd,
                                   access_control_zip,
                                   target_dir,
                                   force=False):
  '''Install the AccessControl extensions into the NSIS directory.

  Args:
    cwd: The current working directory.

    access_control_zip: The full path of the AccessControl.zip file.  The
        contents of this file are extracted using python's zipfile package.

    target_dir: The full path of the target directory for the AccessControl
        extensions.

    force: Whether or not to force an installation.
  '''
  if not os.path.exists(access_control_zip):
    raise IOError('%s not found' % access_control_zip)

  dst_plugin_dir = os.path.join(target_dir, 'Plugins')
  if force or not os.path.exists(os.path.join(dst_plugin_dir,
                                              ACCESS_CONTROL_DLL)):
    MakeDirsIgnoreExist(dst_plugin_dir)
    zip_file = zipfile.ZipFile(access_control_zip, 'r')
    try:
      zip_file.extractall(target_dir)
    finally:
      zip_file.close()
    # Move the AccessControl plugin DLLs into the main NSIS Plugins directory.
    access_control_plugin_dir = os.path.join(target_dir,
                                             'AccessControl',
                                             'Plugins')
    access_control_plugins = [os.path.join(access_control_plugin_dir, p)
        for p in os.listdir(access_control_plugin_dir)]
    for plugin in access_control_plugins:
      shutil.copy(plugin, dst_plugin_dir)


def InstallMkLinkExtensions(mklink_dll, target_dir, force=False):
  '''Install the AccessControl extensions into the NSIS directory.

  Args:
    mklink_dll: The full path of the MkLink.dll file.

    target_dir: The full path of the target directory for the MkLink
        extensions.

    force: Whether or not to force an installation.
  '''
  if not os.path.exists(mklink_dll):
    raise IOError('%s not found' % mklink_dll)

  dst_plugin_dir = os.path.join(target_dir, 'Plugins')
  dst_mklink_dll = os.path.join(dst_plugin_dir, os.path.basename(mklink_dll))
  if force or not os.path.exists(dst_mklink_dll):
    MakeDirsIgnoreExist(dst_plugin_dir)
    # Copy the MkLink plugin DLLs into the main NSIS Plugins directory.
    shutil.copy(mklink_dll, dst_mklink_dll)


def Install(cwd, target_dir=None, force=False):
  '''Install the entire NSIS compiler and SDK with extensions.

  Installs the NSIS SDK and extensions into |target_dir|.  By default, the
  target directory is NSIS_DIR under |cwd|.

  If NSIS is already installed and |force| is False, do nothing.  If |force|
  is True or NSIS is not already installed, then install NSIS and the necessary
  extensions.

  Args:
    cwd: The current working directory.

    target_dir: NSIS is installed here.  If |target_dir| is None, then NSIS is
        installed in its default location, which is NSIS_DIR under |cwd|.

    force: True means install NSIS whether it already exists or not.
  '''
  # If the NSIS compiler and SDK hasn't been installed, do so now.
  nsis_dir = target_dir or os.path.join(cwd, NSIS_DIR)
  InstallNsis(os.path.join(cwd, NSIS_INSTALLER), nsis_dir, force=force)
  InstallMkLinkExtensions(os.path.join(cwd, MKLINK_DLL), nsis_dir, force=force)
  InstallAccessControlExtensions(
      cwd, os.path.join(cwd, ACCESS_CONTROL_ZIP), nsis_dir, force=force)
