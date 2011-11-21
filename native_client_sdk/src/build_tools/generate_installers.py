#!/usr/bin/python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Assemble the final installer for each platform.

At this time this is just a tarball.
"""

import build_utils
import installer_contents
import optparse
import os
import re
import shutil
import stat
import string
import subprocess
import sys

EXCLUDE_DIRS = ['.download',
                '.svn']

INSTALLER_NAME = 'nacl-sdk.tgz'


# A list of all platforms that should use the Windows-based build strategy
# (which makes a self-extracting zip instead of a tarball).
WINDOWS_BUILD_PLATFORMS = ['cygwin', 'win32']


# Return True if |file| should be excluded from the tarball.
def ExcludeFile(dir, file):
  return (file.startswith('.DS_Store') or
          file.startswith('._') or file == "make.cmd" or
          file == 'DEPS' or file == 'codereview.settings' or
          (file == "httpd.cmd"))


def main(argv):
  bot = build_utils.BotAnnotator()
  bot.Print('generate_installers is starting.')

  # Cache the current location so we can return here before removing the
  # temporary dirs.
  script_dir = os.path.abspath(os.path.dirname(__file__))
  home_dir = os.path.realpath(os.path.dirname(os.path.dirname(script_dir)))

  version_dir = build_utils.VersionString()
  parent_dir = os.path.dirname(script_dir)
  deps_file = os.path.join(parent_dir, 'DEPS')

  # Create a temporary directory using the version string, then move the
  # contents of src to that directory, clean the directory of unwanted
  # stuff and finally tar it all up using the platform's tar.  There seems to
  # be a problem with python's tarfile module and symlinks.
  temp_dir = os.path.join(script_dir, 'installers_temp')
  installer_dir = os.path.join(temp_dir, version_dir)
  bot.Print('generate_installers chose installer directory: %s' %
            (installer_dir))
  try:
    os.makedirs(installer_dir, mode=0777)
  except OSError:
    pass

  # Decide environment to run in per platform.
  env = os.environ.copy()
  # Set up the required env variables for the scons builders.
  env['NACL_SDK_ROOT'] = parent_dir
  env['NACL_TARGET_PLATFORM'] = '.'  # Use the repo's toolchain.

  # Use native tar to copy the SDK into the build location
  # because copytree has proven to be error prone and is not supported on mac.
  # We use a buffer for speed here.  -1 causes the default OS size to be used.
  bot.BuildStep('copy to install dir')
  bot.Print('generate_installers is copying contents to install directory.')
  tar_src_dir = os.path.realpath(os.curdir)
  all_contents = (installer_contents.INSTALLER_CONTENTS +
                  installer_contents.DOCUMENTATION_FILES)
  if sys.platform == 'darwin':
    all_contents += installer_contents.MAC_ONLY_CONTENTS
  else:
    all_contents += installer_contents.LINUX_ONLY_CONTENTS

  all_contents_string = string.join(
      installer_contents.GetFilesFromPathList(all_contents) +
      installer_contents.GetDirectoriesFromPathList(all_contents),
      ' ')
  tar_cf = subprocess.Popen('tar cf - %s' % all_contents_string,
                            bufsize=-1,
                            cwd=tar_src_dir, env=env, shell=True,
                            stdout=subprocess.PIPE)
  tar_xf = subprocess.Popen('tar xfv -',
                            cwd=installer_dir, env=env, shell=True,
                            stdin=tar_cf.stdout)
  assert tar_xf.wait() == 0
  assert tar_cf.poll() == 0

  # This loop prunes the result of os.walk() at each excluded dir, so that it
  # doesn't descend into the excluded dir.
  bot.Print('generate_installers is pruning installer directory')
  for root, dirs, files in os.walk(installer_dir):
    rm_dirs = []
    for excl in EXCLUDE_DIRS:
      if excl in dirs:
        dirs.remove(excl)
        rm_dirs.append(os.path.join(root, excl))
    for rm_dir in rm_dirs:
      shutil.rmtree(rm_dir)
    rm_files = [os.path.join(root, f) for f in files if ExcludeFile(root, f)]
    for rm_file in rm_files:
      os.remove(rm_file)

  # Update the README file with date and version number
  build_utils.UpdateReadMe(os.path.join(installer_dir, 'README'))

  bot.BuildStep('create archive')
  bot.Print('generate_installers is creating the installer archive')
  # Now that the SDK directory is copied and cleaned out, tar it all up using
  # the native platform tar.

  # Set the default shell command and output name.  Create a compressed tar
  # archive from the contents of |input|.  The contents of the tar archive
  # have to be relative to |input| without including |input| in the path.
  # Then copy the resulting archive to the |output| directory and make it
  # read-only to group and others.
  ar_cmd = ('tar -cvzf %(INSTALLER_NAME)s -C %(input)s . && '
            'cp %(INSTALLER_NAME)s %(output)s && chmod 644 %(output)s')

  archive = os.path.join(home_dir, INSTALLER_NAME)
  subprocess.check_call(
      ar_cmd % (
           {'INSTALLER_NAME':INSTALLER_NAME,
            'input':installer_dir,
            'output':archive}),
      cwd=temp_dir,
      env=env,
      shell=True)

  # Clean up.
  shutil.rmtree(temp_dir)
  return 0


if __name__ == '__main__':
  print "Directly running generate_installers.py is no longer supported."
  print "Please instead run './scons installer' from the src directory."
  sys.exit(1)
