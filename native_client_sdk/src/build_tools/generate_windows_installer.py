#!/usr/bin/env python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Assemble the final installer for windows."""

import build_utils
import installer_contents
import make_nsis_installer
import optparse
import os
import shutil
import stat
import string
import subprocess
import sys
import tar_archive

IGNORE_PATTERN = ('.download*', '.svn*')

def main(argv):
  bot = build_utils.BotAnnotator()
  bot.Print('generate_windows_installer is starting.')

  # Make sure that we are running python version 2.6 or higher
  (major, minor) = sys.version_info[:2]
  assert major == 2 and minor >= 6
  # Cache the current location so we can return here before removing the
  # temporary dirs.
  script_dir = os.path.abspath(os.path.dirname(__file__))
  home_dir = os.path.realpath(os.path.dirname(os.path.dirname(script_dir)))

  version_dir = build_utils.VersionString()
  parent_dir = os.path.dirname(script_dir)
  deps_file = os.path.join(parent_dir, 'DEPS')

  # Create a temporary directory using the version string, then move the
  # contents of src to that directory, clean the directory of unwanted
  # stuff and finally create an installer.
  temp_dir = os.path.join(script_dir, 'installers_temp')
  installer_dir = os.path.join(temp_dir, version_dir)
  bot.Print('generate_windows_installer chose installer directory: %s' %
            (installer_dir))
  try:
    os.makedirs(installer_dir, mode=0777)
  except OSError:
    pass

  env = os.environ.copy()
  # Set up the required env variables for the scons builders.
  env['NACL_SDK_ROOT'] = parent_dir
  env['NACL_TARGET_PLATFORM'] = '.'  # Use the repo's toolchain.

  # Build the experimental projects.
  bot.BuildStep('build experimental')
  bot.Print('generate_windows_installer is building the experimental projects.')
  experimental_path = os.path.join(home_dir, 'src', 'experimental')
  scons_path = os.path.join(experimental_path, 'scons.bat')
  scons_cmd = scons_path + ' --nacl-platform="."'
  subprocess.check_call(scons_cmd, cwd=experimental_path, env=env)

  # On windows we use copytree to copy the SDK into the build location
  # because there is no native tar and using cygwin's version has proven
  # to be error prone.

  # In case previous run didn't succeed, clean this out so copytree can make
  # its target directories.
  bot.BuildStep('copy to install dir')
  bot.Print('generate_windows_installer is cleaning out install directory.')
  shutil.rmtree(installer_dir)
  bot.Print('generate_windows_installer: copying files to install directory.')
  all_contents = installer_contents.INSTALLER_CONTENTS + \
                 installer_contents.WINDOWS_ONLY_CONTENTS
  for copy_source_dir in installer_contents.GetDirectoriesFromPathList(
      all_contents):
    copy_target_dir = os.path.join(installer_dir, copy_source_dir)
    bot.Print("Copying %s to %s" % (copy_source_dir, copy_target_dir))
    shutil.copytree(copy_source_dir,
                    copy_target_dir,
                    symlinks=True,
                    ignore=shutil.ignore_patterns(*IGNORE_PATTERN))
  for copy_source_file in installer_contents.GetFilesFromPathList(
      all_contents):
    copy_target_file = os.path.join(installer_dir, copy_source_file)
    bot.Print("Copying %s to %s" % (copy_source_file, copy_target_file))
    if not os.path.exists(os.path.dirname(copy_target_file)):
      os.makedirs(os.path.dirname(copy_target_file))
    shutil.copy(copy_source_file, copy_target_file)

  # Do special processing on the user-readable documentation files.
  for copy_source_file in installer_contents.DOCUMENTATION_FILES:
    copy_target_file = os.path.join(installer_dir, copy_source_file + '.txt')
    bot.Print("Copying %s to %s" % (copy_source_file, copy_target_file))
    with open(copy_source_file, "U") as source_file:
      text = source_file.read().replace("\n", "\r\n")
    with open(copy_target_file, "wb") as dest_file:
      dest_file.write(text)

  # Update the README.txt file with date and version number
  build_utils.UpdateReadMe(os.path.join(installer_dir, 'README.txt'))

  # Clean out the cruft.
  bot.Print('generate_windows_installer: cleaning up installer directory.')

  # Make everything read/write (windows needs this).
  for root, dirs, files in os.walk(installer_dir):
    def UpdatePermissions(list):
      for file in map(lambda f: os.path.join(root, f), list):
        os.chmod(file, os.lstat(file).st_mode | stat.S_IWRITE | stat.S_IREAD)
    UpdatePermissions(dirs)
    UpdatePermissions(files)

  # Grab the toolchain manifest files and massage them so their paths are
  # corrected for the actual toolchain layout in the SDK.  The newlib toolchain
  # manifest has these changes made:
  #  1. Transform path components 'sdk/nacl-sdk' to 'toolchain/win_x86_newlib'
  #  2. Remove the spurious 'sdk' directory entry.
  # The glibc manifests can be used unmolested.
  bot.BuildStep('generate toolchain manifests')
  def TransformNewlibPath(npath):
    return os.path.normpath(npath.replace('sdk/nacl-sdk',
                                          'toolchain/win_x86_newlib'))
  newlib_manifest = tar_archive.TarArchive()
  newlib_manifest.path_filter = TransformNewlibPath
  newlib_manifest_path = os.path.join(
      home_dir,
      'src',
      installer_contents.GetToolchainManifest('newlib'))
  newlib_manifest.InitWithManifest(newlib_manifest_path)
  newlib_manifest.dirs.discard('sdk')

  glibc_manifest_path = os.path.join(
      home_dir,
      'src',
      installer_contents.GetToolchainManifest('glibc'))
  glibc_manifest = tar_archive.TarArchive()
  glibc_manifest.InitWithManifest(glibc_manifest_path)

  # Merge the newlib and glibc manifests and send them to the script generator.
  bot.BuildStep('create Windows installer')
  bot.Print('generate_windows_installer is creating the windows installer.')
  build_tools_dir = os.path.join(home_dir, 'src', 'build_tools')
  make_nsis_installer.MakeNsisInstaller(
      installer_dir,
      cwd=build_tools_dir,
      toolchain_manifests=newlib_manifest | glibc_manifest)
  bot.Print("Installer created!")

  # Clean up.
  shutil.rmtree(temp_dir)
  return 0


if __name__ == '__main__':
  print "Directly running generate_windows_installer.py is no longer supported."
  print "Please instead run 'scons.bat installer' from the src directory."
  sys.exit(1)
